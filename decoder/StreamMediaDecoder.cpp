#include "StreamMediaDecoder.h"
#include <QDebug>
#include <time.h>
#include "common/ffmpeg_commons.h"

StreamMediaDecoder::StreamMediaDecoder(QObject *parent)
    : QObject(parent), mThread(this), syncer(new AVSynchronizer)
{
    //TEST
    fp_pcm = 0;
    fp_yuv = 0;
}

StreamMediaDecoder::~StreamMediaDecoder()
{

}

void StreamMediaDecoder::setOutAudio2(int rate, int channels)
{
    enum AVSampleFormat fmt;
    getOutAudioParams(&fmt, NULL, NULL);
    setOutAudio(fmt, rate, channels);
}

void StreamMediaDecoder::setOutVideoFmt(enum AVPixelFormat fmt)
{
    int width, height;
    getOutVideoParams(NULL, &width, &height);
    setOutVideo(fmt, width, height);
}

void StreamMediaDecoder::setOutVideo(enum AVPixelFormat fmt, int width, int height)
{
    FFmpegMediaDecoder::setOutVideo(fmt, width, height);
}

void StreamMediaDecoder::setOutVideo2(int width, int height)
{
    enum AVPixelFormat fmt;
    getOutVideoParams(&fmt, NULL, NULL);
    setOutVideo(fmt, width, height);
}

std::shared_ptr<AVSynchronizer> &StreamMediaDecoder::getSynchronizer()
{
    return syncer;
}

void StreamMediaDecoder::startPlay(QString url, bool isHwaccels)
{
    mInputUrl = url;
    mIsHwaccels = isHwaccels;

    mThread.runTh();
}

void StreamMediaDecoder::stopPlay()
{
    mStopMutex.lock();
    mStopFlag = true;
    mStopMutex.unlock();
}

void StreamMediaDecoder::seek(int64_t seekFrame)
{
    seek_time = seekFrame;
}

void StreamMediaDecoder::onDecodeError(int code, std::string msg)
{
    qDebug()<<"onDecodeError"<<QString::number(code)<<QString::fromStdString(msg);
}

void StreamMediaDecoder::printInfo(const char* message)
{
    qDebug()<<"printInfo"<<QString::fromStdString(message);
}
void StreamMediaDecoder::printError(const char* message)
{
    qDebug()<<"printError"<<QString::fromStdString(message);
}

void StreamMediaDecoder::audioDecodedData(AVFrame *frame, AVPacket *packet)
{
    audio_ts = frame->pts;
    const AVStream* stream = audioStream();
    syncer->setAudioTimeBase(stream->time_base);
    syncer->setAudioDecodingTs(packet->pts);

    FFmpegMediaDecoder::audioDecodedData(frame, packet);
}

void StreamMediaDecoder::videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight)
{
    video_ts = frame->pts;
    const AVStream* stream = videoStream();
    syncer->setVideoTimeBase(stream->time_base);
    syncer->setVideoDecodingTs(packet->pts);

    FFmpegMediaDecoder::videoDecodedData(frame, packet, pixelHeight);
}

void StreamMediaDecoder::audioDataReady(std::shared_ptr<MediaData> data)
{
    emit audioData(data);
}

void StreamMediaDecoder::videoDataReady(std::shared_ptr<MediaData> data)
{
    //qDebug() << "----videoDataReady----"<< data->pixel_format<<data->width<< data->height;
    emit videoData(data);
}

//线程
void StreamMediaDecoder::run()
{
    qDebug()<<"----thread-running----";
    mStopMutex.lock();
    mStopFlag = false;
    mStopMutex.unlock();

    char url[512];
    std::string url_s = mInputUrl.toStdString();
    memcpy(url, url_s.c_str(), (url_s.length()+1)>512?512:(url_s.length()+1));

    qDebug()<<"----start-decoding----";

    int retCode = 0;

    audio_ts = 0;
    video_ts = 0;
    seek_time = -1;
    syncer->reset();

    AVDictionary* options = NULL;
//    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    /////////////////////
    AVDictionary *avdic = NULL;
    av_dict_set(&avdic, "stimeout", "3000000", 0);//6秒 microseconds:
    av_dict_set(&avdic, "rtsp_transport", "tcp", 0);//3秒：//解决H265回放拉流花屏的问题
    //av_dict_set(&avdic, "rtsp_transport", "udp", 0);
    av_dict_set(&avdic, "buffer_size", "8388608", 0); //设置udp的接收缓冲 8M

    //av_dict_set(&avdic, "buffer_size", "10240000", 0);//解决H265回放拉流花屏的问题
    //av_dict_set(&avdic, "threads", "auto", 0);
    //av_dict_set(&avdic, "refcounted_frames", "1", 0);
    /////////////////////
    retCode = open(url, avdic, mIsHwaccels);
    if(retCode<0) {
        emit mediaDecoderEvent(OpenFailure, "OPENFAIL");
    } else {
        emit mediaDecoderEvent(OpenSuccess, "rtspstream is start playing!");
    }
    bool haveVideo = videoStream()?true:false;
    bool haveAudio = audioStream()?true:false;

	if (retCode >= 0 && haveVideo) {
		enum AVPixelFormat fmt;
		int width, height;
		getSrcVideoParams(&fmt, &width, &height);
		setOutVideo2(width, height);
	}

    bool isDecoding = true;
    syncer->resetOsTime();
    clock_t start, finish;
    for (int loopCnt=0;isDecoding;) {
        //TEST
        if(fp_pcm||fp_yuv) {
            if(++loopCnt>5000)
                break;
        }

        //退出标记
        mStopMutex.lock();
        if(mStopFlag)
            isDecoding = false;
        mStopMutex.unlock();

        start = clock();
        retCode = decoding();
        if(retCode<0) {
            qDebug()<<"----decode-failure----"<<retCode<<QString::fromLocal8Bit(wrap_av_err2str(retCode));
            emit mediaDecoderEvent(DecodingFault, "DECODEFAIL");
            break;
        }

        if(seek_time>=0) {
            seekto(seek_time/1000.0);
            seek_time = -1;
        }

        for(int i=0; isDecoding&&i<100; i++) {
            //退出标记
            mStopMutex.lock();
            if(mStopFlag)
                isDecoding = false;
            mStopMutex.unlock();

//            if((haveAudio && syncer->getAudioDelaySec()>1)
//                    ||(haveVideo && syncer->getVideoDelaySec()>1)) {
//                QThread::msleep(10);
//            } else {
//                break;
//            }
            if(syncer->getOsDelayMs()>500) {
                QThread::msleep(10);
            } else {
                break;
            }
        }
        finish = clock();
//        if(finish-start>40)
//            qDebug()<<"time:"<<(finish-start);
    }

    close();

    qDebug()<<"----stop-decoding----";
    emit mediaDecoderEvent(DecoderFinish, "DECODEDONE");
}

void StreamMediaDecoder::test()
{
    fp_pcm = fopen("D:\\test\\out.pcm", "wb");
    fp_yuv = fopen("D:\\test\\out.yuv", "wb");

    qDebug()<<"------------------open-------------------";
    open("http://ivi.bupt.edu.cn/hls/cctv1hd.m3u8", true);
    qDebug()<<"------------------decoding-------------------";
    decoding();
    qDebug()<<"------------------close-------------------";
    close();
    qDebug()<<"------------------end-------------------";

    fclose(fp_pcm);
    fclose(fp_yuv);
}
