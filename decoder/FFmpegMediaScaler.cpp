#include "FFmpegMediaScaler.h"
#include <QDebug>

FFmpegMediaScaler::FFmpegMediaScaler(QObject *parent) : QObject(parent)
{
    pAudioResamle = nullptr;
    pAudioOutBuffer = nullptr;
    pVideoScale = nullptr;
    pVideoOutFrame = nullptr;
    pVideoOutBuffer = nullptr;

    out_audio_fmt = AV_SAMPLE_FMT_S16;
    out_audio_rate = 48000;
    out_audio_ch = AV_CH_LAYOUT_STEREO;

    out_video_width = 1280;
    out_video_height = 720;
    out_video_fmt = AV_PIX_FMT_YUV420P;
}

enum AVPixelFormat FFmpegMediaScaler::outVideoFmt()
{
    return out_video_fmt;
}

int FFmpegMediaScaler::outAudioRate()
{
    return out_audio_rate;
}

int FFmpegMediaScaler::outAudioChannels()
{
    return av_get_channel_layout_nb_channels(out_audio_ch);
}

int FFmpegMediaScaler::outVideoHeight()
{
    return out_video_height;
}

int FFmpegMediaScaler::outVideoWidth()
{
    return out_video_width;
}

void FFmpegMediaScaler::setOutAudio(enum AVSampleFormat fmt, int rate, uint64_t ch_layout)
{
    out_audio_fmt = fmt;
    out_audio_rate = rate;
    out_audio_ch = ch_layout;
}

void FFmpegMediaScaler::setOutVideo(enum AVPixelFormat fmt, int width, int height)
{
    out_video_width = width;
    out_video_height = height;
    out_video_fmt = fmt;
}

bool FFmpegMediaScaler::isAudioReady()
{
    QMutexLocker locker(&audioMutex);
    return pAudioResamle;
}

bool FFmpegMediaScaler::isVideoReady()
{
    QMutexLocker locker(&videoMutex);
    return pVideoScale;
}

int FFmpegMediaScaler::initAudioResample(AVCodecContext *pCodeCtx)
{
    QMutexLocker locker(&audioMutex);

    int ret =0;
    FFmpegSwresample *p = new FFmpegSwresample();
    ret = p->init(pCodeCtx, out_audio_fmt, out_audio_rate, out_audio_ch);
    if(ret<0) {
        printError(-1, "初始化重采样失败");
        delete p;
        return -1;
    }
    ret = p->mallocOutBuffer(&pAudioOutBuffer, &audioOutBufferSize);
    if(ret<0) {
        printError(-1, "申请内存失败");
        delete p;
        return -1;
    }

    pAudioResamle = p;
    return 0;
}

int FFmpegMediaScaler::initVideoScale(AVCodecContext *pCodeCtx)
{
    QMutexLocker locker(&videoMutex);

    int ret = 0;
    FFmpegSwscale *p = new FFmpegSwscale();
    ret = p->init(pCodeCtx, out_video_width, out_video_height, out_video_fmt);
    if(ret<0) {
        printError(-1, "初始化重缩放失败");
        delete p;
        return -1;
    }
    ret = p->mallocOutFrame(&pVideoOutFrame, &pVideoOutBuffer, &videoOutBufferSize);
    if(ret<0) {
        printError(-1, "申请内存失败");
        if(pVideoOutFrame) {
            av_frame_free(&pVideoOutFrame);
            pVideoOutFrame = nullptr;
        }
        delete p;
        return -1;
    }

    pVideoScale = p;
    return 0;
}

void FFmpegMediaScaler::freeAudioResample()
{
    QMutexLocker locker(&audioMutex);

    if(pAudioResamle) {
        pAudioResamle->freeOutBuffer(&pAudioOutBuffer, &audioOutBufferSize);
        delete pAudioResamle;
        pAudioResamle = nullptr;
    }
}

void FFmpegMediaScaler::freeVideoScale()
{
    QMutexLocker locker(&videoMutex);

    if(pVideoScale) {
        pVideoScale->freeOutFrame(&pVideoOutFrame, &pVideoOutBuffer, &videoOutBufferSize);
        delete pVideoScale;
        pVideoScale = nullptr;
    }
}


uint8_t * FFmpegMediaScaler::audioConvert(AVFrame *frame, int *out_sp, int *out_s)
{
    QMutexLocker locker(&audioMutex);

    if(!pAudioResamle) {
        printError(-1, "pAudioResamle is null");
        return nullptr;
    }

    int out_samples = pAudioResamle->convert(frame, pAudioOutBuffer, audioOutBufferSize);
    //获取sample的size
    int out_buffer_size = av_samples_get_buffer_size(NULL, pAudioResamle->outChannels(), out_samples,
            pAudioResamle->outSampleFmt(), 1);

    if(out_samples) {
        *out_sp = out_samples;
        *out_s = out_buffer_size;
        return pAudioOutBuffer;
    }
    return nullptr;
}

AVFrame* FFmpegMediaScaler::videoScale(AVCodecContext *pCodecCtx, AVFrame *frame, int *out_h)
{
    QMutexLocker locker(&videoMutex);

    if(!pVideoScale) {
        printError(-1, "pVideoScale is null");
        return nullptr;
    }

    int out_height = pVideoScale->scale(frame, pCodecCtx->height, pVideoOutFrame);

    if(out_height) {
        *out_h = out_height;
        return pVideoOutFrame;
    }
    return nullptr;
}

void FFmpegMediaScaler::printError(int code, const char* message)
{
    qDebug()<<QString("code:%1 msg:%2").arg(code).arg(QString::fromLocal8Bit(message));
}
