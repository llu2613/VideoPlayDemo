#include "StreamMediaDecoder.h"
#include <QDebug>

StreamMediaDecoder::StreamMediaDecoder(QObject *parent)
    : QObject(parent), mThread(this)
{
    //TEST
    fp_pcm = 0;
    fp_yuv = 0;

    //av_register_all();
    //avcodec_register_all();
    //avformat_network_init();

    mediaDecoder.setCallback(this);
    syncer = new AVSynchronizer;
}

StreamMediaDecoder::~StreamMediaDecoder()
{
    delete syncer;
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

AVSynchronizer* StreamMediaDecoder::getSynchronizer()
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
    audio_ts = data->pts*data->time_base_d;
    if(audio_ts>video_ts) {
        syncer->setDecodingTs(audio_ts);
    }
    emit audioData(data);
}

void StreamMediaDecoder::onVideoDataReady(std::shared_ptr<MediaData> data)
{
    video_ts = data->pts*data->time_base_d;
    if(audio_ts<video_ts) {
        syncer->setDecodingTs(video_ts);
    }
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

    retCode = mediaDecoder.open(url, mIsHwaccels);

    bool isDecoding = true;
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
            if(mediaDecoder.status()==FFmpegMediaDecoder::Broken
                    ||mediaDecoder.readFailedCount()>10) {
                break;
            }
        }

        for(int i=0; isDecoding&&i<20; i++) {
            //退出标记
            mStopMutex.lock();
            if(mStopFlag)
                isDecoding = false;
            mStopMutex.unlock();

            if(syncer->getAudioDecodeDelay()>1
                    ||syncer->getVideoDecodeDelay()>1) {
                    QThread::msleep(100);
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
