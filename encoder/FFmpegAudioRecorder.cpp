#include "FFmpegAudioRecorder.h"

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
            return AVERROR_EXIT;
        }

        if (av_audio_fifo_read(audio_fifo, (void **)output_frame->data, frame_size) < frame_size) {
            av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
            av_frame_free(&output_frame);
            return AVERROR_EXIT;
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
    if(ofmt_ctx) {
        av_log(NULL,AV_LOG_ERROR,"ofmt_ctx has been inited, please close first!\n");
        return 0;
    }

    int ret = 0;
    ofmt_ctx = avformat_alloc_context();
    AVOutputFormat *oformat = av_guess_format(NULL,output,NULL);
    if (oformat==NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to find the output format\n");
        return -1;
    }
    if (avformat_alloc_output_context2(&ofmt_ctx,oformat,oformat->name,output) <0){
        av_log(NULL,AV_LOG_ERROR,"fail to alloc output context\n");
        return -1;
    }
    out_stream = avformat_new_stream(ofmt_ctx,NULL);
    if (out_stream == NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to create new stream\n");
        return -1;
    }

    AVCodec *pCodec = avcodec_find_encoder(oformat->audio_codec);
    if (pCodec == NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to find codec\n");
        return -1;
    }

    enc_ctx = avcodec_alloc_context3(pCodec);
    if (!enc_ctx) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate the encoder context\n");
        return -1;
    }

    AVSampleFormat sample_fmt = pCodec->sample_fmts[0]!=AV_SAMPLE_FMT_NONE?
                pCodec->sample_fmts[0]:AV_SAMPLE_FMT_NONE;
    //enc_ctx->codec_id = oformat->audio_codec;
    enc_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    enc_ctx->sample_fmt = sample_fmt;
    enc_ctx->channel_layout = src_ch_layout;
    enc_ctx->channels = av_get_channel_layout_nb_channels(src_ch_layout);
    enc_ctx->sample_rate = src_sample_rate;
    /* 比特率=采样率 * 采样位数 * 通道数 * 压缩比例 */
//    enc_ctx->bit_rate = (enc_ctx->sample_rate)*(enc_ctx->channels)*
//            av_get_bytes_per_sample(enc_ctx->sample_fmt)*8;

    ret = avcodec_open2(enc_ctx,pCodec,NULL);
    if (ret < 0){
        av_log(NULL,AV_LOG_ERROR,"fail to open codec\n");
        return ret;
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
//    printf("swr o: chlt %d fmt %d rate %d, i: chlt %d fmt %d rate %d\n",
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

    return 0;
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

    AVOutputFormat *oformat = av_guess_format(NULL,output, NULL);
    if (oformat==NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to find the output format\n");
        return -1;
    }

    AVCodec *pCodec = avcodec_find_encoder(oformat->audio_codec);
    if (pCodec == NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to find codec\n");
        return -1;
    }

    AVSampleFormat enc_sample_fmt = AV_SAMPLE_FMT_NONE;
    int enc_sample_rate = 0;
    uint64_t enc_ch_layout = 0;
    for(int i=0; pCodec->sample_fmts[i]!=AV_SAMPLE_FMT_NONE;i++) {
        if(out_sample_fmt==AV_SAMPLE_FMT_NONE
                ||out_sample_fmt==pCodec->sample_fmts[i]) {
            enc_sample_fmt = pCodec->sample_fmts[i];
            break;
        }
    }
    for(int i=0; pCodec->supported_samplerates[i]!=0;i++) {
        if(out_sample_rate==0
                ||out_sample_rate==pCodec->supported_samplerates[i]) {
            enc_sample_rate = pCodec->supported_samplerates[i];
            break;
        }
    }
    for(int i=0; pCodec->channel_layouts[i]!=0;i++) {
        if(out_ch_layout==0
                ||out_ch_layout==pCodec->channel_layouts[i]) {
            enc_ch_layout = pCodec->channel_layouts[i];
            break;
        }
    }

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

    ofmt_ctx = avformat_alloc_context();

    if (avformat_alloc_output_context2(&ofmt_ctx,oformat,oformat->name,output) <0){
        av_log(NULL,AV_LOG_ERROR,"fail to alloc output context\n");
        return -1;
    }
    out_stream = avformat_new_stream(ofmt_ctx,NULL);
    if (out_stream == NULL){
        av_log(NULL,AV_LOG_ERROR,"fail to create new stream\n");
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
    /* 比特率=采样率 * 采样位数 * 通道数 * 压缩比例 */
//    enc_ctx->bit_rate = (enc_ctx->sample_rate)*(enc_ctx->channels)*
//            av_get_bytes_per_sample(enc_ctx->sample_fmt)*8;

    av_log(NULL, AV_LOG_INFO, "enc_ctx: sp %d ch %d fmt %d(bits:%d) br %d\n", enc_ctx->sample_rate, enc_ctx->channels,
           enc_ctx->sample_fmt, av_get_bytes_per_sample(enc_ctx->sample_fmt), enc_ctx->bit_rate);

    ret = avcodec_open2(enc_ctx,pCodec,NULL);
    if (ret < 0){
        av_log(NULL,AV_LOG_ERROR,"fail to open codec\n");
        return ret;
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
//    printf("swr o: chlt %d fmt %d rate %d, i: chlt %d fmt %d rate %d\n",
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
        return AVERROR_EXIT;
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
    int error;

    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + nb_samples)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not reallocate FIFO\n");
        return error;
    }

    if (av_audio_fifo_write(fifo, (void **)converted_input_samples,
        nb_samples) < nb_samples) {
        av_log(NULL, AV_LOG_ERROR, "Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }
    return 0;
}

int FFmpegAudioRecorder::encode_write_frame_fifo(AVFrame *filt_frame, unsigned int stream_index) {
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
        if (init_output_frame(&output_frame, enc_ctx, frame_size) < 0) {
            av_log(NULL, AV_LOG_ERROR, "init_output_frame failed\n");
            return AVERROR_EXIT;
        }

        if (av_audio_fifo_read(audio_fifo, (void **)output_frame->data, frame_size) < frame_size) {
            av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
            av_frame_free(&output_frame);
            return AVERROR_EXIT;
        }

        ret = encode_write_frame(output_frame, stream_index);
        av_frame_free(&output_frame);
    }
    return ret;
}

int FFmpegAudioRecorder::encode_write_frame(AVFrame *filt_frame, unsigned int stream_index) {
    int ret;
    AVPacket enc_pkt;

    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);

    if (filt_frame) {
        filt_frame->pts = audio_pts;
        audio_pts += filt_frame->nb_samples;
    }

    ret = avcodec_send_frame(enc_ctx, filt_frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error submitting the frame to the encoder, %s\n", wrap_av_err2str(ret));
        return ret;
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
        /* mux encoded frame */
        ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
        av_packet_unref(&enc_pkt);
        if (ret < 0)
            return ret;
    }
}

void FFmpegAudioRecorder::close()
{
    //刷新编码器的缓冲区
    if(ofmt_ctx&&ofmt_ctx->pb&&enc_ctx) {
        flush_encoder(ofmt_ctx,out_stream->index);
        av_write_trailer(ofmt_ctx);
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

    if(level<AV_LOG_WARNING) {
        print_errmsg(-1, sprint_buf);
    }
    printf("[%s]%s", "FFmpegAudioRecorder", sprint_buf);
}

void FFmpegAudioRecorder::print_errmsg(int code, const char *msg)
{

}

bool FFmpegAudioRecorder::isReady()
{
    return ofmt_ctx && swr_ctx && audio_fifo && audio_data;
}
