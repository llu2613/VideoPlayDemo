#ifndef FFMPEGSWSCALE_H
#define FFMPEGSWSCALE_H

extern "C" {
//编码
#include "libavcodec/avcodec.h"
//封装格式处理
#include "libavformat/avformat.h"
//像素处理
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

class FFmpegSwscale
{
public:
    explicit FFmpegSwscale();
    ~FFmpegSwscale();

    int init(AVCodecContext *pCodecCtx,
                            int out_width, int out_height,
                            enum AVPixelFormat out_fmt);
    int init(enum AVPixelFormat in_pixel_fmt, int in_width, int in_height,
                            enum AVPixelFormat out_pixel_fmt, int out_width, int out_height);

    int mallocOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size);

    void freeOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size);

    int scale(AVFrame *pFrame, int srcSliceH, AVFrame *pOutFrame);

    void reset();

    enum AVPixelFormat inFormat();
    int inHeight();
    int inWidth();

    enum AVPixelFormat outFormat();
    int outHeight();
    int outWidth();

private:
    AVFormatContext *ctx;
    struct SwsContext *swsCtx;

    int in_frame_width, in_frame_height;
    int out_frame_width, out_frame_height;
    enum AVPixelFormat in_frame_fmt, out_frame_fmt;
    int out_count;

};

#endif // FFMPEGSWSCALE_H
