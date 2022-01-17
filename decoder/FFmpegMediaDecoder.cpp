#include "FFmpegMediaDecoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <QDebug>

#define LOG(...) qDebug(__VA_ARGS__)

//20s超时退出
#define INTERRUPT_TIMEOUT (10 * 1000 * 1000)

int interrupt_callback(void *pCtx)
{
    FFmpegMediaDecoder *pThis = (FFmpegMediaDecoder*)pCtx;
    if(pThis->diffLastFrame(av_gettime()) > pThis->interruptTimeout()){ //INTERRUPT_TIMEOUT
        pThis->_setStatus(FFmpegMediaDecoder::Broken);
        pThis->_printError(0, "(interruptCallback) media stream has been disconnected!");
        return AVERROR_EOF;
    }
    return 0;
}

FFmpegMediaDecoder::FFmpegMediaDecoder()
{
    pFormatCtx = nullptr;
    pAudioCodecCtx = nullptr;
    pVideoCodecCtx = nullptr;

    readFailedCnt = 0;
    audioFrameCnt = 0;
    videoFrameCnt = 0;
    audio_stream_idx = -1;
    video_stream_idx = -1;

    mCallback = nullptr;

    //编码数据
    mAVPacket = av_packet_alloc();
    //解压缩数据
    mAVFrame = av_frame_alloc();

    mStatus = Closed;
    mIsHwaccels = false;
    lastFrameRealtime = 0;
    mInterruptTimeout = INTERRUPT_TIMEOUT;
    memset(mTag, 0, sizeof(mTag));
}

FFmpegMediaDecoder::~FFmpegMediaDecoder()
{
    if(mAVFrame)
        av_frame_free(&mAVFrame);

    if(mAVPacket)
        av_packet_free(&mAVPacket);
}

void FFmpegMediaDecoder::getSrcAudioParams(enum AVSampleFormat *fmt,
                                           int *rate, int *channels)
{
    if(fmt)
        *fmt = scaler.srcAudioFmt();
    if(rate)
        *rate = scaler.srcAudioRate();
    if(channels)
        *channels = scaler.srcAudioChannels();
}

void FFmpegMediaDecoder::getOutAudioParams(enum AVSampleFormat *fmt,
                                           int *rate, int *channels)
{
    if(fmt)
        *fmt = scaler.outAudioFmt();
    if(rate)
        *rate = scaler.outAudioRate();
    if(channels)
        *channels = scaler.outAudioChannels();
}

void FFmpegMediaDecoder::getSrcVideoParams(enum AVPixelFormat *fmt,
                                           int *width, int *height)
{
    if(fmt)
        *fmt = scaler.outVideoFmt();
    if(width)
        *width = scaler.outVideoWidth();
    if(height)
        *height = scaler.outVideoHeight();
}

void FFmpegMediaDecoder::getOutVideoParams(enum AVPixelFormat *fmt,
                                           int *width, int *height)
{
    if(fmt)
        *fmt = scaler.outVideoFmt();
    if(width)
        *width = scaler.outVideoWidth();
    if(height)
        *height = scaler.outVideoHeight();
}

void FFmpegMediaDecoder::setOutAudio(enum AVSampleFormat fmt, int rate, int channels)
{
    if(pAudioCodecCtx && scaler.isAudioReady()) {
        scaler.freeAudioResample();
        scaler.setOutAudio(fmt, rate, channels);
        scaler.initAudioResample(pAudioCodecCtx);
    } else {
        scaler.setOutAudio(fmt, rate, channels);
    }
}

void FFmpegMediaDecoder::setOutVideo(enum AVPixelFormat fmt, int width, int height)
{
    if(pVideoCodecCtx && scaler.isVideoReady()) {
        scaler.freeVideoScale();
        scaler.setOutVideo(fmt, width, height);
        scaler.initVideoScale(pVideoCodecCtx);
    } else {
        scaler.setOutVideo(fmt, width, height);
    }
}

int FFmpegMediaDecoder::audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int ret, got_frame=0;
    //解码AVPacket->AVFrame
    //ret = avcodec_decode_audio4(pCodecCtx, frame, &got_frame, packet);
	ret = avcodec_send_packet(pCodecCtx, packet);
    if (ret < 0) {
        print_error("avcodec_decode_audio4", ret);
        return ret;
    }

    //非0，正在解码
    for (int i=0;i<100;i++) {
		ret = avcodec_receive_frame(pCodecCtx, frame);
		if (ret != 0) {
			break;
		}
		got_frame++;
		audioFrameCnt++;
		audioDecodedData(frame, packet);
		if (frame->decode_error_flags) {
			char tmp[256];
			sprintf(tmp, "audio decode_error_flags:%d", frame->decode_error_flags);
			printError(0, tmp);
		}
	}

    return got_frame;
}

int FFmpegMediaDecoder::videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int ret, got_picture=0;
    //7.解码一帧视频压缩数据，得到视频像素数据
    //ret = avcodec_decode_video2(pCodecCtx, frame, &got_picture, packet);
	ret = avcodec_send_packet(pCodecCtx, packet);
    if (ret < 0) {
        print_error("avcodec_decode_video2", ret);
        return ret;
    }

    //为0说明解码完成，非0正在解码
    for (int i=0;i<100;i++) {
		ret = avcodec_receive_frame(pCodecCtx, frame);
		if (ret != 0) {
			break;
		}
		got_picture++;
        videoFrameCnt++;
        videoDecodedData(frame, packet, pCodecCtx->height);
        if(frame->decode_error_flags) {
            char tmp[256];
            sprintf(tmp, "video decode_error_flags:%d", frame->decode_error_flags);
            printError(0, tmp);
        }
    }

    return got_picture;
}

void FFmpegMediaDecoder::audioDecodedData(AVFrame *frame, AVPacket *packet)
{
    int out_samples, out_buffer_size;
    uint8_t *pAudioOutBuffer = scaler.audioConvert(frame,
                                                   &out_samples,
                                                   &out_buffer_size);

    //写入文件进行测试
//        if(fp_pcm) {
//            fwrite(pAudioOutBuffer, 1, out_buffer_size, fp_pcm);
//        }

    audioResampledData(packet, pAudioOutBuffer, out_buffer_size, out_samples);
}

void FFmpegMediaDecoder::videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight)
{
    //AVFrame转为像素格式YUV420，宽高
    //2 6输入、输出数据
    //3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
    //4 输入数据第一列要转码的位置 从0开始
    //5 输入画面的高度
    int out_height;
    AVFrame* out_frame = scaler.videoScale(pixelHeight, frame, &out_height);

//    AVFrame *pFrameYUV = out_frame;
//        if(fp_yuv) {
//            //输出到YUV文件
//            //AVFrame像素帧写入文件
//            //data解码后的图像像素数据（音频采样数据）
//            //Y 亮度 UV 色度（压缩了） 人对亮度更加敏感
//            //U V 个数是Y的1/4
//            int y_size = pVideoScale->width() * pVideoScale->height();
//            fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);
//            fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);
//            fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);
//        }
    videoScaledData(out_frame, packet, out_height);
}

void FFmpegMediaDecoder::audioResampledData(AVPacket *packet, uint8_t *sampleBuffer,
	int bufferSize, int samples)
{
    try {
        std::shared_ptr<MediaData> mediaData(new MediaData);
        mediaData->sample_format = scaler.outAudioFmt();
        mediaData->nb_samples = samples;
        mediaData->sample_rate = scaler.outAudioRate();
        mediaData->channels = scaler.outAudioChannels();
        mediaData->pts = packet->pts;
        mediaData->duration = packet->duration;
        AVStream *stream = pFormatCtx->streams[packet->stream_index];
        mediaData->time_base_d = av_q2d(stream->time_base);
        //帧数据
        mediaData->data[0] = new unsigned char[bufferSize];
        mediaData->datasize[0] = bufferSize;
        memcpy(mediaData->data[0], sampleBuffer, bufferSize);
        audioDataReady(mediaData);
    } catch(...) {
        LOG("audioDecodedData failed share data!");
    }
}

void FFmpegMediaDecoder::videoScaledData(AVFrame *frame, AVPacket *packet, int pixelHeight)
{
    try {
        std::shared_ptr<MediaData> mediaData(new MediaData);
        mediaData->pixel_format = scaler.outVideoFmt();
        mediaData->width = scaler.outVideoWidth();
        mediaData->height = pixelHeight;
        mediaData->pts = packet->pts;
        mediaData->duration = packet->duration;
        mediaData->repeat_pict = frame->repeat_pict; //重复显示次数
        mediaData->best_timestamp = frame->best_effort_timestamp;
        mediaData->key_frame = (frame->key_frame==1||frame->pict_type==AV_PICTURE_TYPE_I);
        AVStream *stream = pFormatCtx->streams[packet->stream_index];
        mediaData->time_base_d = av_q2d(stream->time_base);
        mediaData->frame_rate = av_q2d(stream->avg_frame_rate);
        //一帧图像（音频）的时间戳（时间戳一般以第一帧为0开始）
        //时间戳 = pts * (AVRational.num/AVRational.den)
        //int second= pFrame->pts * av_q2d(stream->time_base);
        if(scaler.outVideoFmt()==AV_PIX_FMT_YUV420P) {
            fillPixelYUV420P(mediaData.get(), frame, scaler.outVideoWidth(), pixelHeight);
        } else if(scaler.outVideoFmt()==AV_PIX_FMT_RGB24) {
            fillPixelRGB24(mediaData.get(), frame, scaler.outVideoWidth(), pixelHeight);
        }
        videoDataReady(mediaData);
    } catch(...) {
        LOG("videoDecodedData failed share data!");
    }
}

void FFmpegMediaDecoder::fillPixelRGB24(MediaData *mediaData, AVFrame *frame,
                                        int pixelWidth, int pixelHeight)
{
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pixelWidth, pixelHeight, 0);
    mediaData->data[0] = new unsigned char[numBytes];
    mediaData->datasize[0] = numBytes;
    mediaData->linesize[0] = frame->linesize[0];
    memcpy(&(mediaData->data[0]), frame->data[0], numBytes);
}

void FFmpegMediaDecoder::fillPixelYUV420P(MediaData *mediaData, AVFrame *frame,
                                         int pixelWidth, int pixelHeight)
{
    int y_size = pixelWidth * pixelHeight;
    int dataSize[] = {y_size, y_size / 4, y_size / 4};
    for(int i=0; i<3; i++) {
        mediaData->data[i] = new unsigned char[dataSize[i]];
        mediaData->datasize[i] = dataSize[i];
        mediaData->linesize[i] = frame->linesize[i];
        memcpy(mediaData->data[i], frame->data[i], dataSize[i]);
    }
}

void FFmpegMediaDecoder::audioDataReady(std::shared_ptr<MediaData> data)
{
    std::lock_guard<std::mutex> lk(mCallbackMutex);

    if(mCallback)
        mCallback->onAudioDataReady(data);
}

void FFmpegMediaDecoder::videoDataReady(std::shared_ptr<MediaData> data)
{
    std::lock_guard<std::mutex> lk(mCallbackMutex);

    if(mCallback)
        mCallback->onVideoDataReady(data);
}

int FFmpegMediaDecoder::openCodec(AVFormatContext *pFormatCtx, int stream_idx,
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
		printError(-1, "无法解码(pCodeCtx)");
		return -1;
	}
    if (pCodec == NULL) {
        printError(-1, "无法解码(pCodec)");
        return -1;
    }
    //5.打开解码器
    ret = avcodec_open2(*pCodeCtx, pCodec, NULL);
    if (ret < 0) {
        printError(ret, "编码器无法打开");
        return ret;
    }

    return ret;
}

AVHWAccel *FFmpegMediaDecoder::findHwaccel(enum AVCodecID codec_id, enum AVPixelFormat pix_fmt)
{
	AVHWAccel *hwaccel = NULL;

	while ((hwaccel = av_hwaccel_next(hwaccel))) {
		if (hwaccel->id == codec_id
			&& hwaccel->pix_fmt == pix_fmt)
			return hwaccel;
	}
	return NULL;
}
int FFmpegMediaDecoder::openHwCodec(AVFormatContext *pFormatCtx, int stream_idx,
									AVCodecContext **pCodeCtx)
{
	int ret = 0;
	
	AVCodecParameters *pCodecPar = pFormatCtx->streams[stream_idx]->codecpar;
	if (!(*pCodeCtx))
		avcodec_free_context(pCodeCtx);

	findHwaccel(pCodecPar->codec_id, (enum AVPixelFormat)pCodecPar->format);
	//AVCodec *pCodec = avcodec_find_decoder_by_name("h264_qsv");//Intel核心显卡
	//if (!pCodec) pCodec = avcodec_find_decoder_by_name("h264_nvenc");//英伟达显卡
	AVCodec *pCodec = avcodec_find_decoder(pCodecPar->codec_id);
	if (pCodec == NULL) {
		printError(-1, "无法解码(pCodec)");
		return -1;
	}

	*pCodeCtx = avcodec_alloc_context3(pCodec);
	if (*pCodeCtx == NULL) {
		printError(-1, "无法解码(pCodeCtx)");
		return -1;
	}

	avcodec_parameters_to_context(*pCodeCtx, pCodecPar);
	//5.打开解码器
	ret = avcodec_open2(*pCodeCtx, pCodec, NULL);
	if (ret < 0) {
		printError(ret, "编码器无法打开");
		return ret;
	}

	return ret;
}

int FFmpegMediaDecoder::initAudioCodec(AVFormatContext *pFormatCtx, int stream_idx)
{
    int ret = 0;
    //打开解码器
    ret = openCodec(pFormatCtx, stream_idx, &pAudioCodecCtx);
    if(ret<0) {
        printError(ret, "音频解码器打开失败");
        return ret;
    }
    //重采样
    scaler.initAudioResample(pAudioCodecCtx);

    printCodecInfo(pAudioCodecCtx);

    return ret;
}

int FFmpegMediaDecoder::initVideoCodec(AVFormatContext *pFormatCtx, int stream_idx)
{
    int ret = 0;
    //打开解码器
    ret = openHwCodec(pFormatCtx, stream_idx, &pVideoCodecCtx);
    if(ret<0) {
        printError(ret, "视频解码器打开失败");
        return ret;
    }
    //重缩放
    scaler.initVideoScale(pVideoCodecCtx);

    printCodecInfo(pVideoCodecCtx);

    return ret;
}

int FFmpegMediaDecoder::open(const char* input, bool hwaccels)
{
    return open(input, NULL, hwaccels);
}

int FFmpegMediaDecoder::open(const char* input,
                             AVDictionary *dict, bool hwaccels)
{
    int ret =0;
    //封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    if(!pFormatCtx)
        pFormatCtx = avformat_alloc_context();

    //设置阻塞中断函数
    pFormatCtx->interrupt_callback.callback = interrupt_callback;
    pFormatCtx->interrupt_callback.opaque = this;
    lastFrameRealtime = av_gettime();

    AVDictionary* options = NULL;
    if(dict)
        av_dict_copy(&options, dict, 0);

    mStatus = Broken;

    //2.打开输入视频文件
    ret = avformat_open_input(&pFormatCtx, input, NULL, &options);
    if (ret != 0) {
        av_dict_free(&options);
        print_error("avformat_open_input", ret);
        printError(ret, "无法打开输入媒体文件");
        return ret;
    }
    av_dict_free(&options);

    //3.获取视频文件信息
    ret = avformat_find_stream_info(pFormatCtx,NULL);
    if (ret < 0) {
        printError(ret, "无法获取媒体文件信息");
        return ret;
    }

    //获取视频流的索引位置
    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流
    audio_stream_idx = -1;
    video_stream_idx = -1;
    //number of streams
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        //流的类型
		//pFormatCtx->streams[i]->codec->codec_type
		enum AVMediaType codec_type = pFormatCtx->streams[i]->codecpar->codec_type;
        if(codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            ret = initAudioCodec(pFormatCtx, i);
        } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            ret = initVideoCodec(pFormatCtx, i);
        }
    }

    readFailedCnt = 0;
    audioFrameCnt = 0;
    videoFrameCnt = 0;

    mStatus = Ready;

    return ret;
}

void FFmpegMediaDecoder::close()
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

    scaler.freeAudioResample();
    scaler.freeVideoScale();

    mStatus = Closed;
}

int FFmpegMediaDecoder::decoding()
{
    if(mStatus!=Ready) {
        printf("decoding, illegal status %d", mStatus);
        return -1;
    }

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
    int readRet = 0;
    readRet = av_read_frame(pFormatCtx, mAVPacket);
    if(readRet < 0) {
        if(AVERROR_EOF==readRet) {
            mStatus = EndOfFile;
            return readRet;
        }
        readFailedCnt++;
        print_error("av_read_frame", readRet);
        return readRet;
    }

    readFailedCnt = 0;
    lastFrameRealtime = av_gettime();

    //解码
    int retCode = 0;
    if (mAVPacket->stream_index == audio_stream_idx) {
        retCode = audioRawFrame(pAudioCodecCtx, mAVFrame, mAVPacket);
    } else if(mAVPacket->stream_index == video_stream_idx) {
        retCode = videoRawFrame(pVideoCodecCtx, mAVFrame, mAVPacket);
    }

    //释放资源
	av_packet_unref(mAVPacket);

    return retCode;
}

const AVFormatContext* FFmpegMediaDecoder::formatContext()
{
    return pFormatCtx;
}

const AVCodecContext * FFmpegMediaDecoder::audioCodecContext()
{
    return pAudioCodecCtx;
}

const AVCodecContext * FFmpegMediaDecoder::videoCodecContext()
{
    return pVideoCodecCtx;
}

const AVStream* FFmpegMediaDecoder::audioStream()
{
    if(audio_stream_idx!=-1) {
        return pFormatCtx->streams[audio_stream_idx];
    }
    return nullptr;
}

const AVStream* FFmpegMediaDecoder::videoStream()
{
    if(video_stream_idx!=-1) {
        return pFormatCtx->streams[video_stream_idx];
    }
    return nullptr;
}

void FFmpegMediaDecoder::setCallback(FFmpegMediaDecoderCallback* callback)
{
    std::lock_guard<std::mutex> lk(mCallbackMutex);
    mCallback = callback;
}

int64_t FFmpegMediaDecoder::diffLastFrame(int64_t current)
{
    return (current - lastFrameRealtime);
}

void FFmpegMediaDecoder::setInterruptTimeout(const int microsecond)
{
    mInterruptTimeout = microsecond;
}

const int FFmpegMediaDecoder::interruptTimeout()
{
    return mInterruptTimeout;
}

long FFmpegMediaDecoder::readFailedCount()
{
    return readFailedCnt;
}

bool FFmpegMediaDecoder::isHwaccels()
{
    return mIsHwaccels;
}

const char* FFmpegMediaDecoder::inputfile()
{
    char file[1024]={0};

    if(pFormatCtx&&pFormatCtx->url)
        memcpy(file, pFormatCtx->url, sizeof(file));

    return file;
}

enum FFmpegMediaDecoder::Status FFmpegMediaDecoder::status()
{
    return mStatus;
}

void FFmpegMediaDecoder::setTag(const char* tag)
{
    sprintf(mTag, "%s", tag);
}

const char* FFmpegMediaDecoder::tag()
{
    return mTag;
}

void FFmpegMediaDecoder::printCodecInfo(AVCodecContext *pCodeCtx)
{
    if(pCodeCtx->codec_type==AVMEDIA_TYPE_AUDIO) {
        int64_t streamDuration = 0;
        if(audio_stream_idx>=0 && audio_stream_idx<pFormatCtx->nb_streams) {
             AVStream *stream = pFormatCtx->streams[audio_stream_idx];
             streamDuration = stream->duration * stream->time_base.num / stream->time_base.den;
        }
        //输出音频信息
        AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
        LOG("audio file format: %s",pFormatCtx->iformat->name);
        LOG("audio duration: %d", (pFormatCtx->duration!=AV_NOPTS_VALUE)?
                (pFormatCtx->duration/AV_TIME_BASE):0);
        LOG("audio stream duration: %d", streamDuration);
        LOG("audio channels: (lt:%d) %d", pCodeCtx->channel_layout, pCodeCtx->channels);
        LOG("audio sample rate: %d",pCodeCtx->sample_rate);
        LOG("audio sample accuracy: (fmt:%d) %d",pCodeCtx->sample_fmt,
               av_get_bytes_per_sample(pCodeCtx->sample_fmt)<<3);
        LOG("audio bit rate: %d",pCodeCtx->bit_rate);
        if(pCodec)
            LOG("audio decode name: %s", pCodec->name);
    } else if(pCodeCtx->codec_type==AVMEDIA_TYPE_VIDEO) {
        int64_t streamDuration = 0;
        if(video_stream_idx>=0 && video_stream_idx<pFormatCtx->nb_streams) {
             AVStream *stream = pFormatCtx->streams[video_stream_idx];
             streamDuration = stream->duration * stream->time_base.num / stream->time_base.den;
        }
        //输出视频信息
        AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
        LOG("video file format: %s",pFormatCtx->iformat->name);
        LOG("video duration: %d", (pFormatCtx->duration!=AV_NOPTS_VALUE)?
                (pFormatCtx->duration/AV_TIME_BASE):0);
        LOG("video stream duration: %d", streamDuration);
        LOG("video frame pixel format: %d", pCodeCtx->pix_fmt);
        LOG("video frame size: %d,%d",pCodeCtx->width, pCodeCtx->height);
        if(pCodec)
            LOG("video decode name: %s", pCodec->name);
        if(pCodeCtx->extradata&&pCodeCtx->extradata_size) {
            char *buf = (char*)av_malloc(pCodeCtx->extradata_size+1);
            if(buf) {
                memcpy(buf, pCodeCtx->extradata, pCodeCtx->extradata_size);
                buf[pCodeCtx->extradata_size] = '\0';
                LOG("video extradata: %s", buf);
                av_freep(&buf);
            }
        }
    }
}

void FFmpegMediaDecoder::_setStatus(Status status)
{
    mStatus = status;
}

void FFmpegMediaDecoder::_printError(int code, const char* message)
{
    printError(code, message);
}

void FFmpegMediaDecoder::printError(int code, const char* message)
{
    mCallbackMutex.lock();
    if(mCallback)
        mCallback->onDecodeError(code, message);
    mCallbackMutex.unlock();
}

void FFmpegMediaDecoder::print_error(const char *name, int err)
{
    char errbuf[200], message[256];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", name, errbuf_ptr);

    sprintf(message, "(%s)%s", name, errbuf_ptr);
    printError(err, message);
}
