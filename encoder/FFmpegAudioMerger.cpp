#include "FFmpegAudioMerger.h"
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
    duration = 0;
    samples = 0;

    out_ch_layout = FF_INVALID;
    out_sample_fmt = AV_SAMPLE_FMT_NONE;
    out_sample_rate = FF_INVALID;

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
           int64_t  out_ch_layout,
           enum AVSampleFormat out_sample_fmt,
           int out_sample_rate)
{
    this->out_file = out_file;
    this->out_ch_layout = out_ch_layout;
    this->out_sample_fmt = out_sample_fmt;
    this->out_sample_rate = out_sample_rate;
}

int FFmpegAudioMerger::merge(std::string in_file)
{
    int ret=0;
    if(open(in_file.c_str(), NULL, false)>=0) {
        //计算时长
        const AVStream *st = audioStream();
        const AVFormatContext* ctx = formatContext();
        if(ctx&&ctx->duration!=AV_NOPTS_VALUE) {
            duration = ctx->duration/AV_TIME_BASE;
            samples = 0;
        } else if(st) {
            duration = st->duration*st->time_base.num/st->time_base.den;
            samples = 0;
        }

        if(!has_opened) {
            ret = openOutput();
            if(ret<0)
                print_error(ret, "Canot open output file!");
        }
        if(ret>=0) {
            while((ret=decoding())>=0);
        }
        close();
    } else {
        print_error(0, "Canot open input file!");
    }

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
    mRecoder.addData(frame, packet);

    //计算进度
    const AVCodecContext * codec = audioCodecContext();
    if(codec) {
        samples += frame->nb_samples;
        const int64_t seconds = samples/codec->sample_rate;
        progress(seconds, duration);
    }
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
                        out_ch_layout, out_sample_fmt, out_sample_rate);

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
