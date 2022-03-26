#ifndef FFMPEGCOMMON_H
#define FFMPEGCOMMON_H

#ifdef _WIN32
#ifdef FFMEDIACODEC_EXPORTS
#define FFMEDIACODEC_API __declspec(dllexport)
#else
#define FFMEDIACODEC_API __declspec(dllimport)
#endif
#else
#define FFMEDIACODEC_API
#endif

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/mathematics.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

//extern "C" {
//#include <string.h>
//#include "libavutil/error.h"
//}
#define FF_INVALID (-1)

inline char* wrap_av_err2str(int errnum)
{
    static char errbuf[AV_ERROR_MAX_STRING_SIZE];
    memset(errbuf, 0, sizeof(errbuf));
    if(av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE)<0) {
        return strerror(AVUNERROR(errnum));
    }
    return errbuf;
}
/*
int ff_print_frame(AVFrame *frame)
{
    uint8_t *buffer = NULL;
    int size;
    int ret = 0;

    size = av_image_get_buffer_size((AVPixelFormat)frame->format, frame->width,
                                    frame->height, 1);
    buffer = (uint8_t*)av_malloc(size);
    if (!buffer) {
        fprintf(stderr, "Can not alloc buffer\n");
        ret = AVERROR(ENOMEM);
        av_freep(&buffer);
        return -1;
    }
    ret = av_image_copy_to_buffer(buffer, size,
                                  (const uint8_t * const *)frame->data,
                                  (const int *)frame->linesize, (AVPixelFormat)frame->format,
                                  frame->width, frame->height, 1);
    if (ret < 0) {
        fprintf(stderr, "Can not copy image to buffer\n");
        av_freep(&buffer);
        return -1;
    }

    av_freep(&buffer);
    return 0;
}
*/
#endif // FFMPEGCOMMON_H
