#ifndef WAVEFORMPROCESSOR_H
#define WAVEFORMPROCESSOR_H

#include <QObject>
#include <QThread>
#include "LockedMap.h"
#include "SampleBuffer.h"
#include <memory>

class LoopBuffer
{
public:
    LoopBuffer() {
        data = nullptr;
        size = 0;
        offset = 0;
        length = 0;
    }
    LoopBuffer(int size) {
        data = new unsigned char[size];
        this->size = size;
        offset = 0;
        length = 0;
    }
    ~LoopBuffer() {
        delete data;
    }

    void alloc(int size) {
        if(data)
            delete data;
        data = new unsigned char[size];
        this->size = size;
    }

    int push(unsigned char *pda, int len) {
        int cpLen = size-length>len?len:size-length;
        int start = (offset+length)>size?(offset+length)-size:(offset+length);
        int stop = (offset+length+cpLen)>size?(offset+length+cpLen)-size:(offset+length+cpLen);
        if(start>stop) {
            memcpy(data+start, pda, size-offset);
            memcpy(data, pda+(size-offset), cpLen-(size-offset));
        } else {
            memcpy(data+start, pda, cpLen);
        }
        length += cpLen;
        return cpLen;
    }

    int pop(unsigned char *pda, int len) {
        int cpLen = length>len?len:length;
        int start = offset;
        int stop = (offset+cpLen)>size?(offset+cpLen)-size:(offset+cpLen);
        if(start>stop) {
            memcpy(pda, data+start, size-start);
            memcpy(pda+(size-start), data, cpLen-(size-start));
        } else {
            memcpy(pda, data+start, cpLen);
        }
        length -= cpLen;
        return cpLen;
    }

    int len() {
        return length;
    }
    int siz() {
        return size;
    }
    int remain() {
        return size-length;
    }
    void reset() {
        offset = 0;
        length = 0;
    }

private:
    unsigned char *data;
    int size;
    int offset;
    int length;
};

class WaveformProcessor : public QObject
{
    Q_OBJECT
public:
    explicit WaveformProcessor(QObject *parent = nullptr);
    ~WaveformProcessor();

    void addSample(int sourceId, std::shared_ptr<SampleBuffer> buffer);
    QList<int> getShowData(int sourceId, int maxlen=0);
    void clear(int sourceId);

private:
    QThread *mThread;
    LockedMap<int, std::list<std::shared_ptr<SampleBuffer>>> mDataMap;
    LockedMap<int, std::list<int>> mShowData;
    LockedMap<int, LoopBuffer> mData16Map;

    void addData16Bits(int sourceId, SampleBuffer *buffer);
    void addData32Bits(int sourceId, SampleBuffer *buffer);
    void addWave(int sourceId, int wave);

signals:
    void sigRun();

private slots:
    void run();
};

#endif // WAVEFORMPROCESSOR_H
