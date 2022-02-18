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
    static char str[AV_ERROR_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}

#endif // FFMPEGCOMMON_H
