#include "WaveformProcessor.h"
#include <QMutexLocker>
#include <QDebug>

#define BYTE4_INT(a,b,c,d) ( (0x0ff&a)<<24|(0x0ff&b)<<16|(0x0ff&c)<<8|(0x0ff&d) )
#define BYTE2_INT(a,b) ( (0x0ff&a)<<8|(0x0ff&b) )

#define AUDIO_FRAME_POINT_SIZE   8  //2^n次方
#define BYTE_PER_SAMPLE 2  //16位采样

#define LIMIT_SHOW_SIZE  1000

WaveformProcessor::WaveformProcessor(QObject *parent) : QObject(parent)
{
    mThread = new QThread();
    moveToThread(mThread);
    connect(mThread, &QThread::finished, mThread, &QObject::deleteLater);
    connect(this, &WaveformProcessor::sigRun, this,&WaveformProcessor::run);
    mThread->start();
}

void WaveformProcessor::addSample(int sourceId, std::shared_ptr<SampleBuffer> buffer)
{
    mDataMap.lock();
    if(mDataMap.contains(sourceId)) {
        mDataMap[sourceId].append(buffer);
    } else{
        QList<std::shared_ptr<SampleBuffer>> list;
        list.append(buffer);
        mDataMap.insert(sourceId, list);
    }
    mDataMap.unlock();

    emit sigRun();
}

QList<int> WaveformProcessor::getShowData(int sourceId)
{
    QList<int> list;

    QMutexLocker locker(&mShowData.mutex());
    if(mShowData.contains(sourceId)) {
        int len = mShowData[sourceId].size();
        for(int i=0; i<len; i++) {
            list.append(mShowData[sourceId].at(i));
        }
    }

    return list;
}

void WaveformProcessor::clear(int sourceId)
{
    mShowData.lock();
    if(mShowData.contains(sourceId))
        mShowData.remove(sourceId);
    mShowData.unlock();
}

void WaveformProcessor::run()
{
    mDataMap.lock();
    QList<int> keys = mDataMap.keys();
    mDataMap.unlock();

    int count = 0;
    for(int idx: keys) {
        bool isEmpty = false;
        mDataMap.lock();
        if(mDataMap.contains(idx)
                && mDataMap[idx].size()==0) {
            mDataMap.remove(idx);
            isEmpty = true;
        }
        mDataMap.unlock();

        if(!isEmpty) {
            std::shared_ptr<SampleBuffer> buffer;
            mDataMap.lock();
            buffer = mDataMap[idx].takeFirst();
            mDataMap.unlock();

            addData(idx, buffer.get());
            count++;
        }
    }
    if(count)
        QThread::msleep(20);
}

void WaveformProcessor::addData(int sourceId, SampleBuffer *buffer)
{
    double total = 0;
    int count = 0;

    unsigned char *data = (unsigned char *)(buffer->data());
    int dataSize = buffer->len();
    int nChannels = buffer->channels;
    int interval = dataSize/AUDIO_FRAME_POINT_SIZE/nChannels;
    //显示点采用间隔段平均值的方法进行显示：不是每一个点都显示:
    for(int i=0;i < dataSize;i=i+2*nChannels){
        total = total + abs(*((short *)(data+i)));
        if(count%interval == 0&&(count!=0)){
            short avgValue=(short)(total/interval);
            addWave(sourceId, avgValue);
            total = 0;
        }
        ++count;
    }
}

void WaveformProcessor::addWave(int sourceId, int wave)
{
    QMutexLocker locker(&mShowData.mutex());
    if(mShowData.contains(sourceId)) {
        mShowData[sourceId].append(wave);
        for(;mShowData[sourceId].size()>LIMIT_SHOW_SIZE;)
            mShowData[sourceId].removeFirst();
    } else {
        QList<int> list;
        list.append(wave);
        mShowData.insert(sourceId, list);
    }

    qDebug()<<"addWave"<<sourceId<<"wave:"<<wave;
}
