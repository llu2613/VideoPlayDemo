#include "FFmpegAudioMerger.h"
#include <iostream>

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

    av_log_set_level(AV_LOG_WARNING);
//    av_log_set_callback(ff_log_callback);
}

FFmpegAudioMerger::~FFmpegAudioMerger()
{

}

void FFmpegAudioMerger::start(std::string out_file)
{
    this->out_file = out_file;
}

int FFmpegAudioMerger::merge(std::string in_file)
{
    int ret=0;
    if(open(in_file.c_str(), NULL, false)>=0) {
        if(!has_opened) {
            ret = openOutput();
        }
        if(ret>=0) {
            while((ret=decoding())>=0);
        }
        close();
    }
    return ret;
}

void FFmpegAudioMerger::finish()
{
    if(has_opened) {
        closeOutput();
    }
}

void FFmpegAudioMerger::audioDecodedData(AVFrame *frame, AVPacket *packet)
{
    mRecoder.addData(frame, packet);
}

void FFmpegAudioMerger::videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight)
{
    return;
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

    ret = mRecoder.open(output_filename, in_codec_ctx->channel_layout,
                  in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate);

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
