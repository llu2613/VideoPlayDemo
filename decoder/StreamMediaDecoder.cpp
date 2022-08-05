#include "StreamMediaDecoder.h"
#include <QDebug>

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

void StreamMediaDecoder::onDecodeError(int code, std::string msg)
{
    qDebug()<<"onDecodeError"<<QString::number(code)<<QString::fromStdString(msg);
}

void StreamMediaDecoder::audioDecodedData(AVFrame *frame, AVPacket *packet)
{
    FFmpegMediaDecoder::audioDecodedData(frame, packet);
}

void StreamMediaDecoder::videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight)
{
    FFmpegMediaDecoder::videoDecodedData(frame, packet, pixelHeight);
}

void StreamMediaDecoder::audioDataReady(std::shared_ptr<MediaData> data)
{
    audio_ts = data->pts;
    syncer->setAudioTimeBaseD(data->time_base_d);
    syncer->setAudioDecodingTs(data->pts);

    emit audioData(data);
}

void StreamMediaDecoder::videoDataReady(std::shared_ptr<MediaData> data)
{
    video_ts = data->pts;
    syncer->setVideoTimeBaseD(data->time_base_d);
    syncer->setVideoDecodingTs(data->pts);
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

        retCode = decoding();
        if(retCode<0) {
            qDebug()<<"----decode-failure----";
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

    close();

    qDebug()<<"----stop-decoding----";
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
