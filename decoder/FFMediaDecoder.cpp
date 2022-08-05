#include "FFMediaDecoder.h"

#define INTERRUPT_TIMEOUT (10 * 1000 * 1000)

/*
 * FFmpeg 示例硬件解码hw_decode
 * https://www.jianshu.com/p/3ea9ef713211
 * Access the power of hardware accelerated video codecs in your Windows applications via FFmpeg / libavcodec
 * https://habr.com/en/company/intel/blog/575632/
*/

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{

    if(ctx && ctx->opaque) {
        const enum AVPixelFormat *p;
        FFMediaDecoder *self = static_cast<FFMediaDecoder*>(ctx->opaque);
        AVPixelFormat hw_pix_fmt = self? self->_hw_pix_fmt(): AV_PIX_FMT_NONE;
        for (p = pix_fmts; *p != -1; p++) {
            if (*p == hw_pix_fmt)
                return *p;
        }
    }

    //fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

static int interrupt_callback(void *pCtx)
{
    FFMediaDecoder *pThis = (FFMediaDecoder*)pCtx;
    if(pThis->diffLastFrame(av_gettime()) > pThis->interruptTimeout()){ //INTERRUPT_TIMEOUT
        pThis->_print_error("(interrupt_callback) media stream has been disconnected!");
        return AVERROR_EOF;
    }
    return 0;
}

FFMediaDecoder::FFMediaDecoder()
{
    pFormatCtx = nullptr;
    pAudioCodecCtx = nullptr;
    pVideoCodecCtx = nullptr;
    hw_device_ctx = nullptr;
	mAVFrame = nullptr;
	mAVPacket = nullptr;

    audio_stream_idx = -1;
    video_stream_idx = -1;

    mIsHwaccels = false;
    lastFrameRealtime = 0;
    mInterruptTimeout = INTERRUPT_TIMEOUT;
    memset(mInputUrl, 0 ,sizeof(mInputUrl));
}

FFMediaDecoder::~FFMediaDecoder()
{

}

int FFMediaDecoder::decode_video_frame_hw(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int ret = 0;
    AVFrame *sw_frame = NULL;

    if (!(sw_frame = av_frame_alloc())) {
        print_averror("Can not alloc frame", -1);
        return ret;
    }

    ret = avcodec_send_packet(pCodecCtx, packet);
    if (ret < 0) {
        print_averror("avcodec_send_packet", ret);
        return ret;
    }

    while (1) {
        int errcode = avcodec_receive_frame(pCodecCtx, sw_frame);
        if (errcode == AVERROR(EAGAIN) || errcode == AVERROR_EOF) {
            break;
        } else if (errcode < 0) {
            ret = errcode;
            print_averror("Error while decoding", ret);
            break;
        }

        if(!sws_isSupportedInput((AVPixelFormat)sw_frame->format)) {
        //if(frame->format == hw_pix_fmt) {
            /* retrieve data from GPU to CPU */
            if ((ret = av_hwframe_transfer_data(frame, sw_frame, 0)) < 0) {
                print_averror("Error transferring the data to system memory", ret);
                break;
            }
        } else {
            av_frame_copy(frame, sw_frame);
            av_frame_ref(frame, sw_frame);
        }

        videoDecodedData(frame, packet, pCodecCtx->height);
        if(frame->decode_error_flags) {
            print_error("video decode_error_flags:%d", frame->decode_error_flags);
        }
        av_frame_unref(frame);
    }

    if(sw_frame)
        av_frame_free(&sw_frame);

    return ret;
}
int FFMediaDecoder::decode_video_frame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int ret = 0;

    ret = avcodec_send_packet(pCodecCtx, packet);
    if (ret < 0) {
        print_averror("avcodec_decode_video2", ret);
        return ret;
    }

    while (1) {
        int errcode = avcodec_receive_frame(pCodecCtx, frame);
        if (errcode != 0) {
            break;
        }

        videoDecodedData(frame, packet, pCodecCtx->height);
        if(frame->decode_error_flags) {
            print_error("video decode_error_flags:%d", frame->decode_error_flags);
        }
    }

    return ret;
}

int FFMediaDecoder::hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                      NULL, NULL, 0)) < 0) {
        print_error("Failed to create specified HW device.");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    hw_formats.clear();

    AVHWFramesConstraints* hw_frames_const = av_hwdevice_get_hwframe_constraints(hw_device_ctx, NULL);
    if (hw_frames_const) {
        for (AVPixelFormat* p = hw_frames_const->valid_hw_formats;
            *p != AV_PIX_FMT_NONE; p++) {
            hw_formats.push_back(*p);
            //qDebug()<<"hw_formats"<<QString::fromLocal8Bit(av_get_pix_fmt_name(*p));
        }
        for (AVPixelFormat* p = hw_frames_const->valid_sw_formats;
            *p != AV_PIX_FMT_NONE; p++) {
            //qDebug()<<"sw_formats"<<QString::fromLocal8Bit(av_get_pix_fmt_name(*p));
        }
        av_hwframe_constraints_free(&hw_frames_const);
    } else {
        //qDebug()<<"hw_frames_const is null"<<type;
    }

    return err;
}

const AVCodecHWConfig *FFMediaDecoder::find_hwaccel(const AVCodec *codec, enum AVPixelFormat pix_fmt)
{
    std::list<enum AVHWDeviceType> typeList;
    std::list<std::string> typeNameList;
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
        typeList.push_back(type);
        const char *type_name = av_hwdevice_get_type_name(type);
        if(type_name)
            typeNameList.push_back(type_name);
    }

    for (int i=0;;i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
        if (!config) {
            print_error("Decoder does not support hw device");
            return NULL;
        }

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    config->device_type == type) {
                    hw_pix_fmt = config->pix_fmt;
                    break;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
            for(std::list<AVHWDeviceType>::iterator iter=typeList.begin();
                iter!=typeList.end();iter++) {
                if(*iter==config->device_type)
                    return config;
            }
        }
    }

    return NULL;
}

int FFMediaDecoder::openHwCodec(AVFormatContext *pFormatCtx, int stream_idx,
                                    AVCodecContext **pCodeCtx)
{
    int ret = 0;

    AVCodecParameters *pCodecPar = pFormatCtx->streams[stream_idx]->codecpar;
    if (!(*pCodeCtx))
        avcodec_free_context(pCodeCtx);

    AVCodec *pCodec = avcodec_find_decoder(pCodecPar->codec_id);
    if (pCodec == NULL) {
        print_error("avcodec_find_decoder is null");
        return -1;
    }

    const AVCodecHWConfig* config = find_hwaccel(pCodec, (enum AVPixelFormat)pCodecPar->format);
    if(config == NULL) {
        print_error("avcodec_find_decoder is null");
        return -1;
    }

    print_info("find_hwaccel: fmt %d %s", config->pix_fmt, av_hwdevice_get_type_name(config->device_type));

    *pCodeCtx = avcodec_alloc_context3(pCodec);
    if (*pCodeCtx == NULL) {
        print_error("avcodec_alloc_context3 failed");
        return -1;
    }

    if ((ret=avcodec_parameters_to_context(*pCodeCtx, pCodecPar)) < 0) {
        print_averror("avcodec_parameters_to_context", ret);
        return ret;
    }

//    (*pCodeCtx)->get_format  = get_hw_format;
//    (*pCodeCtx)->opaque = this;

    if ((ret=hw_decoder_init(*pCodeCtx, config->device_type)) < 0) {
        print_averror("Failed to init specified HW device", ret);
        return -1;
    }

    ret = avcodec_open2(*pCodeCtx, pCodec, NULL);
    if (ret < 0) {
        print_averror("failed to open codec", ret);
        return ret;
    }

    return ret;
}

int FFMediaDecoder::openCodec(AVFormatContext *pFormatCtx, int stream_idx,
                                  AVCodecContext **pCodeCtx)
{
    int ret =0;
    //4.获取解码器
    //根据索引拿到对应的流,根据流拿到解码器上下文
    //*pCodeCtx = pFormatCtx->streams[stream_idx]->codec;
    //再根据上下文拿到编解码id，通过该id拿到解码器
    //AVCodec *pCodec = avcodec_find_decoder((*pCodeCtx)->codec_id);

    AVCodecParameters *pCodecPar = pFormatCtx->streams[stream_idx]->codecpar;
    AVCodec *pCodec = avcodec_find_decoder(pCodecPar->codec_id);
    if(!(*pCodeCtx))
        avcodec_free_context(pCodeCtx);
    *pCodeCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(*pCodeCtx, pCodecPar);

    if (*pCodeCtx == NULL) {
        print_error("pCodeCtx is null");
        return -1;
    }
    if (pCodec == NULL) {
        print_error("pCodec is null");
        return -1;
    }
    //5.打开解码器
    ret = avcodec_open2(*pCodeCtx, pCodec, NULL);
    if (ret < 0) {
        print_averror("avcodec_open2", ret);
        return ret;
    }

    return ret;
}

int FFMediaDecoder::initAudioCodec(AVFormatContext *pFormatCtx, int stream_idx)
{
    int ret = 0;
    //打开解码器
    ret = openCodec(pFormatCtx, stream_idx, &pAudioCodecCtx);
    if(ret<0) {
        print_error("openCodec failed");
        return ret;
    }

    return ret;
}

int FFMediaDecoder::initVideoCodec(AVFormatContext *pFormatCtx, int stream_idx)
{
    int ret = 0;
    //打开解码器
    if(mIsHwaccels) {
        ret = openHwCodec(pFormatCtx, stream_idx, &pVideoCodecCtx);
        if(ret<0) {
            mIsHwaccels = false;
        }
    }
    if(!mIsHwaccels) {
        ret = openCodec(pFormatCtx, stream_idx, &pVideoCodecCtx);
    }

    print_info("mIsHwaccels: %s", (mIsHwaccels?"True":"False"));

    if(ret<0) {
        print_error("openCodec failed");
        return ret;
    }

    return ret;
}

int FFMediaDecoder::open(const char* input,
                             AVDictionary *dict, bool hwaccels)
{
    int ret =0;

    if(!pFormatCtx)
        pFormatCtx = avformat_alloc_context();

    pFormatCtx->interrupt_callback.callback = interrupt_callback;
    pFormatCtx->interrupt_callback.opaque = this;
    lastFrameRealtime = av_gettime();

    mIsHwaccels = hwaccels;

    AVDictionary* options = NULL;
    if(dict)
        av_dict_copy(&options, dict, 0);

    //2.打开输入视频文件
    strncpy(mInputUrl, input, sizeof(mInputUrl));
    ret = avformat_open_input(&pFormatCtx, input, NULL, &options);
    av_dict_free(&options);
    if (ret < 0) {
        print_averror("avformat_open_input", ret);
        return ret;
    }

    //3.获取视频文件信息
    ret = avformat_find_stream_info(pFormatCtx,NULL);
    if (ret < 0) {
        print_averror("avformat_find_stream_info", ret);
        return ret;
    }

    //获取视频流的索引位置
    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流
    audio_stream_idx = -1;
    video_stream_idx = -1;
    //number of streams
    int audio_stream_count = 0;
    int video_stream_count = 0;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        //流的类型
        enum AVMediaType codec_type = pFormatCtx->streams[i]->codecpar->codec_type;
        if(codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_count++;
            if(audio_stream_idx==-1) {
                audio_stream_idx = i;
                ret = initAudioCodec(pFormatCtx, i);
            }
        } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_count++;
            if(video_stream_idx==-1) {
                video_stream_idx = i;
                ret = initVideoCodec(pFormatCtx, i);
            }
        }
        av_dump_format(pFormatCtx, i, input, 0);
    }
    if(audio_stream_count>1) {
        print_error("find more %d audio stream", audio_stream_count);
    }
    if(video_stream_count>1) {
        print_error("find more %d video stream", audio_stream_count);
    }

    if(!mAVFrame)
        mAVFrame = av_frame_alloc();

    if(!mAVPacket)
        mAVPacket = av_packet_alloc();

    return ret;
}

void FFMediaDecoder::close()
{
    if(pAudioCodecCtx) {
        avcodec_close(pAudioCodecCtx);
        avcodec_free_context(&pAudioCodecCtx);
        pAudioCodecCtx = nullptr;
    }
    if(pVideoCodecCtx) {
        avcodec_close(pVideoCodecCtx);
        avcodec_free_context(&pVideoCodecCtx);
        pVideoCodecCtx = nullptr;
    }
    if(pFormatCtx) {
        avformat_close_input(&pFormatCtx);
        pFormatCtx = nullptr;
    }
    if(hw_device_ctx) {
        av_buffer_unref(&hw_device_ctx);
    }

    if(mAVFrame) {
        av_frame_free(&mAVFrame);
    }
    if(mAVPacket) {
        av_packet_free(&mAVPacket);
    }
}

int FFMediaDecoder::decoding()
{
    if(!pFormatCtx) {
        printf("pFormatCtx is null");
        return -1;
    }

    if(!mAVFrame) {
        printf("mAVFrame is null");
        return -1;
    }

    if(!mAVPacket) {
        printf("mAVPacket is null");
        return -1;
    }

    //6.一帧一帧读取压缩的音频数据AVPacket
    int ret = 0;
    ret = av_read_frame(pFormatCtx, mAVPacket);
    if(ret < 0) {
        print_averror("av_read_frame", ret);
        return ret;
    }

    lastFrameRealtime = av_gettime();

    //解码
    if (mAVPacket->stream_index == audio_stream_idx) {
        ret = audioRawFrame(pAudioCodecCtx, mAVFrame, mAVPacket);
    } else if(mAVPacket->stream_index == video_stream_idx) {
        ret = videoRawFrame(pVideoCodecCtx, mAVFrame, mAVPacket);
    } else {
        //print_error("unsupport stream index %d", mAVPacket->stream_index);
    }

    //释放资源
    av_packet_unref(mAVPacket);

    return ret;
}

const AVFormatContext* FFMediaDecoder::formatContext()
{
    return pFormatCtx;
}

const AVCodecContext * FFMediaDecoder::audioCodecContext()
{
    return pAudioCodecCtx;
}

const AVCodecContext * FFMediaDecoder::videoCodecContext()
{
    return pVideoCodecCtx;
}

const AVStream* FFMediaDecoder::audioStream()
{
    if(pFormatCtx && pFormatCtx->streams && audio_stream_idx!=-1) {
        return pFormatCtx->streams[audio_stream_idx];
    }
    return nullptr;
}

const AVStream* FFMediaDecoder::videoStream()
{
    if(pFormatCtx && pFormatCtx->streams && video_stream_idx!=-1) {
        return pFormatCtx->streams[video_stream_idx];
    }
    return nullptr;
}

void FFMediaDecoder::getSrcAudioParams(enum AVSampleFormat *fmt,
                                           int *rate, int *channels)
{
    const AVCodecContext* codec = audioCodecContext();

    if(fmt)
        *fmt = codec? codec->sample_fmt: AV_SAMPLE_FMT_NONE;
    if(rate)
        *rate = codec? codec->sample_rate: 0;
    if(channels) {
        if(codec) {
            int ch = av_get_channel_layout_nb_channels(codec->channel_layout);
            *channels = codec->channels? codec->channels: ch;
        } else {
            *channels = 0;
        }
    }
}

void FFMediaDecoder::getSrcVideoParams(enum AVPixelFormat *fmt,
                                           int *width, int *height)
{
    const AVCodecContext* codec = videoCodecContext();

    if(fmt)
        *fmt = codec? codec->pix_fmt: AV_PIX_FMT_NONE;
    if(width)
        *width = codec? codec->width: 0;
    if(height)
        *height = codec? codec->height: 0;
}

int64_t FFMediaDecoder::diffLastFrame(int64_t current)
{
    return (current - lastFrameRealtime);
}

void FFMediaDecoder::stopInterrupt()
{
    lastFrameRealtime = 0;
}

void FFMediaDecoder::setInterruptTimeout(const int microsecond)
{
    mInterruptTimeout = microsecond;
}

const int FFMediaDecoder::interruptTimeout()
{
    return mInterruptTimeout;
}

bool FFMediaDecoder::isHwaccels()
{
    return mIsHwaccels;
}

AVPixelFormat FFMediaDecoder::_hw_pix_fmt()
{
    return hw_pix_fmt;
}

const char* FFMediaDecoder::inputfile()
{
    if(pFormatCtx&&pFormatCtx->url)
        strncpy(mInputUrl, pFormatCtx->url, sizeof(mInputUrl));
    else
        memset(mInputUrl, 0, sizeof(mInputUrl));

    return mInputUrl;
}

int FFMediaDecoder::audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int ret;

    ret = avcodec_send_packet(pCodecCtx, packet);
    if (ret < 0) {
        print_averror("avcodec_decode_audio4", ret);
        return ret;
    }

    while (1) {
        int errcode = avcodec_receive_frame(pCodecCtx, frame);
        if (errcode < 0) {
            break;
        }

        audioDecodedData(frame, packet);
        if (frame->decode_error_flags) {
            print_error("audio decode_error_flags:%d", frame->decode_error_flags);
        }
    }

    return ret;
}

int FFMediaDecoder::videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    if(mIsHwaccels) {
        return decode_video_frame_hw(pCodecCtx, frame, packet);
    } else {
        return decode_video_frame(pCodecCtx, frame, packet);
    }
}

void FFMediaDecoder::printInfo(const char* msg)
{
    av_log(NULL, AV_LOG_INFO, msg);
}

void FFMediaDecoder::printError(const char* msg)
{
    av_log(NULL, AV_LOG_ERROR, msg);
}

void FFMediaDecoder::_print_error(const char* message)
{
    print_error(message);
}

void FFMediaDecoder::print_info(const char* fmt, ...)
{
    char printf_buf[512] = {0};
    va_list args;
    va_start(args, fmt);
    vsprintf(printf_buf, fmt, args);
    va_end(args);

    printInfo(printf_buf);
}

void FFMediaDecoder::print_error(const char* fmt, ...)
{
    char printf_buf[512] = {0};
    va_list args;
    va_start(args, fmt);
    vsprintf(printf_buf, fmt, args);
    va_end(args);

    printError(printf_buf);
}

void FFMediaDecoder::print_averror(const char *name, int averr)
{
    char errbuf[256];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(averr, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(averr));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", name, errbuf_ptr);

    print_error("(%s)%s", name, errbuf_ptr);
}


void FFMediaDecoder::printCodecInfo(AVCodecContext *pCodeCtx)
{
    if(pCodeCtx->codec_type==AVMEDIA_TYPE_AUDIO) {
        int64_t streamDuration = 0;
        if(audio_stream_idx>=0 && audio_stream_idx<pFormatCtx->nb_streams) {
             AVStream *stream = pFormatCtx->streams[audio_stream_idx];
             streamDuration = stream->duration * stream->time_base.num / stream->time_base.den;
        }
        //输出音频信息
        AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
        print_info("audio file format: %s",pFormatCtx->iformat->name);
        print_info("audio duration: %d", (pFormatCtx->duration!=AV_NOPTS_VALUE)?
                (pFormatCtx->duration/AV_TIME_BASE):0);
        print_info("audio stream duration: %d", streamDuration);
        print_info("audio channels: (lt:%d) %d", pCodeCtx->channel_layout, pCodeCtx->channels);
        print_info("audio sample rate: %d",pCodeCtx->sample_rate);
        print_info("audio sample accuracy: (fmt:%d) %d",pCodeCtx->sample_fmt,
               av_get_bytes_per_sample(pCodeCtx->sample_fmt)<<3);
        print_info("audio bit rate: %d",pCodeCtx->bit_rate);
        if(pCodec) {
            print_info("audio decode name: %s", pCodec->name);
        }
    } else if(pCodeCtx->codec_type==AVMEDIA_TYPE_VIDEO) {
        int64_t streamDuration = 0;
        if(video_stream_idx>=0 && video_stream_idx<pFormatCtx->nb_streams) {
             AVStream *stream = pFormatCtx->streams[video_stream_idx];
             streamDuration = stream->duration * stream->time_base.num / stream->time_base.den;
        }
        //输出视频信息
        AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
        print_info("video file format: %s",pFormatCtx->iformat->name);
        print_info("video duration: %d", (pFormatCtx->duration!=AV_NOPTS_VALUE)?
                (pFormatCtx->duration/AV_TIME_BASE):0);
        print_info("video stream duration: %d", streamDuration);
        print_info("video frame pixel format: %d", pCodeCtx->pix_fmt);
        print_info("video frame size: %d,%d",pCodeCtx->width, pCodeCtx->height);
        if(pCodec) {
            print_info("video decode name: %s", pCodec->name);
        }
        if(pCodeCtx->extradata&&pCodeCtx->extradata_size) {
            char *buf = (char*)av_malloc(pCodeCtx->extradata_size+1);
            if(buf) {
                memcpy(buf, pCodeCtx->extradata, pCodeCtx->extradata_size);
                buf[pCodeCtx->extradata_size] = '\0';
                print_info("video extradata: %s", buf);
                av_freep(&buf);
            }
        }
    }
}
