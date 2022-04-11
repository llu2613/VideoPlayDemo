#ifndef FFMPEGMEDIADECODER_H
#define FFMPEGMEDIADECODER_H

#include <mutex>
#include <memory>
#include <list>
#include "FFmpegMediaScaler.h"
#include "model/MediaData.h"

extern "C" {
//封装格式
#include "libavformat/avformat.h"
//解码
#include "libavcodec/avcodec.h"
//工具
#include "libavutil/error.h"
#include "libavutil/time.h"
}

class FFmpegMediaDecoderCallback
{
public:
    virtual void onDecodeError(int code, std::string msg)=0;
    virtual void onAudioDataReady(std::shared_ptr<MediaData> data)=0;
    virtual void onVideoDataReady(std::shared_ptr<MediaData> data)=0;
};

class FFmpegMediaDecoder
{
public:
    enum Status{
        Closed=0, //关闭
        Ready,    //待取帧解码
        Broken,    //故障
        EndOfFile  //结束
    };
    explicit FFmpegMediaDecoder();
    virtual ~FFmpegMediaDecoder();

    int open(const char* input, bool hwaccels);
    int open(const char* input, AVDictionary *dict, bool hwaccels);
    void close();
    int decoding();

    bool isHwaccels();
    const char* inputfile();
    enum Status status();
    void setTag(const char* tag);
    const char* tag();

    void setOutAudio(enum AVSampleFormat fmt, int rate, int channels);
    void setOutVideo(enum AVPixelFormat fmt, int width, int height);

    void getSrcAudioParams(enum AVSampleFormat *fmt, int *rate, int *channels);
    void getOutAudioParams(enum AVSampleFormat *fmt, int *rate, int *channels);
    void getSrcVideoParams(enum AVPixelFormat *fmt, int *width, int *height);
    void getOutVideoParams(enum AVPixelFormat *fmt, int *width, int *height);

    void setCallback(FFmpegMediaDecoderCallback* callback);

    int64_t diffLastFrame(int64_t current);
    void setInterruptTimeout(const int microsecond);
    const int interruptTimeout();
    void stopInterrupt();

    void _setStatus(Status status);
    void _printError(int code, const char* message);

    const AVFormatContext* formatContext();
    const AVCodecContext* audioCodecContext();
    const AVCodecContext* videoCodecContext();
    const AVStream* audioStream();
    const AVStream* videoStream();

protected:
    virtual void printError(int code, const char* message);

    virtual int audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    virtual int videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);

    virtual void audioDecodedData(AVFrame *frame, AVPacket *packet);
    virtual void videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight);

    virtual void audioResampledData(AVPacket *packet, uint8_t *sampleBuffer,
                                    int bufferSize, int samples);
    virtual void videoScaledData(AVFrame *frame, AVPacket *packet, int pixelHeight);

    virtual void audioDataReady(std::shared_ptr<MediaData> data);
    virtual void videoDataReady(std::shared_ptr<MediaData> data);

private:
    AVFormatContext* pFormatCtx;
    AVCodecContext *pAudioCodecCtx, *pVideoCodecCtx;
    AVBufferRef *hw_device_ctx;
    std::list<enum AVPixelFormat> hw_formats;
    int audio_stream_idx, video_stream_idx;

    long audioFrameCnt, videoFrameCnt;
    unsigned long audio_pts, video_pts;

    FFmpegMediaScaler scaler;

    std::mutex mCallbackMutex;
    FFmpegMediaDecoderCallback *mCallback;

    //编码数据
    AVPacket *mAVPacket;
    //解压缩数据
    AVFrame *mAVFrame;

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

    void fillPixelRGB24(MediaData *mediaData, AVFrame *frame, int pixelWidth, int pixelHeight);
    void fillPixelYUV420P(MediaData *mediaData, AVFrame *frame, int pixelWidth, int pixelHeight);

    void printCodecInfo(AVCodecContext *pCodeCtx);

    void print_error(const char *name, int err);

    char mTag[256];
    Status mStatus;
    bool mIsHwaccels;
    int64_t lastFrameRealtime;
    int mInterruptTimeout;

};

#endif // FFMPEGMEDIADECODER_H
