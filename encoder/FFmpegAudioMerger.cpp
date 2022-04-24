#include "FFmpegAudioMerger.h"
#include "common/ffmpeg_commons.h"
#include <climits>
#include <iostream>
#include <QDebug>

using namespace std;

static void ff_log_callback(void *avcl, int level, const char *fmt,
    va_list vl) {

//	if (level >= av_log_get_level())
//		return;

    char logBuffer[2048];
    int cnt = vsnprintf(logBuffer, sizeof(logBuffer) / sizeof(char), fmt, vl);
    cout << logBuffer;
}

FFmpegAudioMerger::FFmpegAudioMerger()
{
    has_opened = false;
    mCallback = nullptr;
    sample_fifo = nullptr;
    duration = 0;
    samples = 0;
    skipped_samples = 0;
    written_samples = 0;

    out_codec_id = AV_CODEC_ID_NONE;
    out_nb_channels = FF_INVALID;
    out_sample_fmt = AV_SAMPLE_FMT_NONE;
    out_sample_rate = FF_INVALID;
    skipSamples = 0;
    mergeSamples = LONG_MAX;

    log_level = AV_LOG_INFO;
}

FFmpegAudioMerger::~FFmpegAudioMerger()
{

}

void FFmpegAudioMerger::setLogLevel(int level)
{
    log_level = level;
}

void FFmpegAudioMerger::setCallback(FFMergerCallback *callback)
{
    std::lock_guard<std::mutex> lk(m_mutex);

    mCallback = callback;
}

void FFmpegAudioMerger::start(std::string out_file)
{
    start(out_file, FF_INVALID, AV_SAMPLE_FMT_NONE, FF_INVALID);
}

void FFmpegAudioMerger::start(std::string out_file,
                              int out_nb_channels,
                              enum AVSampleFormat out_sample_fmt,
                              int out_sample_rate)
{
    start(out_file, "", AV_CODEC_ID_NONE,
          out_nb_channels, out_sample_fmt, out_sample_rate);
}

void FFmpegAudioMerger::start(std::string out_file, std::string format_name, enum AVCodecID codec_id,
                              int out_nb_channels, enum AVSampleFormat out_sample_fmt, int out_sample_rate)
{
    this->out_file = out_file;
    this->out_format_name = format_name;
    this->out_codec_id = codec_id;
    this->out_nb_channels = out_nb_channels;
    this->out_sample_fmt = out_sample_fmt;
    this->out_sample_rate = out_sample_rate;
}

int FFmpegAudioMerger::merge(std::string in_file)
{
    return merge(in_file, 0, 0);
}

int FFmpegAudioMerger::merge(std::string in_file, int skipSec, int totalSec)
{
    int ret=0;
    if(open(in_file.c_str(), NULL, false)>=0) {
        /* calculate duration */
        const AVStream *st = audioStream();
        const AVFormatContext* ctx = formatContext();
        const AVCodecContext* dec_ctx = audioCodecContext();
        if(ctx&&ctx->duration!=AV_NOPTS_VALUE) {
            duration = ctx->duration/AV_TIME_BASE;
        } else if(st&&st->duration!=AV_NOPTS_VALUE) {
            duration = st->duration*st->time_base.num/st->time_base.den;
        }
        samples = 0;
        skipped_samples = 0;
        written_samples = 0;

        /* calculate samples */
        if(dec_ctx) {
            skipSamples = skipSec>0?(skipSec*dec_ctx->sample_rate):0;
            mergeSamples = totalSec>0?(totalSec*dec_ctx->sample_rate):LONG_MAX;
            qDebug()<<"skipSamples:"<<skipSamples<<"mergeSamples:"<<mergeSamples;
        }

        /* allocate sample fifo */
        sample_fifo = av_audio_fifo_alloc(dec_ctx->sample_fmt,
                                          av_get_channel_layout_nb_channels(dec_ctx->channel_layout),
                                          dec_ctx->frame_size);

        /* decode and write samples */
        if(!has_opened) {
            ret = openOutput();
            if(ret<0)
                print_errmsg(ret, "Canot open output file!");
        }
        if(ret>=0) {
            isStopFlag = false;
            while((ret=decoding())>=0&&!isStopFlag);
        }

        /* delete sample fifo */
        if(sample_fifo) {
            flush_sample_fifo();
            av_audio_fifo_free(sample_fifo);
            sample_fifo = nullptr;
        }

        close();
    } else {
        print_errmsg(0, "Canot open input file!");
    }

    skipSamples = 0;
    mergeSamples = LONG_MAX;

    merge_statistics();

    return ret;
}

void FFmpegAudioMerger::merge_statistics()
{
    char text[4096];
    sprintf(text, "merge_statistics:{ samples:%d, skipped_samples:%d, written_samples:%d }\n",
            samples, skipped_samples, written_samples);

    av_log(NULL, AV_LOG_INFO, text);
}

void FFmpegAudioMerger::finish()
{
    if(has_opened) {
        closeOutput();
    }
}

int FFmpegAudioMerger::audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    return FFmpegMediaDecoder::audioRawFrame(pCodecCtx, frame, packet);
}

int FFmpegAudioMerger::videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    return 0;
}

void FFmpegAudioMerger::audioDecodedData(AVFrame *frame, AVPacket *packet)
{
    const AVCodecContext *dec_ctx = audioCodecContext();
    //计算进度
    int64_t second = -1;
    if(dec_ctx) {
        samples += frame->nb_samples;
        second = samples/dec_ctx->sample_rate;
    }

    write_frame_fifo(frame);

    progress(second, duration);
}

void FFmpegAudioMerger::videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight)
{
    return;
}

void FFmpegAudioMerger::progress(long current, long total)
{
    std::lock_guard<std::mutex> lk(m_mutex);

    if(mCallback) {
        mCallback->onMergerProgress(current, total);
    }
}

void FFmpegAudioMerger::av_log(void *avcl, int level, const char *fmt, ...)
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

void FFmpegAudioMerger::print_errmsg(int code, const char *msg)
{
    std::lock_guard<std::mutex> lk(m_mutex);

    if(mCallback) {
        mCallback->onMergerError(code, msg);
    }
}

int FFmpegAudioMerger::openOutput()
{
    int ret=0;
    has_opened = false;

    if(!out_file.size()) {
        printf("openOutput out_file is empty!\n");
        return -1;
    }

    if(!audioStream()) {
        printf("audioStream is null!\n");
        return -1;
    }

    const char *output_filename = out_file.c_str();
    const AVCodecContext *in_codec_ctx = audioCodecContext();

//    ret = mRecoder.open(output_filename, in_codec_ctx->channel_layout,
//                  in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate);

    ret = mRecoder.open(output_filename, out_format_name.size()?out_format_name.data():NULL,
                        out_codec_id!=AV_CODEC_ID_NONE?out_codec_id:in_codec_ctx->codec_id,
                        in_codec_ctx->channel_layout, in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate,
                        out_nb_channels>0?av_get_default_channel_layout(out_nb_channels):in_codec_ctx->channel_layout,
                        out_sample_fmt!=AV_SAMPLE_FMT_NONE?out_sample_fmt:in_codec_ctx->sample_fmt,
                        out_sample_rate>0?out_sample_rate:in_codec_ctx->sample_rate);

    has_opened = true;

    return ret;
}

void FFmpegAudioMerger::closeOutput()
{
    if(has_opened) {
        mRecoder.close();
        has_opened = false;
    }
}

int FFmpegAudioMerger::write_samples_to_fifo(AVAudioFifo *fifo,
    uint8_t **converted_input_samples,
    const int nb_samples)
{
    int ret;

    ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + nb_samples);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not reallocate FIFO\n");
        return ret;
    }

    ret = av_audio_fifo_write(fifo, (void **)converted_input_samples, nb_samples);
    if (ret < nb_samples) {
        av_log(NULL, AV_LOG_ERROR, "Could not write data to FIFO\n");
        return ret;
    }

    return ret;
}

int FFmpegAudioMerger::init_input_frame(AVFrame **frame,
        AVCodecContext *input_codec_context, int frame_size)
{
    int ret;

    if (!(*frame = av_frame_alloc())) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate output frame\n");
        return -1;
    }

    (*frame)->nb_samples = frame_size;
    (*frame)->channel_layout = input_codec_context->channel_layout;
    (*frame)->format = input_codec_context->sample_fmt;
    (*frame)->sample_rate = input_codec_context->sample_rate;

    ret = av_frame_get_buffer(*frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate output frame samples (error '%s')\n",
            wrap_av_err2str(ret));
        av_frame_free(frame);
        return ret;
    }

    return ret;
}

int FFmpegAudioMerger::write_frame_fifo(AVFrame *frame)
{
    int ret = 0;
    const AVCodecContext *dec_ctx = audioCodecContext();

    if(written_samples>=mergeSamples) {
        isStopFlag = true;
        return 0;
    }

    if(samples<=skipSamples) {
        return 0;
    }

    write_samples_to_fifo(sample_fifo, frame->data, frame->nb_samples);
    int audio_fifo_size = av_audio_fifo_size(sample_fifo);
    if (audio_fifo_size < dec_ctx->frame_size) {
        return 0;
    }
/*
    int skipFramSize = 0;
    if(skipSamples>samples) {
        skipFramSize = (skipSamples-samples)>dec_ctx->frame_size?
                    dec_ctx->frame_size:(skipSamples>samples?skipSamples-samples:0);
    }

    if(skipFramSize>0) {
        AVFrame *input_frame;
        ret = init_input_frame(&input_frame, const_cast<AVCodecContext*>(dec_ctx), dec_ctx->frame_size);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "init_input_frame failed\n");
            return ret;
        }

        int nb_samples = av_audio_fifo_read(sample_fifo, (void **)input_frame->data, skipFramSize);
        if (nb_samples < skipFramSize) {
            av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
            av_frame_free(&input_frame);
            return -1;
        }
        skipped_samples += nb_samples;
        av_frame_free(&input_frame);
        return 0;
    }
*/
    while (av_audio_fifo_size(sample_fifo) >= dec_ctx->frame_size) {
        if(written_samples>=mergeSamples) {
            isStopFlag = true;
            break;
        }
        int wrote_size = 0;
        int frame_size = FFMIN(av_audio_fifo_size(sample_fifo), dec_ctx->frame_size);
        ret = fifo_write_encoder(frame_size, &wrote_size);
        if(ret<0) {
            break;
        }
    }

    return 0;
}

int FFmpegAudioMerger::fifo_write_encoder(int frame_size, int *ret_wrote_size)
{
    int ret=0;
    const AVCodecContext *dec_ctx = audioCodecContext();
    if(frame_size>dec_ctx->frame_size)
        frame_size = dec_ctx->frame_size;

    AVFrame *input_frame;
    ret = init_input_frame(&input_frame, const_cast<AVCodecContext*>(dec_ctx), dec_ctx->frame_size);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "init_output_frame failed\n");
        return ret;
    }

    int nb_samples = av_audio_fifo_read(sample_fifo, (void **)input_frame->data, frame_size);
    if (nb_samples < frame_size) {
        av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
        av_frame_free(&input_frame);
        return -1;
    }

    ret = mRecoder.addData(input_frame, nullptr);
    if(ret>=0 && ret_wrote_size) {
        written_samples += nb_samples;
        *ret_wrote_size = frame_size;
    }
    av_frame_free(&input_frame);

    return ret;
}

int FFmpegAudioMerger::flush_sample_fifo()
{
    if(samples<=skipSamples || mergeSamples<0) {
        return 0;
    }

    const AVCodecContext *dec_ctx = audioCodecContext();

    int ret = 0;
    while (mergeSamples>0 && av_audio_fifo_size(sample_fifo)) {
        int wrote_size = 0;
        int frame_size = FFMIN3(av_audio_fifo_size(sample_fifo), dec_ctx->frame_size, mergeSamples);
        ret = fifo_write_encoder(frame_size, &wrote_size);
        mergeSamples -= wrote_size;
        if(ret<0) {
            break;
        }
    }

    return 0;
}
