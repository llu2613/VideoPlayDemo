#include "SdlAudioDevice.h"
#include <QMutexLocker>
#include <QScopedArrayPointer>
#include <QTextCodec>
#include <QDebug>

#define MAX_FRAME_NUM 3000

SdlAudioDevice::SdlAudioDevice(int iscapture, QObject *parent)
    : QObject(parent), mDataMutex(QMutex::Recursive)
{
    mTextCodec = QTextCodec::codecForName("UTF-8");
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
        qDebug()<<"SDL_AudioDeviceID:"<<mDeviceId;
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
    qDebug()<<"SDL_CloseAudioDevice:"<<mDeviceId;
    mDeviceId = 0;
}

void SdlAudioDevice::addData(int index, Uint8* buf, int len,
                             double timestamp)
{
    SampleBuffer *buffer = new SampleBuffer();
    if(!buffer->copy(buf, len)) {
        qCritical()<<"No memory allocation!";
        return;
    }

    buffer->channels = mDesiredSpec.channels;
    buffer->timestamp = timestamp;
    buffer->sampleBytes = 16/8;

    QMutexLocker locker(&mDataMutex);
    if(mDataMap.contains(index)) {
        mDataMap[index].append(buffer);

        int discard = 0;
        for(int i= mDataMap[index].size(); i>MAX_FRAME_NUM; i--) {
            SampleBuffer *b = mDataMap[index].takeFirst();
            delete b;
            discard++;
        }
        if(discard)
            qDebug()<<"AudioCard buffer discard"<<discard<<"frames,"
                   <<mTextCodec->toUnicode(name())<<"index:"<<index;
    } else {
        mDataMap.insert(index, QList<SampleBuffer*>()<<buffer);
    }

    if(mDataMap[index].length()>MAX_FRAME_NUM*2/3) {
        qDebug()<<"AudioCard buffer is too long:"<<mTextCodec->toUnicode(name())
               <<"index:"<<index<<"buf:"<<len<<"list:"<<mDataMap[index].length();
    }
}

QList<int> SdlAudioDevice::indexs()
{
    QMutexLocker locker(&mDataMutex);
    return mDataMap.keys();
}

int SdlAudioDevice::bufferSize(int index)
{
    QMutexLocker locker(&mDataMutex);
    if(mDataMap.contains(index))
        return mDataMap[index].size();
    return 0;
}

int SdlAudioDevice::takeIndex(int index, Uint8 *stream, int len)
{
    QMutexLocker locker(&mDataMutex);
    if(mDataMap.contains(index)) {
        QList<SampleBuffer*> &list = mDataMap[index];
        int tLen = 0;
        for(;tLen<len&&list.size();) {
            SampleBuffer* buffer = list.first();
            int remain = buffer->len()-buffer->pos();
            int cpLen = remain>(len-tLen)?(len-tLen):remain;
            memcpy(stream+tLen, buffer->data()+buffer->pos(), cpLen);
            buffer->setPos(buffer->pos()+cpLen);
            if(buffer->pos()>=buffer->len()) {
                SampleBuffer *b = list.takeFirst();
                delete b;
            }
            tLen += cpLen;
        }
        if(tLen&&(tLen<len||!list.size()))
            qDebug()<<"sample buffer is too short, index"<<index<<tLen<<len<<list.size();
        return tLen;
    }
    return 0;
}

int SdlAudioDevice::takeIndex(int index, SampleBuffer *buffer, int len)
{
    QScopedArrayPointer<Uint8> data(new Uint8[len]);
    memset(data.data(), 0, len);

    QMutexLocker locker(&mDataMutex);
    if(mDataMap.contains(index)) {
        QList<SampleBuffer*> &list = mDataMap[index];
        if(list.size()) {
            SampleBuffer* first = list.first();
            buffer->channels = first->channels;
            buffer->timestamp = first->timestamp;
            buffer->sampleBytes = first->sampleBytes;
            int cp = takeIndex(index, data.data(), len);
            buffer->copy(data.data(), len);
            return cp;
        }
    }
    return 0;
}

int SdlAudioDevice::take(Uint8 *stream, int len)
{
    int dataCnt = 0;
    QScopedArrayPointer<Uint8> data(new Uint8[len]);
    //多声源混音
    QMutexLocker locker(&mDataMutex);
    for(int index: mDataMap.keys()) {
        memset(data.data(), 0, len);
        int cpLen = takeIndex(index, data.data(), len);
        if(cpLen) {
            SDL_MixAudioFormat(stream, data.data(),
                               mDesiredSpec.format, cpLen, SDL_MIX_MAXVOLUME);
            dataCnt++;
        }
    }

    return dataCnt;
}

void SdlAudioDevice::clear(int index)
{
    QMutexLocker locker(&mDataMutex);
    if(mDataMap.contains(index)) {
        QList<SampleBuffer*> &list = mDataMap[index];
        for(;list.size()>0;) {
            SampleBuffer *b = list.takeFirst();
            delete b;
        }
        mDataMap.remove(index);
    }
}

void SdlAudioDevice::clearAll()
{
    QMutexLocker locker(&mDataMutex);
    for(int index: mDataMap.keys()) {
        clear(index);
    }
    mDataMap.clear();
}

bool SdlAudioDevice::isOpened() {
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
