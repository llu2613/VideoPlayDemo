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

    out_nb_channels = FF_INVALID;
    out_sample_fmt = AV_SAMPLE_FMT_NONE;
    out_sample_rate = FF_INVALID;
    skipSamples = 0;
    mergeSamples = LONG_MAX;

    av_log_set_level(AV_LOG_WARNING);
//    av_log_set_callback(ff_log_callback);
}

FFmpegAudioMerger::~FFmpegAudioMerger()
{

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
    this->out_file = out_file;
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

        /* calculate samples */
        if(dec_ctx) {
            skipSamples = skipSec>0?(skipSec*dec_ctx->sample_rate):0;
            mergeSamples = totalSec>0?(totalSec*dec_ctx->sample_rate):LONG_MAX;
//            int mergeSec = duration-trimTailSec-skipSec;
//            mergeSamples = mergeSec>1?mergeSec*dec_ctx->sample_rate:LONG_MAX;
        }

        /* allocate sample fifo */
        sample_fifo = av_audio_fifo_alloc(dec_ctx->sample_fmt,
                                          av_get_channel_layout_nb_channels(dec_ctx->channel_layout),
                                          dec_ctx->frame_size);

        /* decode and write samples */
        if(!has_opened) {
            ret = openOutput();
            if(ret<0)
                print_error(ret, "Canot open output file!");
        }
        if(ret>=0) {
            while((ret=decoding())>=0);
        }

        /* delete sample fifo */
        if(sample_fifo) {
            flush_sample_fifo();
            av_audio_fifo_free(sample_fifo);
            sample_fifo = nullptr;
        }

        close();
    } else {
        print_error(0, "Canot open input file!");
    }

    skipSamples = 0;
    mergeSamples = LONG_MAX;

    return ret;
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

void FFmpegAudioMerger::print_error(int code, std::string msg)
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

    ret = mRecoder.open(output_filename, in_codec_ctx->channel_layout,
                        in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate,
                        out_nb_channels>0?av_get_default_channel_layout(out_nb_channels):FF_INVALID,
                        out_sample_fmt, out_sample_rate);

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

int FFmpegAudioMerger::init_input_frame(AVFrame **frame,
        AVCodecContext *input_codec_context, int frame_size)
{
    int error;

    if (!(*frame = av_frame_alloc())) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate output frame\n");
        return AVERROR_EXIT;
    }

    (*frame)->nb_samples = frame_size;
    (*frame)->channel_layout = input_codec_context->channel_layout;
    (*frame)->format = input_codec_context->sample_fmt;
    (*frame)->sample_rate = input_codec_context->sample_rate;

    if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate output frame samples (error '%s')\n",
            wrap_av_err2str(error));
        av_frame_free(frame);
        return error;
    }

    return 0;
}

int FFmpegAudioMerger::write_frame_fifo(AVFrame *frame)
{
    const AVCodecContext *dec_ctx = audioCodecContext();

    if(mergeSamples<=0)
        return 0;

    write_samples_to_fifo(sample_fifo, frame->data, frame->nb_samples);
    int audio_fifo_size = av_audio_fifo_size(sample_fifo);
    if (audio_fifo_size < dec_ctx->frame_size) {
        return 0;
    }

    int skipFramSize = skipSamples-samples>dec_ctx->frame_size?
                dec_ctx->frame_size:(skipSamples>samples?skipSamples-samples:0);

    if(skipFramSize>0) {
        AVFrame *input_frame;
        if (init_input_frame(&input_frame, const_cast<AVCodecContext*>(dec_ctx), skipFramSize) < 0) {
            av_log(NULL, AV_LOG_ERROR, "init_input_frame failed\n");
            return AVERROR_EXIT;
        }

        if (av_audio_fifo_read(sample_fifo, (void **)input_frame->data, skipFramSize) < skipFramSize) {
            av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
            av_frame_free(&input_frame);
            return AVERROR_EXIT;
        }
        av_frame_free(&input_frame);
        return 0;
    }

    int ret = 0;
    while (mergeSamples>0 && av_audio_fifo_size(sample_fifo) >= dec_ctx->frame_size) {
        int frame_size = FFMIN(av_audio_fifo_size(sample_fifo), dec_ctx->frame_size);
        ret = fifo_write_encoder(frame_size);
        if(ret<0) {
            break;
        }
    }

    return 0;
}

int FFmpegAudioMerger::fifo_write_encoder(int frame_size)
{
    int ret=0;
    const AVCodecContext *dec_ctx = audioCodecContext();

    AVFrame *input_frame;
    if (init_input_frame(&input_frame, const_cast<AVCodecContext*>(dec_ctx), frame_size) < 0) {
        av_log(NULL, AV_LOG_ERROR, "init_output_frame failed\n");
        return AVERROR_EXIT;
    }

    if (av_audio_fifo_read(sample_fifo, (void **)input_frame->data, frame_size) < frame_size) {
        av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
        av_frame_free(&input_frame);
        return AVERROR_EXIT;
    }

    ret = mRecoder.addData(input_frame, nullptr);
    if(ret>=0) {
        mergeSamples -= frame_size;
    }
    av_frame_free(&input_frame);

    return ret;
}

int FFmpegAudioMerger::flush_sample_fifo()
{
    if(samples<=skipSamples || mergeSamples<=0) {
        return 0;
    }

    const AVCodecContext *dec_ctx = audioCodecContext();

    while (dec_ctx && mergeSamples>0) {
        int frame_size = FFMIN(av_audio_fifo_size(sample_fifo), FFMIN(dec_ctx->frame_size, mergeSamples));
        fifo_write_encoder(frame_size);
    }

    return 0;
}
