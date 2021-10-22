#ifndef FFMPEGSWRESAMPLE_H
#define FFMPEGSWRESAMPLE_H

extern "C" {
//封装格式
#include "libavformat/avformat.h"
//解码
#include "libavcodec/avcodec.h"
//缩放
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

class FFmpegSwresample
{
public:
    explicit FFmpegSwresample();
    ~FFmpegSwresample();

    int init(AVCodecContext *pCodeCtx,
             enum AVSampleFormat out_fmt,
             int out_rate, uint64_t out_ch_layout);
    int init(enum AVSampleFormat in_fmt, int in_rate, uint64_t in_ch_layout,
             enum AVSampleFormat out_fmt, int out_rate, uint64_t out_ch_layout);

    int mallocOutBuffer(uint8_t **out_buffer, int *out_buffer_size);

    void freeOutBuffer(uint8_t **out_buffer, int *out_buffer_size);

    int convert(AVFrame *frame, uint8_t **out_buffer, int out_buffer_size);
    int convert(const uint8_t **in_buffer, int nb_samples,
                uint8_t **out_buffer, int out_buffer_size);

    enum AVSampleFormat inSampleFmt();
    int inSampleRate();
    uint64_t inChannelLayout();

    enum AVSampleFormat outSampleFmt();
    int outSampleRate();
    uint64_t outChannelLayout();

private:
    SwrContext *swrCtx;

    enum AVSampleFormat in_sample_fmt, out_sample_fmt;
    int in_sample_rate, out_sample_rate;
    uint64_t in_channel_layout, out_channel_layout;
    int out_channel_nb;
    int out_count;

};

#endif // FFMPEGSWRESAMPLE_H
