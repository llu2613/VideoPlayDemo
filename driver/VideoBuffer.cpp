#include "VideoBuffer.h"
#include <QTime>
#include <QElapsedTimer>
#include <QDebug>

#define DEFAULT_FRAME_RATE  25
#define MAX_FRAME_NUM    2000
#define MAX_FRAME_MEM    (200*1000*1000)

/**
 * 【Android FFMPEG 开发】FFMPEG 音视频同步
 * ( 音视频同步方案 | 视频帧 FPS 控制 | H.264 编码 I / P / B 帧 | PTS | 音视频同步 )
 * https://blog.csdn.net/shulianghan/article/details/104891200
 */
VideoBuffer::VideoBuffer(QString tag, QObject *parent)
    : QThread(parent)
{
    mSync = nullptr;
    mFrameRate = DEFAULT_FRAME_RATE;
    mMemorySize = 0;

    mTag = tag;
}

VideoBuffer::~VideoBuffer()
{
    isThRunning = false;

    clear();
}

void VideoBuffer::setSync(std::shared_ptr<AVSynchronizer> &sync)
{
    mSyncMutex.lock();
    mSync = sync;
    mSyncMutex.unlock();
}

void VideoBuffer::run()
{
    isThRunning = true;
    QElapsedTimer eTimer;
    uint64_t playUs = 0;
    while (isThRunning) {

        double frame_delay = 1.0/(mFrameRate>0?mFrameRate:DEFAULT_FRAME_RATE);

        mDataMutex.lock();
        if(mDataList.length()) {
            MediaData *data = mDataList.takeFirst();

            uint64_t durUs = data->duration*data->time_base_d*1000*1000;

            if(!playUs) {
                eTimer.restart();
            } else {
                //如果音视频之间差距低于 0.05 秒 , 不操作 ( 50ms )
                qint64 elapsedUs = eTimer.nsecsElapsed()/1000;
                const qint64 limitUs = 2*50*1000;

                if(abs((long long)(playUs-elapsedUs))>60*1000*1000) {
                    //时钟错误
                    qDebug()<<"reset play timer, diff is too large!";
                    playUs = 0;
                }

                for(int i=0;playUs && abs((long long)(playUs-elapsedUs))>limitUs
                    && i<2 && mDataList.length();i++) {
                    mMemorySize -= calcMemorySize(data);
                    data->release();
                    delete data;
                    playUs += durUs;

                    qDebug()<<"discard one frame";

                    data = mDataList.takeFirst();
                    durUs = data->duration*data->time_base_d*1000*1000;
                }
            }

            playUs += durUs;

            if(data) {
                mSyncMutex.lock();
                if(mSync) {
                    mSync->setVideoPlayingTs(data->pts);
                }
                mSyncMutex.unlock();
                std::shared_ptr<MediaData> mediaData(data);
                showFrame(mediaData);
                mMemorySize -= calcMemorySize(mediaData.get());
            } else {
                qDebug()<<"mediaData is null";
            }
        } else {
            if(playUs)
                qDebug()<<"reset play timer";

            playUs = 0;
        }
        mDataMutex.unlock();

        QThread::usleep(frame_delay*1000*1000);
    }

}

void VideoBuffer::addData(MediaData *data)
{
    if(data->frame_rate>0
            && (mFrameRate!=data->frame_rate)) {
        mFrameRate = data->frame_rate;
        qDebug()<<"frame_rate:"<<QString::number(data->frame_rate);
    }

    QMutexLocker locker(&mDataMutex);

    MediaData *copyData = new MediaData;
    copyData->copy(*data);
    mDataList.append(copyData);

    mMemorySize += calcMemorySize(copyData);

    int discard = 0;
    for(int i=mDataList.length(); i>MAX_FRAME_NUM
        ||mMemorySize>MAX_FRAME_MEM; i--) {
        MediaData* data = mDataList.takeFirst();
        mMemorySize -= calcMemorySize(copyData);
        data->release();
        delete data;
        discard++;
    }
    if(discard)
        qDebug()<<mTag<<"MediaQueue over, discard"<<discard<<"frames";
}

void VideoBuffer::clear()
{
    mDataMutex.lock();
    for(;mDataList.length();) {
        MediaData* data = mDataList.takeFirst();
        data->release();
        delete data;
    }
    mDataMutex.unlock();
}

long VideoBuffer::calcMemorySize(MediaData *data)
{
    long total = 0;
    for(int i=0; i<MAX_DATA_ARRAY; i++) {
        if(data->data[i]) {
            total += data->datasize[i];
        }
    }
    return total;
}
