#ifndef FFMPEGSWRESAMPLE_H
#define FFMPEGSWRESAMPLE_H

#include <QObject>

extern "C" {
//封装格式
#include "libavformat/avformat.h"
//解码
#include "libavcodec/avcodec.h"
//缩放
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

class FFmpegSwresample : public QObject
{
    Q_OBJECT
public:
    explicit FFmpegSwresample(QObject *parent = nullptr);
    ~FFmpegSwresample();

    int init(AVCodecContext *pCodeCtx,
             enum AVSampleFormat out_fmt,
             int out_rate, uint64_t out_ch);

    int mallocOutBuffer(uint8_t **out_buffer, int *out_buffer_size);

    void freeOutBuffer(uint8_t **out_buffer, int *out_buffer_size);

    int convert(AVFrame *frame, uint8_t *out_buffer, int out_buffer_size);

    int outChannels();
    enum AVSampleFormat outSampleFmt();
    int outSampleRate();

private:
    SwrContext *swrCtx;

    enum AVSampleFormat in_sample_fmt, out_sample_fmt;
    int in_sample_rate, out_sample_rate;
    uint64_t in_ch_layout, out_ch_layout;
    int out_channel_nb;
    int out_count;

signals:

public slots:
};

#endif // FFMPEGSWRESAMPLE_H
