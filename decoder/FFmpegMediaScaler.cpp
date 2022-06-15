#include "FFmpegMediaScaler.h"

FFmpegMediaScaler::FFmpegMediaScaler()
{
    pAudioResamle = nullptr;
    pAudioOutBuffer = nullptr;
    pVideoScale = nullptr;
    pVideoOutFrame = nullptr;
    pVideoOutBuffer = nullptr;

    out_audio_fmt = AV_SAMPLE_FMT_S16;
    out_audio_rate = 48000;
    out_audio_ch = 2;

    out_video_width = 1280;
    out_video_height = 720;
    out_video_fmt = AV_PIX_FMT_YUV420P;
}

FFmpegMediaScaler::~FFmpegMediaScaler()
{
    freeAudioResample();
    freeVideoScale();
}

enum AVSampleFormat FFmpegMediaScaler::srcAudioFmt()
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);
    if(pAudioResamle)
        return pAudioResamle->inSampleFmt();
    return AV_SAMPLE_FMT_NONE;
}
int FFmpegMediaScaler::srcAudioRate()
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);
    if(pAudioResamle)
        return pAudioResamle->inSampleRate();
    return 0;
}
int FFmpegMediaScaler::srcAudioChannels()
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);
    if(pAudioResamle)
        return av_get_channel_layout_nb_channels(pAudioResamle->inChannelLayout());
    return 0;
}

enum AVSampleFormat FFmpegMediaScaler::outAudioFmt()
{
    return out_audio_fmt;
}
int FFmpegMediaScaler::outAudioRate()
{
    return out_audio_rate;
}

int FFmpegMediaScaler::outAudioChannels()
{
    return out_audio_ch;
}

enum AVPixelFormat FFmpegMediaScaler::srcVideoFmt()
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);
    if(pVideoScale)
        return pVideoScale->inPixelFmt();
    return AV_PIX_FMT_NONE;
}
int FFmpegMediaScaler::srcVideoHeight()
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);
    if(pVideoScale)
        return pVideoScale->inHeight();
    return 0;
}
int FFmpegMediaScaler::srcVideoWidth()
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);
    if(pVideoScale)
        return pVideoScale->inWidth();
    return 0;
}

enum AVPixelFormat FFmpegMediaScaler::outVideoFmt()
{
    return out_video_fmt;
}
int FFmpegMediaScaler::outVideoHeight()
{
    return out_video_height;
}
int FFmpegMediaScaler::outVideoWidth()
{
    return out_video_width;
}

void FFmpegMediaScaler::setOutAudio(enum AVSampleFormat fmt, int rate, int channels)
{
    out_audio_fmt = fmt;
    out_audio_rate = rate;
    out_audio_ch = channels;
}

void FFmpegMediaScaler::setOutVideo(enum AVPixelFormat fmt, int width, int height)
{
    out_video_width = width;
    out_video_height = height;
    out_video_fmt = fmt;
}

bool FFmpegMediaScaler::isAudioReady()
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);

    return pAudioResamle;
}

bool FFmpegMediaScaler::isVideoReady()
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);

    return pVideoScale;
}

int FFmpegMediaScaler::initAudioResample(enum AVSampleFormat in_fmt,
                                         int in_rate, int in_channels,
                                         enum AVSampleFormat out_fmt,
                                         int out_rate, int out_channels)
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);

    out_audio_fmt = out_fmt;
    out_audio_rate = out_rate;
    out_audio_ch = out_channels;

    int ret =0;
    const int in_ch_layout = av_get_default_channel_layout(in_channels);
    const int out_ch_layout = av_get_default_channel_layout(out_audio_ch);
    FFmpegSwresample *p = new FFmpegSwresample();
    ret = p->init(in_fmt, in_rate, in_ch_layout,
                  out_audio_fmt, out_audio_rate, out_ch_layout);
    if(ret<0) {
        print_error("initAudioResample", ret);
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

int FFmpegMediaScaler::initAudioResample(AVCodecContext *pCodeCtx)
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);

    int ret =0;
    FFmpegSwresample *p = new FFmpegSwresample();
    ret = p->init(pCodeCtx, out_audio_fmt, out_audio_rate,
                  av_get_default_channel_layout(out_audio_ch));
    if(ret<0) {
        print_error("initAudioResample", ret);
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

int FFmpegMediaScaler::initVideoScale(AVCodecContext *pCodecCtx)
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);

    return initVideoScale(pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height);
}

int FFmpegMediaScaler::initVideoScale(enum AVPixelFormat in_pixel_fmt, int in_width, int in_height)
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);

    int ret = 0;
    FFmpegSwscale *p = new FFmpegSwscale();
    ret = p->init(in_pixel_fmt, in_width, in_height,
                  out_video_fmt, out_video_width, out_video_height);
    if(ret<0) {
        print_error("initVideoScale", ret);
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
    std::lock_guard<std::recursive_mutex> lk(audioMutex);

    if(pAudioResamle) {
        pAudioResamle->freeOutBuffer(&pAudioOutBuffer, &audioOutBufferSize);
        delete pAudioResamle;
        pAudioResamle = nullptr;
    }
}

void FFmpegMediaScaler::freeVideoScale()
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);

    if(pVideoScale) {
        pVideoScale->freeOutFrame(&pVideoOutFrame, &pVideoOutBuffer, &videoOutBufferSize);
        delete pVideoScale;
        pVideoScale = nullptr;
    }
}

uint8_t * FFmpegMediaScaler::audioConvert(const uint8_t **in_buffer, int nb_samples,
                                          int *out_sp, int *out_sz)
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);

    if(!pAudioResamle) {
        printError(-1, "pAudioResamle is null");
        return nullptr;
    }

    int out_samples = pAudioResamle->convert(in_buffer, nb_samples,
                                             &pAudioOutBuffer, audioOutBufferSize);
    //获取sample的size
    int out_channels = av_get_channel_layout_nb_channels(pAudioResamle->outChannelLayout());
    int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_samples,
                                                     pAudioResamle->outSampleFmt(), 1);

    if(out_samples) {
        *out_sp = out_samples;
        *out_sz = out_buffer_size;
        return pAudioOutBuffer;
    }
    return nullptr;
}

uint8_t * FFmpegMediaScaler::audioConvert(AVFrame *frame, int *out_sp, int *out_sz)
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);

    if(!pAudioResamle) {
        printError(-1, "pAudioResamle is null");
        return nullptr;
    }

    int out_samples = pAudioResamle->convert(frame, &pAudioOutBuffer, audioOutBufferSize);
    //获取sample的size
    int out_channels = av_get_channel_layout_nb_channels(pAudioResamle->outChannelLayout());
    int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_samples,
            pAudioResamle->outSampleFmt(), 1);

    if(out_samples) {
        *out_sp = out_samples;
        *out_sz = out_buffer_size;
        return pAudioOutBuffer;
    }
    return nullptr;
}

AVFrame* FFmpegMediaScaler::videoScale(int pixelHeight, AVFrame *frame, int *out_h)
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);

    if(!pVideoScale) {
        printError(-1, "pVideoScale is null");
        return nullptr;
    }

    int out_height = pVideoScale->scale(frame, pixelHeight, pVideoOutFrame);

    if(out_height) {
        *out_h = out_height;
        return pVideoOutFrame;
    }
    return nullptr;
}

void FFmpegMediaScaler::printError(int code, const char* message)
{
    av_log(NULL, AV_LOG_ERROR,
           "FFmpegMediaScaler Error code:%d msg:%s\n", code, message);
}

void FFmpegMediaScaler::print_error(const char *name, int err)
{
    char errbuf[200], message[256];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", name, errbuf_ptr);

    sprintf(message, "(%s)%s", name, errbuf_ptr);
    printError(err, message);
}
