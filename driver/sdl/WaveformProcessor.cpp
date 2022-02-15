#include "WaveformProcessor.h"
#include <QMutexLocker>
#include <QDebug>


#define SAMPLE_RATE 32000

#define AUDIO_FRAME_POINT_SIZE (SAMPLE_RATE/32/5) //每秒200个点

#define LIMIT_SHOW_SIZE  200

WaveformProcessor::WaveformProcessor(QObject *parent) : QObject(parent)
{
    mThread = new QThread();
    moveToThread(mThread);
    //connect(mThread, &QThread::finished, mThread, &QObject::deleteLater);
    connect(this, &WaveformProcessor::sigRun, this,&WaveformProcessor::run);

    mThread->start();
}

WaveformProcessor::~WaveformProcessor()
{
	delete mThread;
	for (int k: mData16Map.keys()) {
		delete mData16Map[k];
	}
	mData16Map.clear();

	for (int k : mShowData.keys()) {
		delete mShowData[k];
	}
	mShowData.clear();
	
}

void WaveformProcessor::addSample(int sourceId, std::shared_ptr<SampleBuffer> buffer)
{
    mDataMap.lock();
    if(mDataMap.contains(sourceId)) {
        mDataMap[sourceId].push_back(buffer);
    } else {
        std::list<std::shared_ptr<SampleBuffer>> list;
        list.push_back(buffer);
        mDataMap.insert(sourceId, list);
    }
    mDataMap.unlock();

    emit sigRun();
}

QList<int> WaveformProcessor::getShowData(int sourceId, int maxlen)
{
    QList<int> list;

    LockedMapLocker lk(mShowData.mutex());

    if(mShowData.contains(sourceId)) {
        std::list<int> *dataList = mShowData[sourceId];
        maxlen = maxlen? maxlen: dataList->size();
        std::list<int>::iterator itr = dataList->begin();
        for(int i=0; i<maxlen && itr!=dataList->end(); i++) {
            list.append(*itr);
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
    std::list<int> keys = mDataMap.keys();
    mDataMap.unlock();

    int count = 0;
    for(int idx: keys) {
        bool isEmpty = false;
        mDataMap.lock();
        if(mDataMap.contains(idx)
                && mDataMap[idx].size()==0) {
            mDataMap.remove(idx);
            if(mData16Map.contains(idx))
                mData16Map.remove(idx);
            isEmpty = true;
        }
        mDataMap.unlock();

        if(!isEmpty) {
            std::shared_ptr<SampleBuffer> buffer;
            mDataMap.lock();
            buffer = mDataMap[idx].front();
            mDataMap[idx].pop_front();
            mDataMap.unlock();

            addData16Bits(idx, buffer.get());

            count++;
        }
    }
    if(count)
        QThread::usleep(20);
}

void WaveformProcessor::addData16Bits(int sourceId, SampleBuffer *buffer)
{
    LockedMapLocker lk(mData16Map.mutex());

    const int nChannels = buffer->channels;
    const int step = sizeof(short)*nChannels;
    const int minSize = step*AUDIO_FRAME_POINT_SIZE;

    if(!mData16Map.contains(sourceId)) {
        //立体声*16位样本(2B)*AUDIO_FRAME_POINT_SIZE
        ByteLoopBuffer *bf = new ByteLoopBuffer(minSize * 100);
        bf->reset();
		mData16Map.insert(sourceId, bf);
    }
    ByteLoopBuffer *mData16Buf = mData16Map[sourceId];

    int cpLen = mData16Buf->push((unsigned char *)(buffer->data()+buffer->pos()),
                    buffer->len()-buffer->pos());
    buffer->setPos(buffer->pos()+cpLen);

    unsigned char *buf = new unsigned char[minSize];
    for(int n=0; n<1000; n++) {
        if(mData16Buf->len()<minSize) {
            break;
        }
        long total = 0;
        int cp = mData16Buf->pop(buf, minSize);
        for(int i=0; i<cp-step; i+=step) {
            total = total + abs(*((short *)(buf+i)));
        }
        short avgValue=(short)(total/AUDIO_FRAME_POINT_SIZE);
        addWave(sourceId, avgValue);
    }
    delete buf;

    cpLen = mData16Buf->push((unsigned char *)(buffer->data()+buffer->pos()),
                    buffer->len()-buffer->pos());
    buffer->setPos(buffer->pos()+cpLen);
}

void WaveformProcessor::addData32Bits(int sourceId, SampleBuffer *buffer)
{

}

void WaveformProcessor::addWave(int sourceId, int wave)
{
    LockedMapLocker lk(mShowData.mutex());

    if(mShowData.contains(sourceId)) {
		std::list<int> *list = mShowData[sourceId];
		list->push_back(wave);
        for(;list->size()>LIMIT_SHOW_SIZE;)
            list->pop_front();
    } else {
        std::list<int> *list = new std::list<int>();
        list->push_back(wave);
        mShowData.insert(sourceId, list);
    }

//    qDebug()<<"addWave"<<sourceId<<"wave:"<<wave;
}
