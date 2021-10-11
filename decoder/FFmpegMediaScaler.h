#ifndef FFMPEGMEDIASCALER_H
#define FFMPEGMEDIASCALER_H

#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include "../scaler/FFmpegSwresample.h"
#include "../scaler/FFmpegSwscale.h"
#include "model/MediaData.h"

extern "C" {
//封装格式
#include "libavformat/avformat.h"
}

class FFmpegMediaScaler : public QObject
{
    Q_OBJECT
public:
    explicit FFmpegMediaScaler(QObject *parent = nullptr);

    enum AVPixelFormat outVideoFmt();
    int outAudioRate();
    int outAudioChannels();

    int outVideoHeight();
    int outVideoWidth();

    void setOutAudio(enum AVSampleFormat fmt, int rate, uint64_t ch_layout);
    void setOutVideo(enum AVPixelFormat fmt, int width, int height);

    bool isAudioReady();

    bool isVideoReady();

    int initAudioResample(AVCodecContext *pCodeCtx);

    int initVideoScale(AVCodecContext *pCodeCtx);

    void freeAudioResample();

    void freeVideoScale();

    uint8_t * audioConvert(AVFrame *frame, int *out_sp, int *out_s);

    AVFrame* videoScale(AVCodecContext *pCodecCtx, AVFrame *frame, int *out_h);

private:
    //音频输出格式
    enum AVSampleFormat out_audio_fmt;
    int out_audio_rate;
    uint64_t out_audio_ch;
    //视频输出格式
    int out_video_width;
    int out_video_height;
    enum AVPixelFormat out_video_fmt;
    //音频重采样
    FFmpegSwresample *pAudioResamle;
    uint8_t *pAudioOutBuffer;
    int audioOutBufferSize;
    //视频缩放
    FFmpegSwscale *pVideoScale;
    AVFrame *pVideoOutFrame;
    uint8_t *pVideoOutBuffer;
    int videoOutBufferSize;

    QMutex audioMutex, videoMutex;

    void printError(int code, const char* message);

signals:

public slots:
};

#endif // FFMPEGMEDIASCALER_H
