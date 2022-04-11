#ifndef SDLAUDIODEVICE_H
#define SDLAUDIODEVICE_H

#include <list>
#include "LockedMap.h"
#include "samplebuffer.h"

extern "C"{
    #include "SDL.h"
}

/*无法解析的外部符号 main，该符号在函数 WinMain 中被引用*/
#undef main

class SdlAudioDevBuf
{
public:
    SdlAudioDevBuf() {
        mMemory = 0;
        mSamples = 0;
    }
    ~SdlAudioDevBuf() {
        clear();
    }
    void enqueue(SampleBuffer *buffer) {
        mList.push_back(buffer);
        mMemory += buffer->len();
        mSamples += buffer->samples;
    }
    SampleBuffer* front() {
        if(mList.size()) {
            return mList.front();
        }
        return nullptr;
    }
    SampleBuffer* dequeue() {
        if(mList.size()) {
            SampleBuffer* b = mList.front();
            mList.pop_front();
            if(b) {
                mMemory -= b->len();
                mSamples -= b->samples;
            }
            return b;
        }
        return nullptr;
    }
    void clear() {
        for(;mList.size();) {
            SampleBuffer* b = dequeue();
            if(b)
                delete b;
        }
    }
    int size() {
        return mList.size();
    }
    long memory() {
        return mMemory;
    }
    long samples() {
        return mSamples;
    }
private:
    std::mutex mMutex;
    std::list<SampleBuffer*> mList;
    long mMemory;
    long mSamples;
};

class SdlAudioDevice
{
public:
    enum CardState{
        Close, Open, Pause
    };

    explicit SdlAudioDevice(int iscapture=0);

    const char* name();

    SDL_AudioDeviceID devid();

    SDL_AudioFormat format();

    int open(const char *device, SDL_AudioSpec spec);

    void pause();

    void resume();

    void close();

    void addData(int index, Uint8* buf, int len, unsigned long timestamp=0);

    int mixVolume();
    void setMixVolume(int volume);

    std::list<int> indexs();

    void setMaxMemory(int size);
    long memorySize(int index);
    long sampleSize(int index);
    int bufferSize(int index);

    int takeIndex(int index, Uint8 *stream, int len);
    int takeIndex(int index, SampleBuffer *buffer, int len);

    int take(Uint8 *stream, int len);

    void clear(int index);

    void clearAll();

    bool isOpened();

    void print_status();


private:
    CardState mState;
    int mIscapture;
    char mDeviceName[256];
    int mMixVolume;
    SDL_AudioDeviceID mDeviceId;
    SDL_AudioSpec mDesiredSpec, mObtainedSpec;
    LockedMap<int, SdlAudioDevBuf*> mDataMap;
    std::mutex mLimitMemoryMutex;
    int mLimitMemory;

    int sampleBytes(int fmt);
    void checkMemory(int index);
};

#endif // SDLAUDIODEVICE_H
