#ifndef SAMPLEBUFFER_H
#define SAMPLEBUFFER_H

#include <string.h>
#include <stdint.h>

class SampleBuffer
{
public:
    SampleBuffer() {
        mData = nullptr;
        mLen = 0;
        mPos = 0;
        //
        timestamp = 0;
        channels = 0;
        samples = 0;
        sampleBytes = 0;
    }

    ~SampleBuffer() {
        if(mData)
            delete mData;
        mData = nullptr;
        mLen = 0;
    }

    bool copy(const uint8_t *data, int len) {
        if(mData)
            delete mData;

        mData = new uint8_t[len];
        if(mData) {
            mLen = len;
            mPos = 0;
            memcpy(mData, data, len);
            return true;
        }
        return false;
    }

    bool copy(SampleBuffer &b) {
        bool s = copy(b.data(), b.len());
        timestamp = b.timestamp;
        channels = b.channels;
        sampleBytes = b.sampleBytes;
        return s;
    }

    const uint8_t *data() {
        return mData;
    }

    int len() {
        return mLen;
    }

    int pos() {
        return mPos;
    }

    void setPos(int pos) {
        if(pos<0)
            mPos = 0;
        else if(pos>mLen)
            mPos = mLen;
        else
            mPos = pos;
    }

public:
    //音频相关
    unsigned long timestamp;
    int channels;
    int samples;
    int sampleBytes;

private:
    uint8_t *mData;
    int mLen;
    int mPos;

};

#endif // SAMPLEBUFFER_H
