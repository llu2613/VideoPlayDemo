#include "FFmpegMediaDecoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <QDebug>

//20s超时退出
#define INTERRUPT_TIMEOUT (20 * 1000 * 1000)

FFmpegMediaDecoder::FFmpegMediaDecoder(QObject *parent) : QObject(parent)
{
    pFormatCtx = nullptr;
    pAudioCodecCtx = nullptr;
    pVideoCodecCtx = nullptr;

    audioFrameCnt = 0;
    videoFrameCnt = 0;
    audio_stream_idx = -1;
    video_stream_idx = -1;

    av_register_all();
    avformat_network_init();

    //Thread
    mThread = nullptr;

    //TEST
    fp_pcm = 0;
    fp_yuv = 0;
}

FFmpegMediaDecoder::~FFmpegMediaDecoder()
{
    mThread->quit();
    mThread->terminate();
    mThread->deleteLater();
}

int FFmpegMediaDecoder::interruptCallback(void *pCtx)
{
    FFmpegMediaDecoder *pThis = (FFmpegMediaDecoder*)pCtx;
    if((av_gettime() - pThis->lastFrameRealtime) > INTERRUPT_TIMEOUT){
        printf("(interruptCallback)主码流断开\n");
        return AVERROR_EOF;
    }
    return 0;
}

void FFmpegMediaDecoder::setOutAudio2(int rate, int channels)
{
    setOutAudio(AV_SAMPLE_FMT_S16, rate,
                channels==1?AV_CH_LAYOUT_MONO:AV_CH_LAYOUT_STEREO);
}

void FFmpegMediaDecoder::setOutVideo2(int width, int height)
{
    setOutVideo(AV_PIX_FMT_YUV420P, width, height);
}

void FFmpegMediaDecoder::setOutAudio(enum AVSampleFormat fmt, int rate, uint64_t ch_layout)
{
    if(pAudioCodecCtx && scaler.isAudioReady()) {
        scaler.freeAudioResample();
        scaler.setOutAudio(fmt, rate, ch_layout);
        scaler.initAudioResample(pAudioCodecCtx);
    } else {
        scaler.setOutAudio(fmt, rate, ch_layout);
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

int FFmpegMediaDecoder::audioFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int ret, got_frame;
    //解码AVPacket->AVFrame
    ret = avcodec_decode_audio4(pCodecCtx, frame, &got_frame, packet);
    if (ret < 0) {
        print_error("avcodec_decode_audio4", ret);
    }
    //非0，正在解码
    if (got_frame) {
        audioFrameCnt++;
//        printf(" 音频解码第%d帧 ", audioFrameCnt);
        int out_samples, out_buffer_size;
        uint8_t *pAudioOutBuffer = scaler.audioConvert(frame,
                                                       &out_samples,
                                                       &out_buffer_size);

        //写入文件进行测试
//        if(fp_pcm) {
//            fwrite(pAudioOutBuffer, 1, out_buffer_size, fp_pcm);
//        }
        std::shared_ptr<MediaData> mediaData(new MediaData);
//        MediaData *mediaData = new MediaData;
        mediaData->format = MediaData::AudioS16;
        mediaData->samples = out_samples;
        mediaData->sample_rate = scaler.outAudioRate();
        mediaData->channels = scaler.outAudioChannels();
        mediaData->pts = packet->pts;
        mediaData->duration = packet->duration;
        AVStream *stream = pFormatCtx->streams[packet->stream_index];
        mediaData->time_base_d = av_q2d(stream->time_base);
        //帧数据
        mediaData->data[0] = new unsigned char[out_buffer_size];
        mediaData->datasize[0] = out_buffer_size;
        memcpy(mediaData->data[0], pAudioOutBuffer, out_buffer_size);
        emit audioData(mediaData);
    }

    return got_frame;
}

int FFmpegMediaDecoder::videoFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int ret, got_picture;
    //7.解码一帧视频压缩数据，得到视频像素数据
    ret = avcodec_decode_video2(pCodecCtx, frame, &got_picture, packet);
    if (ret < 0) {
        print_error("avcodec_decode_video2", ret);
    }

    //为0说明解码完成，非0正在解码
    if (got_picture)
    {
        videoFrameCnt++;
//        printf(" 视频解码第%d帧 ", videoFrameCnt);
        //AVFrame转为像素格式YUV420，宽高
        //2 6输入、输出数据
        //3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
        //4 输入数据第一列要转码的位置 从0开始
        //5 输入画面的高度
        int out_height;
        AVFrame* out_frame = scaler.videoScale(pCodecCtx, frame, &out_height);

        AVFrame *pFrameYUV = out_frame;
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
        std::shared_ptr<MediaData> mediaData(new MediaData);
//        MediaData *mediaData = new MediaData;
        mediaData->format = MediaData::VideoYUV;
        mediaData->width = scaler.outVideoWidth();
        mediaData->height = scaler.outVideoHeight();
        mediaData->pts = packet->pts;
        mediaData->duration = packet->duration;
        mediaData->timestamp = frame->best_effort_timestamp;
        mediaData->repeat_pict = frame->repeat_pict; //重复显示次数
        AVStream *stream = pFormatCtx->streams[packet->stream_index];
        mediaData->time_base_d = av_q2d(stream->time_base);
        mediaData->frame_rate = av_q2d(stream->avg_frame_rate);
        //一帧图像（音频）的时间戳（时间戳一般以第一帧为0开始）
        //时间戳 = pts * (AVRational.num/AVRational.den)
        //int second= pFrame->pts * av_q2d(stream->time_base);
        int y_size = scaler.outVideoWidth() * out_height;
        int dataSize[] = {y_size, y_size / 4, y_size / 4};
        for(int i=0; i<3; i++) {
            mediaData->data[i] = new unsigned char[dataSize[i]];
            mediaData->datasize[i] = dataSize[i];
            mediaData->linesize[i] = pFrameYUV->linesize[i];
            memcpy(mediaData->data[i], pFrameYUV->data[i], dataSize[i]);
        }
        emit videoData(mediaData);
    }

    return got_picture;
}

int FFmpegMediaDecoder::openCodec(AVFormatContext *pFormatCtx, int stream_idx,
                                  AVCodecContext **pCodeCtx)
{
    int ret =0;
    //4.获取解码器
    //根据索引拿到对应的流,根据流拿到解码器上下文
    *pCodeCtx = pFormatCtx->streams[stream_idx]->codec;
    //再根据上下文拿到编解码id，通过该id拿到解码器
    AVCodec *pCodec = avcodec_find_decoder((*pCodeCtx)->codec_id);
    if (pCodec == NULL) {
        printError(-1, "无法解码");
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


int FFmpegMediaDecoder::open(const char* input)
{
    int ret =0;
    //封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    pFormatCtx = avformat_alloc_context();

    //设置阻塞中断函数
    pFormatCtx->interrupt_callback.callback = FFmpegMediaDecoder::interruptCallback;
    pFormatCtx->interrupt_callback.opaque = this;
    lastFrameRealtime = av_gettime();

    AVDictionary* options = NULL;
//    av_dict_set(&options, "stimeout", "6000000", 0); //设置超时时间 6s
//    av_dict_set(&options, "rtsp_transport", "tcp", 0);  //强制使用tcp，udp在1080p下会丢包导致花屏
//    av_dict_set(&options, " max_delay", " 5000000", 0);  //强制使用tcp，udp在1080p下会丢包导致花屏
    av_dict_set(&options, "buffer_size", "8388608", 0); //设置udp的接收缓冲 8M

    //2.打开输入视频文件
    ret = avformat_open_input(&pFormatCtx, input, NULL, &options);
    if (ret != 0)
    {
        print_error("avformat_open_input", ret);
        printError(ret, "无法打开输入媒体文件");
        return ret;
    }

    //3.获取视频文件信息
    ret = avformat_find_stream_info(pFormatCtx,NULL);
    if (ret < 0)
    {
        printError(ret, "无法获取媒体文件信息");
        return ret;
    }

    //获取视频流的索引位置
    //遍历所有类型的流（音频流、视频流、字幕流），找到视频流
    audio_stream_idx = -1;
    video_stream_idx = -1;
    //number of streams
    for (int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        //流的类型
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_idx = i;
        }
        else if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_idx = i;
        }
    }

    if (audio_stream_idx != -1)
    {
        //打开解码器
        ret = openCodec(pFormatCtx, audio_stream_idx, &pAudioCodecCtx);
        if(ret<0) {
            printError(ret, "音频解码器打开失败");
            return ret;
        }
        //重采样
        scaler.initAudioResample(pAudioCodecCtx);

        printCodecInfo(pAudioCodecCtx);
    }
    else
    {
        printf("%s","找不到音频流\n");
    }

    if (video_stream_idx != -1)
    {
        //打开解码器
        ret = openCodec(pFormatCtx, video_stream_idx, &pVideoCodecCtx);
        if(ret<0) {
            printError(ret, "视频解码器打开失败");
        }
        //重缩放
        scaler.initVideoScale(pVideoCodecCtx);

        printCodecInfo(pVideoCodecCtx);
    }
    else
    {
        printf("%s","找不到视频流\n");
    }

    audioFrameCnt = 0;
    videoFrameCnt = 0;

    return 0;
}

void FFmpegMediaDecoder::close()
{
    if(pAudioCodecCtx) {
        avcodec_close(pAudioCodecCtx);
        pAudioCodecCtx = nullptr;
    }
    if(pVideoCodecCtx) {
        avcodec_close(pVideoCodecCtx);
        pVideoCodecCtx = nullptr;
    }
    if(pFormatCtx) {
        avformat_free_context(pFormatCtx);
        pFormatCtx = nullptr;
    }

    scaler.freeAudioResample();
    scaler.freeVideoScale();
}

void FFmpegMediaDecoder::decoding()
{
    if(!pFormatCtx) {
        printf("pFormatCtx is null");
        return;
    }

    //编码数据
    AVPacket *packet = av_packet_alloc();
    //解压缩数据
    AVFrame *frame = av_frame_alloc();

    //6.一帧一帧读取压缩的音频数据AVPacket
    int readRet = 0;
    bool isDecoding = true;
    for (int loopCnt=0;isDecoding;) {
        //TEST
        if(fp_pcm||fp_yuv) {
            if(++loopCnt>5000)
                break;
        }

        readRet = av_read_frame(pFormatCtx, packet);
        if(readRet < 0) {
            print_error("av_read_frame", readRet);
            break;
        }

        lastFrameRealtime = av_gettime();

        //退出标记
        mStopMutex.lock();
        if(mStopFlag)
            isDecoding = false;
        mStopMutex.unlock();

        //解码
        if (packet->stream_index == audio_stream_idx) {
            if(isDecoding)
                audioFrame(pAudioCodecCtx, frame, packet);
        } else if(packet->stream_index == video_stream_idx) {
            if(isDecoding)
                videoFrame(pVideoCodecCtx, frame, packet);
        }

        //释放资源
        av_free_packet(packet);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
}

void FFmpegMediaDecoder::printCodecInfo(AVCodecContext *pCodeCtx)
{
    if(pCodeCtx->codec_type==AVMEDIA_TYPE_AUDIO) {
        //输出音频信息
        AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
        qDebug("audio file format: %s",pFormatCtx->iformat->name);
        qDebug("audio duration: %d", (pFormatCtx->duration)/1000000);
        qDebug("audio channels: (lt:%d) %d", pCodeCtx->channel_layout, pCodeCtx->channels);
        qDebug("audio sample rate: %d",pCodeCtx->sample_rate);
        qDebug("audio sample accuracy: (fmt:%d) %d",pCodeCtx->sample_fmt,
               av_get_bytes_per_sample(pCodeCtx->sample_fmt)<<3);
        qDebug("audio bit rate: %d",pCodeCtx->bit_rate);
        if(pCodec)
            qDebug("audio decode name: %s", pCodec->name);
    } else if(pCodeCtx->codec_type==AVMEDIA_TYPE_VIDEO) {
        //输出视频信息
        AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
        qDebug("video file format: %s",pFormatCtx->iformat->name);
        qDebug("video duration: %d", (pFormatCtx->duration)/1000000);
		qDebug("video frame pixel format: %d", pCodeCtx->pix_fmt);
        qDebug("video frame size: %d,%d",pCodeCtx->width, pCodeCtx->height);
        if(pCodec)
            qDebug("video decode name: %s", pCodec->name);
    }
}

void FFmpegMediaDecoder::printError(int code, const char* message)
{
    qDebug()<<QString("code:%1 msg:%2").arg(code).arg(QString::fromLocal8Bit(message));
}

void FFmpegMediaDecoder::print_error(const char *name, int err)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", name, errbuf_ptr);
    qDebug("[qDebug]%s: %s", name, errbuf_ptr);
}

void FFmpegMediaDecoder::startDecoding()
{
    if(!mThread) {
        mThread = new QThread();
        this->moveToThread(mThread);
        mThread->start();
        connect(this, &FFmpegMediaDecoder::runDecoding, this, &FFmpegMediaDecoder::run);
    }
    emit runDecoding();
}

void FFmpegMediaDecoder::stopDecoding()
{
    mStopMutex.lock();
    mStopFlag = true;
    mStopMutex.unlock();
}

//线程
void FFmpegMediaDecoder::run()
{
    qDebug()<<"----thread-running----";
    mStopMutex.lock();
    mStopFlag = false;
    mStopMutex.unlock();

    char url[512];
    std::string url_s = inputUrl.toStdString();
    memcpy(url, url_s.c_str(), (url_s.length()+1)>512?512:(url_s.length()+1));

    qDebug()<<"----start-decoding----";

    open(url);
    decoding();
    close();

    qDebug()<<"----stop-decoding----";
}

void FFmpegMediaDecoder::setInputUrl(QString url)
{
    inputUrl = url;
}

void FFmpegMediaDecoder::test()
{
    fp_pcm = fopen("D:\\test\\out.pcm", "wb");
    fp_yuv = fopen("D:\\test\\out.yuv", "wb");

    qDebug()<<"------------------open-------------------";
    open("http://ivi.bupt.edu.cn/hls/cctv1hd.m3u8");
    qDebug()<<"------------------decoding-------------------";
    decoding();
    qDebug()<<"------------------close-------------------";
    close();
    qDebug()<<"------------------end-------------------";

    fclose(fp_pcm);
    fclose(fp_yuv);
}
