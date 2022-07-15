#include "FFmpegMediaScaler.h"

FFmpegMediaScaler::FFmpegMediaScaler()
{
    pAudioResamle = nullptr;
    pAudioOutBuffer = nullptr;
    pVideoScale = nullptr;
    pVideoOutFrame = nullptr;
    pVideoOutBuffer = nullptr;

    dst_audio_fmt = AV_SAMPLE_FMT_S16;
    dst_audio_rate = 48000;
    dst_audio_chly = AV_CH_LAYOUT_STEREO;

    dst_video_width = 1280;
    dst_video_height = 720;
    dst_video_fmt = AV_PIX_FMT_YUV420P;

    _src_audio_fmt = AV_SAMPLE_FMT_NONE;
    _src_audio_rate = 0;
    _src_audio_chly = 0;
    _dst_audio_fmt = AV_SAMPLE_FMT_NONE;
    _dst_audio_rate = 0;
    _dst_audio_chly = 0;

    _src_video_fmt = AV_PIX_FMT_NONE;
    _src_video_width = 0;
    _src_video_height = 0;
    _dst_video_fmt = AV_PIX_FMT_NONE;
    _dst_video_width = 0;
    _dst_video_height = 0;
}

FFmpegMediaScaler::~FFmpegMediaScaler()
{
    freeAudioResample();
    freeVideoScale();
}

enum AVSampleFormat FFmpegMediaScaler::srcAudioFmt()
{
    return _src_audio_fmt;
}

int FFmpegMediaScaler::srcAudioRate()
{
    return _src_audio_rate;
}

int FFmpegMediaScaler::srcAudioChannels()
{
    return av_get_channel_layout_nb_channels(_src_audio_chly);
}

enum AVSampleFormat FFmpegMediaScaler::outAudioFmt()
{
    return _dst_audio_fmt;
}

int FFmpegMediaScaler::outAudioRate()
{
    return _dst_audio_rate;
}

int FFmpegMediaScaler::outAudioChannels()
{
    return av_get_channel_layout_nb_channels(_dst_audio_chly);
}

enum AVPixelFormat FFmpegMediaScaler::srcVideoFmt()
{
    return _src_video_fmt;
}

int FFmpegMediaScaler::srcVideoHeight()
{
    return _src_video_height;
}

int FFmpegMediaScaler::srcVideoWidth()
{
    return _src_video_width;
}

enum AVPixelFormat FFmpegMediaScaler::outVideoFmt()
{
    return _dst_video_fmt;
}

int FFmpegMediaScaler::outVideoHeight()
{
    return _dst_video_height;
}

int FFmpegMediaScaler::outVideoWidth()
{
    return _dst_video_width;
}

void FFmpegMediaScaler::setOutAudio(enum AVSampleFormat fmt, int rate, int channels)
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);

    dst_audio_fmt = fmt;
    dst_audio_rate = rate;
    dst_audio_chly = av_get_default_channel_layout(channels);

    if(isAudioReady()) {
        AVSampleFormat src_audio_fmt = _src_audio_fmt;
        int src_audio_rate = _src_audio_rate;
        uint64_t src_audio_chly = _src_audio_chly;
        freeAudioResample();
        initAudioResample2(src_audio_fmt, src_audio_rate, src_audio_chly,
                          dst_audio_fmt, dst_audio_rate, dst_audio_chly);
    }
}

void FFmpegMediaScaler::setOutVideo(enum AVPixelFormat fmt, int width, int height)
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);

    dst_video_width = width;
    dst_video_height = height;
    dst_video_fmt = fmt;

    if(isAudioReady()) {
        AVPixelFormat src_video_fmt = _src_video_fmt;
        int src_video_width = _src_video_width;
        int src_video_height = _src_video_height;
        freeVideoScale();
        initVideoScale2(src_video_fmt, src_video_width, src_video_height,
                          dst_video_fmt, dst_video_width, dst_video_height);
    }
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

int FFmpegMediaScaler::initAudioResample(AVCodecContext *pCodeCtx)
{
    const int64_t ly = pCodeCtx->channels?av_get_default_channel_layout(pCodeCtx->channels):pCodeCtx->channel_layout;
    return initAudioResample2(pCodeCtx->sample_fmt, pCodeCtx->sample_rate, ly,
                      dst_audio_fmt, dst_audio_rate, dst_audio_chly);
}

int FFmpegMediaScaler::initAudioResample(enum AVSampleFormat in_sample_fmt,
                                         int in_rate, int in_channels,
                                         enum AVSampleFormat out_sample_fmt,
                                         int out_rate, int out_channels) {
    const uint64_t in_ch_layout = av_get_default_channel_layout(in_channels);
    const uint64_t out_ch_layout = av_get_default_channel_layout(out_channels);
    return initAudioResample2(in_sample_fmt, in_rate, in_ch_layout,
                       out_sample_fmt, out_rate, out_ch_layout);
}

int FFmpegMediaScaler::initAudioResample2(enum AVSampleFormat in_sample_fmt,
                                          int in_rate, uint64_t in_ch_layout,
                                          enum AVSampleFormat out_sample_fmt,
                                          int out_rate, uint64_t out_ch_layout)
{
    std::lock_guard<std::recursive_mutex> lk(audioMutex);

    int ret =0;

    FFmpegSwresample *p = new FFmpegSwresample();
    ret = p->init(in_sample_fmt, in_rate, in_ch_layout,
                  out_sample_fmt, out_rate, out_ch_layout);
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

    dst_audio_fmt = out_sample_fmt;
    dst_audio_rate = out_rate;
    dst_audio_chly = out_ch_layout;

    _src_audio_fmt = in_sample_fmt;
    _src_audio_rate = in_rate;
    _src_audio_chly = in_ch_layout;
    _dst_audio_fmt = out_sample_fmt;
    _dst_audio_rate = out_rate;
    _dst_audio_chly = out_ch_layout;

    return 0;
}

int FFmpegMediaScaler::initVideoScale(AVCodecContext *pCodecCtx)
{
    return initVideoScale(pCodecCtx->pix_fmt,pCodecCtx->width,pCodecCtx->height);
}

int FFmpegMediaScaler::initVideoScale(enum AVPixelFormat in_pixel_fmt, int in_width, int in_height)
{
    return initVideoScale2(in_pixel_fmt, in_width, in_height,
                           dst_video_fmt, dst_video_width, dst_video_height);
}

int FFmpegMediaScaler::initVideoScale2(enum AVPixelFormat in_pixel_fmt,
                    int in_width, int in_height,
                    enum AVPixelFormat out_pixel_fmt,
                    int out_width, int out_height)
{
    std::lock_guard<std::recursive_mutex> lk(videoMutex);

    int ret = 0;
    FFmpegSwscale *p = new FFmpegSwscale();
    ret = p->init(in_pixel_fmt, in_width, in_height,
                  dst_video_fmt, dst_video_width, dst_video_height);
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

    dst_video_fmt = out_pixel_fmt;
    dst_video_width = out_width;
    dst_video_height = out_height;

    _src_video_fmt = in_pixel_fmt;
    _src_video_width = in_width;
    _src_video_height = in_height;
    _dst_video_fmt = out_pixel_fmt;
    _dst_video_width = out_width;
    _dst_video_height = out_height;

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
