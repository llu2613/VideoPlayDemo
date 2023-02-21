#include "FFmpegSwresample.h"

FFmpegSwresample::FFmpegSwresample()
{
    swrCtx = swr_alloc();

    ctx = avformat_alloc_context();

    reset();
}

FFmpegSwresample::~FFmpegSwresample()
{
    if(swrCtx)
        swr_free(&swrCtx);

    if(ctx)
        avformat_free_context(ctx);
}

void FFmpegSwresample::reset()
{
    in_sample_fmt = AV_SAMPLE_FMT_NONE;
    in_sample_rate = 0;
    in_channel_layout = 0;
    out_sample_fmt = AV_SAMPLE_FMT_NONE;
    out_sample_rate = 0;
    out_channel_layout = 0;
}

int FFmpegSwresample::init(enum AVSampleFormat in_fmt, int in_rate, uint64_t in_ch_layout,
                           enum AVSampleFormat out_fmt, int out_rate, uint64_t out_ch_layout)
{
    int ret =0;

    swrCtx = swr_alloc_set_opts(swrCtx, out_ch_layout, out_fmt, out_rate,
                       in_ch_layout, in_fmt, in_rate, 0, NULL);
    ret = swr_init(swrCtx);
    if(ret<0) {
        av_log(ctx, AV_LOG_ERROR, "swr_init failed");
        return ret;
    }

    //获取输出的声道个数
    out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);
    //pcm数据长度
    out_count = av_samples_get_buffer_size(NULL, out_channel_nb, out_rate, out_fmt, 0);

    in_sample_fmt = in_fmt;
    in_sample_rate = in_rate;
    in_channel_layout = in_ch_layout;
    out_sample_fmt = out_fmt;
    out_sample_rate = out_rate;
    out_channel_layout = out_ch_layout;

    return ret;
}

int FFmpegSwresample::mallocOutBuffer(uint8_t **out_buffer, int *out_buffer_size)
{
    //存储pcm数据
    *out_buffer = (uint8_t *) av_malloc(out_count);

    if(!(*out_buffer)) {
        av_log(ctx, AV_LOG_ERROR, "no memory alloc to out_buffer");
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

int FFmpegSwresample::mallocOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size)
{
    /*
     * FFmpeg连载6-音频重采样
     * https://blog.csdn.net/u012944685/article/details/124395001
    */
    // 进行音频重采样
    int src_nb_samples = 0;//avFrame->nb_samples;
    // 为了保持从采样后 dst_nb_samples / dest_sample = src_nb_sample / src_sample_rate
    int64_t max_dst_nb_samples = av_rescale_rnd(src_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);
    // 从采样器中会缓存一部分，获取缓存的长度
    int64_t delay = swr_get_delay(swrCtx, in_sample_rate);
    int64_t dst_nb_samples = av_rescale_rnd(delay + src_nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);

    av_frame_free(out_frame);
    (*out_frame) = av_frame_alloc();
    (*out_frame)->sample_rate = out_sample_rate;
    (*out_frame)->format = out_sample_fmt;
    (*out_frame)->channel_layout = out_channel_layout;
    (*out_frame)->nb_samples = dst_nb_samples;
    // 分配buffer
    av_frame_get_buffer(*out_frame, 0);
    av_frame_make_writable(*out_frame);

    if(nullptr == out_frame){
        //init_out_frame(dst_nb_samples);
    }

    if (dst_nb_samples > max_dst_nb_samples) {
        // 需要重新分配buffer
        //std::cout << "需要重新分配buffer" << std::endl;
        //init_out_frame(dst_nb_samples);
        max_dst_nb_samples = dst_nb_samples;
    }
    return 0;
}

void FFmpegSwresample::freeOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size)
{

}

int FFmpegSwresample::convert(AVFrame *frame, uint8_t **out_buffer, int out_buffer_size)
{
    return convert((const uint8_t **)frame->data, frame->nb_samples,
                   out_buffer, out_buffer_size);
}

int FFmpegSwresample::convert(const uint8_t **in_buffer, int nb_samples,
                              uint8_t **out_buffer, int out_buffer_size) {
    if(!out_buffer) {
        av_log(ctx, AV_LOG_ERROR, "out_buffer is null");
    }

    //重采样
    int out_samples = swr_convert(swrCtx, out_buffer, out_buffer_size,
                                  in_buffer, nb_samples);

    return out_samples;
}


enum AVSampleFormat FFmpegSwresample::inSampleFmt()
{
    return in_sample_fmt;
}
int FFmpegSwresample::inSampleRate()
{
    return in_sample_rate;
}
uint64_t FFmpegSwresample::inChannelLayout()
{
    return in_channel_layout;
}

enum AVSampleFormat FFmpegSwresample::outSampleFmt()
{
    return out_sample_fmt;
}
int FFmpegSwresample::outSampleRate()
{
    return out_sample_rate;
}
uint64_t FFmpegSwresample::outChannelLayout()
{
    return out_channel_layout;
}
