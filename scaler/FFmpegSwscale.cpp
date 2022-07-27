#include "FFmpegSwscale.h"

FFmpegSwscale::FFmpegSwscale()
{
    swsCtx = nullptr;

    ctx = avformat_alloc_context();
}

FFmpegSwscale::~FFmpegSwscale()
{
    if(swsCtx)
        sws_freeContext(swsCtx);

    if(ctx)
        avformat_free_context(ctx);
}

int FFmpegSwscale::init(AVCodecContext *pCodecCtx,
                        int out_width, int out_height,
                        enum AVPixelFormat out_fmt)
{
    return init(pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height,
                out_fmt,out_width,out_height);
}

int FFmpegSwscale::init(enum AVPixelFormat in_pixel_fmt, int in_width, int in_height,
                        enum AVPixelFormat out_pixel_fmt, int out_width, int out_height)
{
    if(swsCtx)
        sws_freeContext(swsCtx);

    swsCtx = sws_getContext(in_width,in_height,in_pixel_fmt,
                            out_width, out_height, out_pixel_fmt,
                            SWS_BICUBIC, NULL, NULL, NULL);
    if(!swsCtx) {
        av_log(ctx, AV_LOG_ERROR, "swr_init failed");
        return -1;
    }

    //新数据长度
    out_count = av_image_get_buffer_size(out_pixel_fmt, out_width, out_height, 1);

    in_frame_fmt = in_pixel_fmt;
    in_frame_width = in_width;
    in_frame_height = in_height;
    out_frame_fmt = out_pixel_fmt;
    out_frame_width = out_width;
    out_frame_height = out_height;

    return 0;
}

int FFmpegSwscale::mallocOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size)
{
    //内存分配
    *out_frame = av_frame_alloc();
    if(!(*out_frame)) {
        av_log(ctx, AV_LOG_ERROR, "no memory alloc to out_frame");
        return -1;
    }
    //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
    //缓冲区分配内存
    *out_buffer = (uint8_t *)av_malloc(out_count);

    if(!(*out_buffer)) {
        av_log(ctx, AV_LOG_ERROR, "no memory alloc to out_buffer");
        return -1;
    }

    *out_buffer_size = out_count;
    //初始化缓冲区
    int bytes = av_image_fill_arrays((*out_frame)->data, (*out_frame)->linesize, *out_buffer,
                         out_frame_fmt, out_frame_width, out_frame_height, 1);

    (*out_frame)->width = out_frame_width;
    (*out_frame)->height = out_frame_height;
    (*out_frame)->format = out_frame_fmt;
    return bytes;
}

void FFmpegSwscale::freeOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size)
{
    if(*out_frame)
        av_frame_free(out_frame);

    if(*out_buffer) {
        av_free(*out_buffer);
        *out_buffer = nullptr;
        *out_buffer_size = 0;
    }
}

int FFmpegSwscale::scale(AVFrame *pFrame, int srcSliceH, AVFrame *pOutFrame)
{
    if(!swsCtx) {
        av_log(ctx, AV_LOG_ERROR, "swsCtx is null");
        return -1;
    }

    if(!pOutFrame) {
        av_log(ctx, AV_LOG_ERROR, "pOutFrame is null");
        return -1;
    }

//    int out_height = sws_scale(swsCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
//                               pOutFrame->data, pOutFrame->linesize);
    int out_height = sws_scale(swsCtx, pFrame->data, pFrame->linesize, 0, srcSliceH,
                               pOutFrame->data, pOutFrame->linesize);

    return out_height;
}

enum AVPixelFormat FFmpegSwscale::inFormat()
{
    return in_frame_fmt;
}
int FFmpegSwscale::inHeight()
{
    return in_frame_height;
}
int FFmpegSwscale::inWidth()
{
    return in_frame_width;
}

enum AVPixelFormat FFmpegSwscale::outFormat()
{
    return out_frame_fmt;
}
int FFmpegSwscale::outHeight()
{
    return out_frame_height;
}
int FFmpegSwscale::outWidth()
{
    return out_frame_width;
}
