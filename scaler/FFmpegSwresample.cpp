#include "FFmpegSwresample.h"

FFmpegSwresample::FFmpegSwresample(QObject *parent) : QObject(parent)
{
    swrCtx = swr_alloc();
}

FFmpegSwresample::~FFmpegSwresample()
{
    swr_free(&swrCtx);
}

int FFmpegSwresample::init(AVCodecContext *pCodeCtx,
                            enum AVSampleFormat out_fmt,
                            int out_rate, uint64_t out_ch)
{
    int ret =0;

    //输入的采样格式
    in_sample_fmt = pCodeCtx->sample_fmt;
    //输入的采样率
    in_sample_rate = pCodeCtx->sample_rate;
    //输入的声道布局
    in_ch_layout = pCodeCtx->channel_layout;

    out_sample_fmt = out_fmt;
    out_sample_rate = out_rate;
    out_ch_layout = out_ch;

    swr_alloc_set_opts(swrCtx, out_ch_layout, out_sample_fmt, out_sample_rate,
                       in_ch_layout, in_sample_fmt, in_sample_rate, 0, NULL);
    ret = swr_init(swrCtx);
    if(ret<0) {
        printf("swr_init failed");
        return ret;
    }

    //获取输出的声道个数
    out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);
    //pcm数据长度
    out_count = av_samples_get_buffer_size(NULL, out_channel_nb,
                                           out_sample_rate, out_sample_fmt, 0);

    return ret;
}

int FFmpegSwresample::mallocOutBuffer(uint8_t **out_buffer, int *out_buffer_size)
{
    //存储pcm数据
    *out_buffer = (uint8_t *) av_malloc(out_count);

    if(!(*out_buffer)) {
        printf("no memory alloc to out_buffer");
        return -1;
    }

    *out_buffer_size = out_count;

    return 0;
}

void FFmpegSwresample::freeOutBuffer(uint8_t **out_buffer, int *out_buffer_size)
{
    if(*out_buffer) {
        av_free(*out_buffer);
        *out_buffer = nullptr;
        *out_buffer_size = 0;
    }
}

int FFmpegSwresample::convert(AVFrame *frame, uint8_t *out_buffer, int out_buffer_size)
{
    if(!out_buffer) {
        printf("out_buffer is null");
    }

    //重采样
    int out_samples = swr_convert(swrCtx, &out_buffer, out_buffer_size,
                                  (const uint8_t **)frame->data, frame->nb_samples);

    return out_samples;
}

int FFmpegSwresample::outChannels()
{
    return out_channel_nb;
}

enum AVSampleFormat FFmpegSwresample::outSampleFmt()
{
    return out_sample_fmt;
}

int FFmpegSwresample::outSampleRate()
{
    return out_sample_rate;
}
