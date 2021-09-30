#include "FFmpegSwscale.h"



FFmpegSwscale::FFmpegSwscale(QObject *parent) : QObject(parent)
{
    swsCtx = nullptr;
}

FFmpegSwscale::~FFmpegSwscale()
{
    if(swsCtx)
        sws_freeContext(swsCtx);
}

int FFmpegSwscale::init(AVCodecContext *pCodecCtx,
                        int out_width, int out_height,
                        enum AVPixelFormat out_fmt)
{
    out_frame_width = out_width;
    out_frame_height = out_height;
    out_frame_fmt = out_fmt;

    swsCtx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,
                                                out_width, out_height, out_fmt,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    if(!swsCtx) {
        printf("swr_init failed");
        return -1;
    }

    //新数据长度
    out_count = avpicture_get_size(out_frame_fmt, out_frame_width, out_frame_height);

    return 0;
}

int FFmpegSwscale::mallocOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size)
{
    //内存分配
    *out_frame = av_frame_alloc();
    //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
    //缓冲区分配内存
    *out_buffer = (uint8_t *)av_malloc(out_count);

    if(!(*out_buffer)) {
        printf("no memory alloc to out_buffer");
        return -1;
    }

    *out_buffer_size = out_count;
    //初始化缓冲区
    av_image_fill_arrays((*out_frame)->data, (*out_frame)->linesize, *out_buffer,
                         out_frame_fmt, out_frame_width, out_frame_height, 1);

    return 0;
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
        printf("swsCtx is null");
        return -1;
    }

    if(!pOutFrame) {
        printf("pOutFrame is null");
        return -1;
    }

//    int out_height = sws_scale(swsCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
//                               pOutFrame->data, pOutFrame->linesize);
    int out_height = sws_scale(swsCtx, pFrame->data, pFrame->linesize, 0, srcSliceH,
                               pOutFrame->data, pOutFrame->linesize);

    return out_height;
}

int FFmpegSwscale::outHeight()
{
    return out_frame_height;
}

int FFmpegSwscale::outWidth()
{
    return out_frame_width;
}
