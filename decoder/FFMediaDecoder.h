#ifndef FFMEDIADECODER_H
#define FFMEDIADECODER_H

#include <list>
#include "common/ffmpeg_commons.h"

extern "C" {
//封装格式
#include "libavformat/avformat.h"
//解码
#include "libavcodec/avcodec.h"
//工具
#include "libavutil/error.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
}

class FFMediaDecoder
{
public:
    FFMediaDecoder();
    virtual ~FFMediaDecoder();

    int open(const char* input, AVDictionary *dict, bool hwaccels);
    void close();
    int decoding();

    bool isHwaccels();
    const char* inputfile();

    int64_t diffLastFrame(int64_t current);
    void setInterruptTimeout(const int microsecond);
    const int interruptTimeout();
    void stopInterrupt();

    const AVFormatContext* formatContext();
    const AVCodecContext* audioCodecContext();
    const AVCodecContext* videoCodecContext();
    const AVStream* audioStream();
    const AVStream* videoStream();

    void getSrcAudioParams(enum AVSampleFormat *fmt,
                           int *rate, int *channels);
    void getSrcVideoParams(enum AVPixelFormat *fmt,
                           int *width, int *height);

    void _print_error(const char* message);
    AVPixelFormat _hw_pix_fmt();

public:
    virtual void printInfo(const char* msg);
    virtual void printError(const char* msg);

    virtual int audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    virtual int videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);

    virtual void audioDecodedData(AVFrame *frame, AVPacket *packet) =0;
    virtual void videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight) =0;

private:
    AVFormatContext* pFormatCtx;
    AVCodecContext *pAudioCodecCtx, *pVideoCodecCtx;
    AVBufferRef *hw_device_ctx;
    AVPixelFormat hw_pix_fmt;
    int audio_stream_idx, video_stream_idx;

    AVPacket *mAVPacket;
    AVFrame *mAVFrame;
    AVFrame *mHwFrame;

    char mInputUrl[256];
    bool mIsHwaccels;
    int64_t lastFrameRealtime;
    int mInterruptTimeout;

    int initAudioCodec(AVFormatContext *pFormatCtx, int stream_idx);
    int initVideoCodec(AVFormatContext *pFormatCtx, int stream_idx);
    int openCodec(AVFormatContext *pFormatCtx,
                  int stream_idx, AVCodecContext **pCodeCtx);
    int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type);
    const AVCodecHWConfig *find_hwaccel(const AVCodec *codec, enum AVPixelFormat pix_fmt);
    int openHwCodec(AVFormatContext *pFormatCtx,
                int stream_idx, AVCodecContext **pCodeCtx);
    int decode_video_frame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    int decode_video_frame_hw(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);

    void print_info(const char* fmt, ...);
    void print_error(const char* fmt, ...);
    void print_averror(const char *name, int averr);

    void printCodecInfo(AVCodecContext *pCodeCtx);

};

#endif // FFMEDIADECODER_H
