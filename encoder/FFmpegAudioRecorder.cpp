#include "FFmpegAudioRecorder.h"
#include <string>
#include <list>
#include <vector>

/*
 * ffmpeg pcm转化为aac-程序员博客中心
 * https://www.361shipin.com/blog/1505075767053193217
*/

// 判断编码器是否支持某个采样格式(采样大小)
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

// 从编码器中获取采样率(从编码器所支持的采样率中获取与44100最接近的采样率)
static int select_sample_rate(const AVCodec *codec, int default_rate)
{
    const int *p;
    int best_samplerate = 0;

    if (!codec->supported_samplerates)
        return default_rate;

    p = codec->supported_samplerates;
    while (*p) {
        if (!best_samplerate || abs(default_rate - *p) < abs(default_rate - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}

/* select layout with the highest channel count */
static uint64_t select_channel_layout(const AVCodec *codec, uint64_t default_layout)
{
    const uint64_t *p;
    uint64_t best_ch_layout = 0;
    int best_nb_channels   = 0;

    if (!codec->channel_layouts)
        return default_layout;

    p = codec->channel_layouts;
    while (*p) {
        int nb_channels = av_get_channel_layout_nb_channels(*p);

        if (nb_channels > best_nb_channels) {
            best_ch_layout    = *p;
            best_nb_channels = nb_channels;
        }
        p++;
    }
    return best_ch_layout;
}

FFmpegAudioRecorder::FFmpegAudioRecorder()
{
    ofmt_ctx = nullptr;
    out_stream = nullptr;
    enc_ctx = nullptr;

    swr_ctx = nullptr;
    audio_data = nullptr;
    audio_data_size = 0;

    audio_fifo = nullptr;
    audio_pts = 0;

    enc_samples = 0;
    log_level = AV_LOG_INFO;
}

FFmpegAudioRecorder::~FFmpegAudioRecorder()
{
    close();
}

int FFmpegAudioRecorder::flush_encoder(AVFormatContext *ofmt_ctx,int stream_index)
{
    int ret;

    if (!(enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
        return 0;

    ret = flush_audio_fifo(stream_index);

    for(int i=0; i<100; i++) {
        //av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
        ret = encode_write_frame(NULL, stream_index);
        if (ret < 0)
            break;
    }
    return ret;
}

int FFmpegAudioRecorder::flush_audio_fifo(int stream_index)
{
    int ret = 0;
    const int output_frame_size = enc_ctx->frame_size;

    while (av_audio_fifo_size(audio_fifo)>0) {
        const int frame_size = FFMIN(av_audio_fifo_size(audio_fifo), output_frame_size);

        AVFrame *output_frame;
        if (init_output_frame(&output_frame, enc_ctx, frame_size) < 0) {
            av_log(NULL, AV_LOG_ERROR, "init_output_frame failed\n");
            return -1;
        }

        if (av_audio_fifo_read(audio_fifo, (void **)output_frame->data, frame_size) < frame_size) {
            av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
            av_frame_free(&output_frame);
            return -1;
        }
        output_frame->nb_samples = frame_size;

        ret = encode_write_frame(output_frame, stream_index);
        av_frame_free(&output_frame);
    }

    return ret;
}

int FFmpegAudioRecorder::open(const char *output,
                              int64_t src_ch_layout,
                              enum AVSampleFormat src_sample_fmt,
                              int src_sample_rate)
{
    int64_t out_ch_layout = src_ch_layout;
    AVSampleFormat out_sample_fmt = src_sample_fmt;
    int out_sample_rate = src_sample_rate;

    return open(output, src_ch_layout, src_sample_fmt, src_sample_rate,
         out_ch_layout, out_sample_fmt, out_sample_rate);
}

int FFmpegAudioRecorder::open(const char *output,
                              int64_t  src_ch_layout,
                              enum AVSampleFormat src_sample_fmt,
                              int src_sample_rate,
                              int64_t  out_ch_layout,
                              enum AVSampleFormat out_sample_fmt,
                              int out_sample_rate)
{
    int ret = 0;

    if(ofmt_ctx) {
        av_log(NULL,AV_LOG_ERROR, "ofmt_ctx has been inited, please close first!\n");
        return 0;
    }

    av_log(NULL,AV_LOG_INFO, "recorder open:%s src: chly:%d fmt:%d rate:%d out: chly:%d fmt:%d rate:%d\n",
           output, src_ch_layout, src_sample_fmt, src_sample_rate,
           out_ch_layout, out_sample_fmt, out_sample_rate);
    enc_samples = 0;

    AVOutputFormat *oformat = av_guess_format(NULL,output, NULL);
    if (oformat==NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to find the output format\n");
        return -1;
    }

    av_log(NULL,AV_LOG_INFO, "recorder guess format name:%s long_name:%s mime_type:%s extensions:%s\n",
           oformat->name?oformat->name:"", oformat->long_name?oformat->long_name:"",
           oformat->mime_type?oformat->mime_type:"", oformat->extensions?oformat->extensions:"");

    AVCodec *pCodec = avcodec_find_encoder(oformat->audio_codec);
    if (pCodec == NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to find codec\n");
        return -1;
    }
    dump_codec(pCodec);

    AVSampleFormat enc_sample_fmt = AV_SAMPLE_FMT_NONE;
    int enc_sample_rate = 0;
    uint64_t enc_ch_layout = 0;

    if(check_sample_fmt(pCodec, out_sample_fmt)) {
        enc_sample_fmt = out_sample_fmt;
    } else if(pCodec->sample_fmts&&pCodec->sample_fmts[0]!=-1) {
        enc_sample_fmt = pCodec->sample_fmts[0];
    }
    enc_sample_rate = select_sample_rate(pCodec, out_sample_rate);
    enc_ch_layout = select_channel_layout(pCodec,  out_ch_layout); //AV_CH_LAYOUT_STEREO

    if(enc_sample_fmt==AV_SAMPLE_FMT_NONE) {
        av_log(NULL, AV_LOG_ERROR, "Unsupported sample format\n");
        return -1;
    }
    if(enc_sample_rate==0) {
        av_log(NULL, AV_LOG_ERROR, "Unsupported sample rate\n");
        return -1;
    }
    if(enc_ch_layout==0) {
        av_log(NULL, AV_LOG_ERROR, "Unsupported channel layout\n");
        return -1;
    }

    av_log(NULL,AV_LOG_INFO, "recorder encode:%s src: chly:%d fmt:%d rate:%d out: chly:%d fmt:%d rate:%d\n",
           output, src_ch_layout, src_sample_fmt, src_sample_rate,
           enc_ch_layout, enc_sample_fmt, enc_sample_rate);

    ofmt_ctx = avformat_alloc_context();

    if (avformat_alloc_output_context2(&ofmt_ctx,oformat,oformat->name,output) <0){
        av_log(NULL,AV_LOG_ERROR,"fail to alloc output context\n");
        return -1;
    }

    enc_ctx = avcodec_alloc_context3(pCodec);
    if (!enc_ctx) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate the encoder context\n");
        return -1;
    }

    enc_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    enc_ctx->sample_fmt = enc_sample_fmt;
    enc_ctx->channel_layout = enc_ch_layout;
    enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
    enc_ctx->sample_rate = enc_sample_rate;
    enc_ctx->bit_rate = 0;
    if(pCodec->profiles)
        enc_ctx->profile = pCodec->profiles[0].profile;

    /* Some formats want stream headers to be separate. */
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ofmt_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_log(NULL, AV_LOG_INFO, "enc_ctx info: sp:%d, ch_lyt:%d(ch:%d), fmt:%d(b:%d), br:%d, codec:%d(%s).\n",
           enc_ctx->sample_rate,
           enc_ctx->channel_layout, enc_ctx->channels,
           enc_ctx->sample_fmt, av_get_bytes_per_sample(enc_ctx->sample_fmt), enc_ctx->bit_rate,
           enc_ctx->codec_id, avcodec_get_name(enc_ctx->codec_id));

    ret = avcodec_open2(enc_ctx,pCodec,NULL);
    if (ret < 0){
        av_log(NULL,AV_LOG_ERROR,"fail to open codec\n");
        return ret;
    }
    out_stream = avformat_new_stream(ofmt_ctx,NULL);
    if (out_stream == NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to create new stream\n");
        return -1;
    }
    ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to out_stream\n");
        return ret;
    }

    av_dump_format(ofmt_ctx,0,output,1);

    if(!enc_ctx->frame_size)
        enc_ctx->frame_size = enc_ctx->sample_rate;

    //新版本需要使用到转换参数，将读取的数据转换成输出的编码格式
    audio_data = (uint8_t**)av_calloc( enc_ctx->channels,sizeof(*audio_data) );
    audio_data_size = av_samples_alloc(audio_data,NULL,enc_ctx->channels,
                                       enc_ctx->frame_size*2,enc_ctx->sample_fmt,1);

    swr_ctx  = swr_alloc();
    swr_alloc_set_opts(swr_ctx,enc_ctx->channel_layout,
                       enc_ctx->sample_fmt,enc_ctx->sample_rate,
                       src_ch_layout,src_sample_fmt,src_sample_rate,0,NULL);
//    av_log(NULL, AV_LOG_INFO, "swr o: chlt %d fmt %d rate %d, i: chlt %d fmt %d rate %d\n",
//           enc_ctx->channel_layout, enc_ctx->sample_fmt,enc_ctx->sample_rate,
//           src_ch_layout,src_sample_fmt,src_sample_rate);
    ret = swr_init(swr_ctx);
    if(ret<0) {
        av_log(NULL,AV_LOG_ERROR,"fail to swr_init\n");
        return -1;
    }

    audio_fifo = av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->channels, enc_ctx->frame_size);
    if (audio_fifo == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }

    if (avio_open(&ofmt_ctx->pb,output,AVIO_FLAG_WRITE) < 0){
        av_log(NULL,AV_LOG_ERROR,"fail to open output\n");
        return -1;
    }

    if (avformat_write_header(ofmt_ctx,NULL) < 0){
        av_log(NULL,AV_LOG_ERROR,"fail to write header");
        return -1;
    }

    audio_pts = 0;
    write_pts = 0;

    return 0;
}

int FFmpegAudioRecorder::addData(AVFrame *frame, AVPacket *packet)
{
    if(!swr_ctx) {
        av_log(NULL,AV_LOG_ERROR,"swr_ctx is null!\n");
        return -1;
    }
    if(!audio_data) {
        av_log(NULL,AV_LOG_ERROR,"audio_data is null!\n");
        return -1;
    }
    if(!enc_ctx) {
        av_log(NULL,AV_LOG_ERROR,"enc_ctx is null!\n");
        return -1;
    }
    if(!audio_fifo) {
        av_log(NULL,AV_LOG_ERROR,"audio_fifo is null!\n");
        return -1;
    }

    AVFrame* out_frame = av_frame_alloc();
    out_frame->channels = enc_ctx->channels;
    out_frame->channel_layout = enc_ctx->channel_layout;
    out_frame->format = enc_ctx->sample_fmt;
    av_frame_get_buffer(out_frame, 0);

    int out_samples = swr_convert(swr_ctx,audio_data,enc_ctx->frame_size*2,
                                  (const uint8_t **)frame->data,frame->nb_samples);

    out_frame->data[0] = audio_data[0];
    if(av_sample_fmt_is_planar(enc_ctx->sample_fmt)) {
        out_frame->data[1] = audio_data[1];
    }
    out_frame->nb_samples = out_samples;

    encode_write_frame_fifo(out_frame, out_stream->index);

    av_frame_free(&out_frame);
    return 0;
}

int FFmpegAudioRecorder::init_output_frame(AVFrame **frame,
    AVCodecContext *output_codec_context, int frame_size)
{
    int error;

    if (!(*frame = av_frame_alloc())) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate output frame\n");
        return -1;
    }

    (*frame)->nb_samples = frame_size;
    (*frame)->channel_layout = output_codec_context->channel_layout;
    (*frame)->format = output_codec_context->sample_fmt;
    (*frame)->sample_rate = output_codec_context->sample_rate;

    if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate output frame samples (error '%s')\n",
            wrap_av_err2str(error));
        av_frame_free(frame);
        return error;
    }

    return 0;
}

int FFmpegAudioRecorder::write_samples_to_fifo(AVAudioFifo *fifo,
    uint8_t **converted_input_samples,
    const int nb_samples)
{
    int ret;

    ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + nb_samples);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not reallocate FIFO\n");
        return ret;
    }

    if (av_audio_fifo_write(fifo, (void **)converted_input_samples, nb_samples) < nb_samples) {
        av_log(NULL, AV_LOG_ERROR, "Could not write data to FIFO\n");
        return -1;
    }
    return 0;
}

int FFmpegAudioRecorder::encode_write_frame_fifo(AVFrame *filt_frame, int stream_index) {
    int ret = 0;

    const int output_frame_size = enc_ctx->frame_size;

    write_samples_to_fifo(audio_fifo, filt_frame->data, filt_frame->nb_samples);
    int audio_fifo_size = av_audio_fifo_size(audio_fifo);
    if (audio_fifo_size < enc_ctx->frame_size) {
        return 0;
    }

    while (av_audio_fifo_size(audio_fifo) >= enc_ctx->frame_size) {
        const int frame_size = FFMIN(av_audio_fifo_size(audio_fifo), output_frame_size);

        AVFrame *output_frame;
        ret = init_output_frame(&output_frame, enc_ctx, frame_size);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "init_output_frame failed\n");
            return ret;
        }

        if (av_audio_fifo_read(audio_fifo, (void **)output_frame->data, frame_size) < frame_size) {
            av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
            av_frame_free(&output_frame);
            return -1;
        }

        ret = encode_write_frame(output_frame, stream_index);
        av_frame_free(&output_frame);
    }
    return ret;
}

int FFmpegAudioRecorder::encode_write_frame(AVFrame *filt_frame, int stream_index) {
    int ret;
    AVPacket enc_pkt;

    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);

    if (filt_frame) {
        filt_frame->pts = audio_pts;
        AVRational av;
        av.num = 1;
        av.den = enc_ctx->sample_rate;
        audio_pts += av_rescale_q(filt_frame->nb_samples, av, enc_ctx->time_base);
    }

    ret = avcodec_send_frame(enc_ctx, filt_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        //av_log(NULL, AV_LOG_INFO, "Error of EAGAIN or EOF\n");
        return 0;
    } else if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error submitting the frame to the encoder, %s\n", wrap_av_err2str(ret));
        //return ret;
    } else if(filt_frame) {
		enc_samples += filt_frame->nb_samples;
	}
	
    while (1) {
        ret = avcodec_receive_packet(enc_ctx, &enc_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            //av_log(NULL, AV_LOG_INFO, "Error of EAGAIN or EOF\n");
            return 0;
        }
        else if (ret < 0) {
            //av_log(NULL, AV_LOG_ERROR, "Error during encoding, %s\n", wrap_av_err2str(ret));
            return ret;
        }
        /* prepare packet for muxing */
        enc_pkt.stream_index = stream_index;
        av_packet_rescale_ts(&enc_pkt,
            enc_ctx->time_base,
            ofmt_ctx->streams[stream_index]->time_base);
        /* write encoded frame */
        ret = av_write_frame(ofmt_ctx, &enc_pkt);
        write_pts += enc_pkt.duration;
        av_packet_unref(&enc_pkt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error during write frame, %s\n", wrap_av_err2str(ret));
            break;
        }
        //av_log(NULL, AV_LOG_INFO, "write_pts %d\n", write_pts );
    }
    return ret;
}

void FFmpegAudioRecorder::dump_codec(AVCodec *pCodec)
{
    char text[4096];
    std::vector<AVSampleFormat> sample_fmts;
    std::vector<int> samplerates;
    std::vector<int> channel_layouts;

    if(pCodec->sample_fmts!=NULL) {
        for(int i=0; pCodec->sample_fmts[i]!=-1;i++) {
            sample_fmts.push_back(pCodec->sample_fmts[i]);
        }
    }
    if(pCodec->supported_samplerates!=NULL) {
        for(int i=0; pCodec->supported_samplerates[i]!=0;i++) {
            samplerates.push_back(pCodec->supported_samplerates[i]);
        }
    }
    if(pCodec->channel_layouts!=NULL) {
        for(int i=0; pCodec->channel_layouts[i]!=0;i++) {
            channel_layouts.push_back(pCodec->channel_layouts[i]);
        }
    }

    sprintf(text, "AVCodec:{");
    sprintf(text+strlen(text), "sample_fmts:");
    for(int i=0; i<sample_fmts.size(); i++) {
        const char* name = av_get_sample_fmt_name(sample_fmts[i]);
        sprintf(text+strlen(text), "%d(%s)%s", sample_fmts[i], name?name:"",
                i==sample_fmts.size()-1?";":",");
    }
    sprintf(text+strlen(text), "samplerates:");
    for(int i=0; i<samplerates.size(); i++) {
        sprintf(text+strlen(text), "%d%s", samplerates[i],
                i==samplerates.size()-1?";":",");
    }
    sprintf(text+strlen(text), "channel_layouts:");
    for(int i=0; i<channel_layouts.size(); i++) {
        sprintf(text+strlen(text), "%d(ch:%d)%s", channel_layouts[i],
                av_get_channel_layout_nb_channels(channel_layouts[i]),
                i==channel_layouts.size()-1?";":",");
    }
    sprintf(text+strlen(text), "}\n");
    av_log(NULL, AV_LOG_INFO, text);
}

void FFmpegAudioRecorder::statistics()
{
    char text[4096];
    sprintf(text, "statistics:{ enc_sample_rate:%d, enc_channel_layout:%d(ch:%d), enc_codec_id:%d(%s), "
                  "enc_samples:%ld, audio_pts:%ld, write_pts:%ld, enc_timebase:%d/%d }",
            enc_ctx?enc_ctx->sample_rate:-1,
            enc_ctx?enc_ctx->channel_layout:-1, enc_ctx?enc_ctx->channels:-1,
            enc_ctx?enc_ctx->codec_id:-1, enc_ctx?avcodec_get_name(enc_ctx->codec_id):"",
            enc_samples, audio_pts, write_pts,
            enc_ctx?enc_ctx->time_base.num:0, enc_ctx?enc_ctx->time_base.den:0);

    av_log(NULL, AV_LOG_INFO, text);
}

void FFmpegAudioRecorder::close()
{
    int ret = 0;
    //刷新编码器的缓冲区
    if(ofmt_ctx&&ofmt_ctx->pb&&enc_ctx) {
        ret = flush_encoder(ofmt_ctx,out_stream->index);
        ret = av_write_trailer(ofmt_ctx);
        if(ret<0) {
            av_log(NULL, AV_LOG_ERROR, "Error during write trailer, %s\n", wrap_av_err2str(ret));
        }
        statistics();
    }

    if(swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }
    if(audio_fifo) {
        av_audio_fifo_free(audio_fifo);
        audio_fifo = nullptr;
    }
    if(audio_data) {
        av_freep(&audio_data[0]);
        av_free(audio_data);
        audio_data = nullptr;
    }
    if(enc_ctx) {
        avcodec_free_context(&enc_ctx);
        enc_ctx = nullptr;
    }
    if(ofmt_ctx) {
        if(ofmt_ctx->pb)
            avio_close(ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
        ofmt_ctx = nullptr;
    }
}

void FFmpegAudioRecorder::av_log(void *avcl, int level, const char *fmt, ...)
{
    char sprint_buf[1024];

    va_list args;
    int n;
    va_start(args, fmt);
    n = vsnprintf(sprint_buf, sizeof(sprint_buf), fmt, args);
    va_end(args);

    if(level<=log_level)
        print_errmsg(-1, sprint_buf);
}

void FFmpegAudioRecorder::print_errmsg(int code, const char *msg)
{
    printf("debug:[%s]%s", "FFmpegAudioRecorder", msg);
}

void FFmpegAudioRecorder::setLogLevel(int level)
{
    log_level = level;
}

bool FFmpegAudioRecorder::isReady()
{
    return ofmt_ctx && swr_ctx && audio_fifo && audio_data;
}
