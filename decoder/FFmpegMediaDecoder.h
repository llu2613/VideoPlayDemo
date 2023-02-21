#ifndef FFMPEGMEDIADECODER_H
#define FFMPEGMEDIADECODER_H

#include <mutex>
#include <memory>
#include <list>
#include "FFmpegMediaScaler.h"
#include "model/MediaData.h"
#include "FFMediaDecoder.h"

class FFmpegMediaDecoderCallback
{
public:
    virtual void onDecodeError(int code, std::string msg)=0;
    virtual void onAudioDataReady(std::shared_ptr<MediaData> data)=0;
    virtual void onVideoDataReady(std::shared_ptr<MediaData> data)=0;
};

class FFmpegMediaDecoder : public FFMediaDecoder
{
public:
    explicit FFmpegMediaDecoder();
    virtual ~FFmpegMediaDecoder();

    int open(const char* input, bool hwaccels);
    int open(const char* input, AVDictionary *dict, bool hwaccels);
    int decoding();
    void close();

    int seekto(double seconds);

    void setTag(const char* tag);
    const char* tag();

    void setOutAudio(enum AVSampleFormat fmt, int rate, int channels);
    void setOutVideo(enum AVPixelFormat fmt, int width, int height);

    void getOutAudioParams(enum AVSampleFormat *fmt, int *rate, int *channels);
    void getOutVideoParams(enum AVPixelFormat *fmt, int *width, int *height);

    void setCallback(FFmpegMediaDecoderCallback* callback);

protected:
    virtual void printInfo(const char* message) override;
    virtual void printError(const char* message) override;

    virtual int audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet) override;
    virtual int videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet) override;

    virtual void audioDecodedData(AVFrame *frame, AVPacket *packet) override;
    virtual void videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight) override;

    virtual void audioResampledData(AVFrame *frame, AVPacket *packet,
                                    uint8_t *sampleBuffer, int bufferSize, int samples);
    virtual void audioResampledData(AVPacket *packet, uint8_t *sampleBuffer,
                                    int bufferSize, int samples);
    virtual void videoScaledData(AVFrame *frame, AVPacket *packet, AVFrame *scaledFrame, int pixelHeight);
    virtual void videoScaledData(AVFrame *frame, AVPacket *packet, int pixelHeight);

    virtual void audioDataReady(std::shared_ptr<MediaData> data);
    virtual void videoDataReady(std::shared_ptr<MediaData> data);

private:
    int64_t video_first_pts, video_last_pts;
    int64_t audio_pts, video_pts;

    std::mutex scaler_mutex;
    FFmpegMediaScaler scaler;

    std::mutex mCallbackMutex;
    FFmpegMediaDecoderCallback *mCallback;

    void fillPixelRGB24(MediaData *mediaData, AVFrame *frame, int pixelWidth, int pixelHeight);
    void fillPixelYUV420P(MediaData *mediaData, AVFrame *frame, int pixelWidth, int pixelHeight);

    char mTag[64];
};

#endif // FFMPEGMEDIADECODER_H
