#include "SdlAudioDevice.h"
#include <memory>
#include <QDebug>

#define INFO(fmt,...) qDebug(fmt, ##__VA_ARGS__)
//#define INFO(fmt,...) SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)
#define WARNING(fmt,...) SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)
#define ERROR(fmt,...) SDL_LogError(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)

#define MAX_FRAME_MEM (10*1024*1024)

SdlAudioDevice::SdlAudioDevice(int iscapture)
{
    mDeviceId = 0;
    mLimitMemory = MAX_FRAME_MEM;
    mIscapture = iscapture?SDL_TRUE:SDL_FALSE;
    SDL_zero(mDesiredSpec);
    SDL_zero(mObtainedSpec);
    memset(mDeviceName, 0, sizeof(mDeviceName));
}

const char* SdlAudioDevice::name()
{
    return mDeviceName;
}

SDL_AudioDeviceID SdlAudioDevice::devid()
{
    return mDeviceId;
}

SDL_AudioFormat SdlAudioDevice::format()
{
    return mDesiredSpec.format;
}

int SdlAudioDevice::open(const char *device, SDL_AudioSpec spec)
{
    mDeviceId = SDL_OpenAudioDevice(device, mIscapture,
                                    &spec, &mObtainedSpec, 0);
    if (mDeviceId == 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
    } else {
        if (spec.format != mObtainedSpec.format) { /* we let this one thing change. */
            SDL_Log("We didn't get %x(got:%x) audio format.", spec.format, mObtainedSpec.format);
        }
        SDL_PauseAudioDevice(mDeviceId, 0); /* start audio playing. */
        INFO("SDL_AudioDeviceID: %d", mDeviceId);
        mDesiredSpec = spec;
        memccpy(mDeviceName, device, '\0', sizeof(mDeviceName));
        mState = Open;
        return mDeviceId;
    }

    return 0;
}

void SdlAudioDevice::pause()
{
    if(mState==Open) {
        SDL_PauseAudioDevice(mDeviceId, 1);
        mState = Pause;
    }
}

void SdlAudioDevice::resume()
{
    if(mState==Pause) {
        SDL_PauseAudioDevice(mDeviceId, 0);
        mState = Open;
    }
}

void SdlAudioDevice::close()
{
    mState = Close;
    SDL_PauseAudioDevice(mDeviceId, 1);
    clearAll();
    SDL_CloseAudioDevice(mDeviceId);
    INFO("SDL_CloseAudioDevice: %d", mDeviceId);
    mDeviceId = 0;
}

void SdlAudioDevice::addData(int index, Uint8* buf, int len,
                             double timestamp)
{
    SampleBuffer *buffer = new SampleBuffer();
    if(!buffer->copy(buf, len)) {
        WARNING("No memory allocation!");
        return;
    }

    buffer->channels = mDesiredSpec.channels;
    buffer->timestamp = timestamp;
    buffer->sampleBytes = sampleBytes(mDesiredSpec.format);

    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.contains(index)) {
        SdlAudioDevBuf &devBuf = *(mDataMap.at(index));
        devBuf.enqueue(buffer);

        int discard = 0;
        mLimitMemoryMutex.lock();
        for(int i=0; i<10&&devBuf.size()>1; i++) {
            if(devBuf.memory()<mLimitMemory) {
                break;
            }
            SampleBuffer *b = devBuf.dequeue();
            if(b) {
                delete b;
                discard++;
            }
        }
        mLimitMemoryMutex.unlock();
        if(discard)
            WARNING("AudioCard buffer over, discard %d frames, %s index:%d",
                   discard, name(), index);
    } else {
        mDataMap.insert(index, new SdlAudioDevBuf());
        mDataMap[index]->enqueue(buffer);
    }
}

std::list<int> SdlAudioDevice::indexs()
{
    LockedMapLocker lk(mDataMap.mutex());
    return mDataMap.keys();
}

void SdlAudioDevice::setMaxMemory(int size)
{
    std::lock_guard<std::mutex> lk(mLimitMemoryMutex);
    mLimitMemory = size;
}

int SdlAudioDevice::memorySize(int index)
{
    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.find(index)!=mDataMap.end())
        return mDataMap.at(index)->memory();
    return 0;
}

int SdlAudioDevice::bufferSize(int index)
{
    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.find(index)!=mDataMap.end())
        return mDataMap.at(index)->size();
    return 0;
}

int SdlAudioDevice::takeIndex(int index, Uint8 *stream, int len)
{
    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.find(index)!=mDataMap.end()) {
        SdlAudioDevBuf &devBuf = *(mDataMap.at(index));
        int tLen = 0;
        for(;tLen<len&&devBuf.size();) {
            SampleBuffer* buffer = devBuf.front();
            int remain = buffer->len()-buffer->pos();
            int cpLen = remain>(len-tLen)?(len-tLen):remain;
            memcpy(stream+tLen, buffer->data()+buffer->pos(), cpLen);
            buffer->setPos(buffer->pos()+cpLen);
            if(buffer->pos()>=buffer->len()) {
                delete devBuf.dequeue();
            }
            tLen += cpLen;
        }
        return tLen;
    }
    return 0;
}

int SdlAudioDevice::takeIndex(int index, SampleBuffer *buffer, int len)
{
    std::unique_ptr<Uint8[]> data(new Uint8[len]);
    memset(data.get(), 0, len);

    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.contains(index)) {
        SdlAudioDevBuf &devBuf = *(mDataMap.at(index));
        if(devBuf.size()) {
            SampleBuffer* first = devBuf.front();
            buffer->channels = first->channels;
            buffer->timestamp = first->timestamp;
            buffer->sampleBytes = first->sampleBytes;
            int cp = takeIndex(index, data.get(), len);
            buffer->copy(data.get(), len);
            return cp;
        }
    }
    return 0;
}

int SdlAudioDevice::take(Uint8 *stream, int len)
{
    int dataCnt = 0;
    std::unique_ptr<Uint8[]> data(new Uint8[len]);

    LockedMapLocker lk(mDataMap.mutex());

    //多声源混音
    for(int index : mDataMap.keys()) {
        memset(data.get(), 0, len);
        int cpLen = takeIndex(index, data.get(), len);
        if(cpLen) {
            SDL_MixAudioFormat(stream, data.get(),
                               mDesiredSpec.format, cpLen, SDL_MIX_MAXVOLUME);
            dataCnt++;
        }
    }

    return dataCnt;
}

void SdlAudioDevice::clear(int index)
{
    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.contains(index)) {
        SdlAudioDevBuf *devBuf = mDataMap.at(index);
        devBuf->clear();
        delete devBuf;
        mDataMap.erase(index);
    }
}

void SdlAudioDevice::clearAll()
{
    LockedMapLocker lk(mDataMap.mutex());

    for(int index : mDataMap.keys()) {
        clear(index);
    }
}

bool SdlAudioDevice::isOpened()
{
    return mState==Open||mState==Pause;
}

void SdlAudioDevice::print_status()
{
    switch (SDL_GetAudioDeviceStatus(mDeviceId))
    {
    case SDL_AUDIO_STOPPED: printf("Current Audio Status:stopped\n"); break;
    case SDL_AUDIO_PLAYING: printf("Current Audio Status:playing\n"); break;
    case SDL_AUDIO_PAUSED: printf("Current Audio Status:paused\n"); break;
    default: printf("Current Audio Status:???"); break;
    }
}

int SdlAudioDevice::sampleBytes(int fmt)
{
    return (SDL_AUDIO_MASK_BITSIZE&fmt)/8;
}
