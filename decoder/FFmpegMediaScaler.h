#ifndef FFMPEGMEDIASCALER_H
#define FFMPEGMEDIASCALER_H

#include <mutex>
#include <memory>
#include "../scaler/FFmpegSwresample.h"
#include "../scaler/FFmpegSwscale.h"
#include "model/MediaData.h"

extern "C" {
//封装格式
#include "libavformat/avformat.h"
}

class FFmpegMediaScaler
{
public:
    explicit FFmpegMediaScaler();
    ~FFmpegMediaScaler();

    enum AVSampleFormat srcAudioFmt();
    int srcAudioRate();
    int srcAudioChannels();

    enum AVSampleFormat outAudioFmt();
    int outAudioRate();
    int outAudioChannels();

    enum AVPixelFormat srcVideoFmt();
    int srcVideoHeight();
    int srcVideoWidth();

    enum AVPixelFormat outVideoFmt();
    int outVideoHeight();
    int outVideoWidth();

    void setOutAudio(enum AVSampleFormat fmt, int rate, int channels);
    void setOutVideo(enum AVPixelFormat fmt, int width, int height);

    bool isAudioReady();

    bool isVideoReady();

    int initAudioResample(AVCodecContext *pCodeCtx);
    int initAudioResample(enum AVSampleFormat in_sample_fmt,
                          int in_rate, int in_channels,
                          enum AVSampleFormat out_sample_fmt,
                          int out_rate, int out_channels);
    int initAudioResample2(enum AVSampleFormat in_sample_fmt,
                          int in_rate, uint64_t in_ch_layout,
                          enum AVSampleFormat out_sample_fmt,
                          int out_rate, uint64_t out_ch_layout);

    int initVideoScale(AVCodecContext *pCodeCtx);
    int initVideoScale(enum AVPixelFormat in_pixel_fmt, int in_width, int in_height);
    int initVideoScale2(enum AVPixelFormat in_pixel_fmt,
                        int in_width, int in_height,
                        enum AVPixelFormat out_pixel_fmt,
                        int out_width, int out_height);

    void freeAudioResample();

    void freeVideoScale();

    uint8_t * audioConvert(AVFrame *frame, int *out_sp, int *out_sz);

    uint8_t * audioConvert(const uint8_t **in_buffer, int nb_samples,
                           int *out_sp, int *out_sz);

    AVFrame* videoScale(int pixelHeight, AVFrame *frame, int *out_h);

protected:
    virtual void printError(int code, const char* message);

private:
    //音频输出格式
    enum AVSampleFormat dst_audio_fmt, _src_audio_fmt;
    int dst_audio_rate, _src_audio_rate;
    uint64_t dst_audio_chly, _src_audio_chly;
    //视频输出格式
    int dst_video_width, _src_video_width;
    int dst_video_height, _src_video_height;
    enum AVPixelFormat dst_video_fmt, _src_video_fmt;
    //音频重采样
    FFmpegSwresample *pAudioResamle;
    AVFrame *pAudioOutFrame;
    uint8_t *pAudioOutBuffer;
    int audioOutBufferSize;
    //视频缩放
    FFmpegSwscale *pVideoScale;
    AVFrame *pVideoOutFrame;
    uint8_t *pVideoOutBuffer;
    int videoOutBufferSize;

    std::recursive_mutex audioMutex, videoMutex;

    void print_error(const char *name, int err);
};

#endif // FFMPEGMEDIASCALER_H
