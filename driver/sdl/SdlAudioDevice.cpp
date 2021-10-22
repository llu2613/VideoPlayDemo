#include "SdlAudioDevice.h"
#include <memory>

#define INFO(fmt,...) SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)
#define WARNING(fmt,...) SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)
#define ERROR(fmt,...) SDL_LogError(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)

#define MAX_FRAME_NUM 3000

SdlAudioDevice::SdlAudioDevice(int iscapture)
{
    mDeviceId = 0;
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
    buffer->sampleBytes = 16/8;

    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.find(index)!=mDataMap.end()) {
        mDataMap[index].push_back(buffer);

        int discard = 0;
        for(int i= mDataMap[index].size(); i>MAX_FRAME_NUM; i--) {
            delete mDataMap[index].front();
            mDataMap[index].pop_front();
            discard++;
        }
        if(discard)
            WARNING("AudioCard buffer discard %d frames, %s index:%d",
                   discard, name(), index);
    } else {
        std::list<SampleBuffer*> list;
        list.push_back(buffer);
        mDataMap.insert(index, list);
    }

//    if(mDataMap[index].size()>MAX_FRAME_NUM*2/3) {
//        INFO("AudioCard buffer is too long:%s index: %d buf:%d list:%d",
//               name(), index, len, mDataMap[index].size());
//    }
}

std::list<int> SdlAudioDevice::indexs()
{
    LockedMapLocker lk(mDataMap.mutex());

    std::list<int> keys;
    for(std::map<int, std::list<SampleBuffer*>>::iterator it=mDataMap.begin();
             it!=mDataMap.end(); ++it) {
        keys.push_back(it->first);
    }

    return keys;
}

int SdlAudioDevice::bufferSize(int index)
{
    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.find(index)!=mDataMap.end())
        return mDataMap[index].size();
    return 0;
}

int SdlAudioDevice::takeIndex(int index, Uint8 *stream, int len)
{
    LockedMapLocker lk(mDataMap.mutex());

    if(mDataMap.find(index)!=mDataMap.end()) {
        std::list<SampleBuffer*> &list = mDataMap[index];
        int tLen = 0;
        for(;tLen<len&&list.size();) {
            SampleBuffer* buffer = list.front();
            int remain = buffer->len()-buffer->pos();
            int cpLen = remain>(len-tLen)?(len-tLen):remain;
            memcpy(stream+tLen, buffer->data()+buffer->pos(), cpLen);
            buffer->setPos(buffer->pos()+cpLen);
            if(buffer->pos()>=buffer->len()) {
                delete list.front();
                list.pop_front();
            }
            tLen += cpLen;
        }
//        if(tLen&&(tLen<len||!list.size()))
//            INFO("sample buffer is too short, index: %d, (%d,%d) list:%d",
//                   index, tLen, len, list.size());
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
        std::list<SampleBuffer*> &list = mDataMap[index];
        if(list.size()) {
            SampleBuffer* first = list.front();
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
    for(std::map<int, std::list<SampleBuffer*>>::iterator it=mDataMap.begin();
             it!=mDataMap.end(); ++it) {
        int index = it->first;
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

    if(mDataMap.find(index)!=mDataMap.end()) {
        std::list<SampleBuffer*> &list = mDataMap[index];
        for(;list.size()>0;) {
            delete list.front();
            list.pop_front();
        }
        mDataMap.erase(index);
    }
}

void SdlAudioDevice::clearAll()
{
    LockedMapLocker lk(mDataMap.mutex());

    for(std::map<int, std::list<SampleBuffer*>>::iterator it=mDataMap.begin();
             it!=mDataMap.end(); ++it) {
        int index = it->first;
        clear(index);
    }
    mDataMap.clear();
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
