#include "StreamMediaDecoder.h"
#include <QDebug>

StreamMediaDecoder::StreamMediaDecoder(QObject *parent)
    : QObject(parent), mThread(this), syncer(new AVSynchronizer)
{
    //TEST
    fp_pcm = 0;
    fp_yuv = 0;

    mediaDecoder.setCallback(this);
}

StreamMediaDecoder::~StreamMediaDecoder()
{

}

void StreamMediaDecoder::setOutAudio2(int rate, int channels)
{
    enum AVSampleFormat fmt;
    mediaDecoder.getOutAudioParams(&fmt, NULL, NULL);
    mediaDecoder.setOutAudio(fmt, rate, channels);
}

void StreamMediaDecoder::setOutVideo2(int width, int height)
{
    enum AVPixelFormat fmt;
    mediaDecoder.getOutVideoParams(&fmt, NULL, NULL);
    mediaDecoder.setOutVideo(fmt, width, height);
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

void StreamMediaDecoder::onDecodeError(int code, std::string msg)
{
    qDebug()<<"onDecodeError"<<QString::number(code)<<QString::fromStdString(msg);
}

void StreamMediaDecoder::onAudioDataReady(std::shared_ptr<MediaData> data)
{
    audio_ts = data->pts;
    syncer->setAudioTimeBaseD(data->time_base_d);
    syncer->setAudioDecodingTs(data->pts);

    emit audioData(data);
}

void StreamMediaDecoder::onVideoDataReady(std::shared_ptr<MediaData> data)
{
    video_ts = data->pts;
    syncer->setVideoTimeBaseD(data->time_base_d);
    syncer->setVideoDecodingTs(data->pts);

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
    syncer->reset();

    AVDictionary* options = NULL;
//    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    retCode = mediaDecoder.open(url, options, mIsHwaccels);
    bool haveVideo = mediaDecoder.videoStream()?true:false;
    bool haveAudio = mediaDecoder.audioStream()?true:false;

    bool isDecoding = true;
    syncer->resetOsTime();
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

        retCode = mediaDecoder.decoding();
        if(retCode<0) {
            qDebug()<<"----decode-failure----"<<mediaDecoder.status();
            break;
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
            if(syncer->getOsDelaySec()>1) {
                QThread::msleep(10);
            } else {
                break;
            }
        }
    }

    mediaDecoder.close();

    qDebug()<<"----stop-decoding----";
}

void StreamMediaDecoder::test()
{
    fp_pcm = fopen("D:\\test\\out.pcm", "wb");
    fp_yuv = fopen("D:\\test\\out.yuv", "wb");

    qDebug()<<"------------------open-------------------";
    mediaDecoder.open("http://ivi.bupt.edu.cn/hls/cctv1hd.m3u8", true);
    qDebug()<<"------------------decoding-------------------";
    mediaDecoder.decoding();
    qDebug()<<"------------------close-------------------";
    mediaDecoder.close();
    qDebug()<<"------------------end-------------------";

    fclose(fp_pcm);
    fclose(fp_yuv);
}
