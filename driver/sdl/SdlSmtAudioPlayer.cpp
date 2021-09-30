#include "SdlSmtAudioPlayer.h"
#include <QDebug>

extern "C"{
    #include "SDL_events.h"
}

//单例
SdlSmtAudioPlayer* SdlSmtAudioPlayer::m_instance = nullptr;

SdlSmtAudioPlayer* SdlSmtAudioPlayer::inst()
{
    if(!m_instance) {
        m_instance = new SdlSmtAudioPlayer();
    }
    return m_instance;
}

SdlSmtAudioPlayer::SdlSmtAudioPlayer()
    : SdlAudioPlayer(nullptr)
{
    mThread = new QThread();
    moveToThread(mThread);
    connect(mThread, &QThread::finished, mThread, &QObject::deleteLater);
    connect(this, &SdlSmtAudioPlayer::sigRun, this,&SdlSmtAudioPlayer::run);
    mThread->start();

    mDefaultAudioSpec.freq = 48000;
    mDefaultAudioSpec.format = AUDIO_S16SYS;
    mDefaultAudioSpec.channels = 2;
    mDefaultAudioSpec.silence = 0;
    mDefaultAudioSpec.samples = 1024*2;

    initAudioSystem();

    //循环事件
    emit sigRun();
}

SdlSmtAudioPlayer::~SdlSmtAudioPlayer()
{
    if(mThread) {
        mRunLooping = false;
        mThread->quit();
    }
    quiteAudioSystem();
}

void SdlSmtAudioPlayer::setSync(int sourceId, AVSynchronizer *sync) {
    if(!sync)
        return;

    mSyncMap.lock();
    if(mSyncMap.contains(sourceId))
        qDebug()<<"AVSynchronizer had been set. NOT update!";
    else
        mSyncMap.insert(sourceId, sync);
    mSyncMap.unlock();
}

void SdlSmtAudioPlayer::removeSync(int sourceId)
{
    mSyncMap.lock();
    if(mSyncMap.contains(sourceId))
        mSyncMap.remove(sourceId);
    mSyncMap.unlock();
}

void SdlSmtAudioPlayer::addData(int cardId, int sourceId, MediaData *data)
{
    bool isSoundOpen = false;
    mSourceInfoMap.lock();
    if(mSourceInfoMap.contains(sourceId)) {
        isSoundOpen = mSourceInfoMap[sourceId].isOpenSound;
    }
    mSourceInfoMap.unlock();
	


    unsigned char *buf = data->data[0];
    int len = data->datasize[0];
    double audio_pts_second = data->pts*data->time_base_d;
    if(isSoundOpen) {
        SdlAudioPlayer::addData(cardId, sourceId, (Uint8*)buf, len, audio_pts_second);
    }

    //计算波形图
    std::shared_ptr<SampleBuffer> b(new SampleBuffer);
    b->copy(buf, len);
    b->channels = audioChannels();
    mWaveformProcessor.addSample(sourceId, b);
}

void SdlSmtAudioPlayer::setSourceOpen(int sourceId, bool isOpen)
{
    QMutexLocker locker(&mSourceInfoMap.mutex());

    if(mSourceInfoMap.contains(sourceId)) {
        AudioSource &source = mSourceInfoMap[sourceId];
        source.isOpenSound = isOpen;
    } else {
        AudioSource source;
        source.isOpenSound = isOpen;
        mSourceInfoMap.insert(sourceId, source);
    }
}

QList<int> SdlSmtAudioPlayer::getWaveformData(int sourceId)
{
    return mWaveformProcessor.getShowData(sourceId);
}

void SdlSmtAudioPlayer::clearWaveformData(int sourceId)
{
    mWaveformProcessor.clear(sourceId);
}

int SdlSmtAudioPlayer::audioSamples()
{
    return mDefaultAudioSpec.freq;
}

int SdlSmtAudioPlayer::audioChannels()
{
    return mDefaultAudioSpec.channels;
}

void SdlSmtAudioPlayer::onCardPlaying(std::shared_ptr<QMap<int, std::shared_ptr<SampleBuffer>>> map)
{
    //同步
    mSyncMap.lock();
    for(int idx: map->keys()) {
        if(mSyncMap.contains(idx)&&mSyncMap[idx]) {
            mSyncMap[idx]->setPlayingSample(map->value(idx));
        }
    }
    mSyncMap.unlock();
}


void SdlSmtAudioPlayer::run()
{
    mRunLooping = true;
    while (mRunLooping) {
        SDL_Delay(100);
        iteration();
    }
}

void SdlSmtAudioPlayer::iteration()
{
    int done = 0;
    SDL_Event e;
    SDL_AudioDeviceID dev;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            done = 1;
        } else if (e.type == SDL_KEYUP) {
            if (e.key.keysym.sym == SDLK_ESCAPE)
                done = 1;
        } else if (e.type == SDL_AUDIODEVICEADDED) {
            int index = e.adevice.which;
            int iscapture = e.adevice.iscapture;
            const char *name = SDL_GetAudioDeviceName(index, iscapture);
            if (name != NULL)
                SDL_Log("New %s audio device at index %u: %s\n", devtypestr(iscapture), (unsigned int) index, name);
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Got new %s device at index %u, but failed to get the name: %s\n",
                    devtypestr(iscapture), (unsigned int) index, SDL_GetError());
                continue;
            }
            if (!iscapture) {
                eventAddCard(name);
            }
        } else if (e.type == SDL_AUDIODEVICEREMOVED) {
            dev = (SDL_AudioDeviceID) e.adevice.which;
            SDL_Log("%s device %u removed.\n", devtypestr(e.adevice.iscapture), (unsigned int) dev);
            eventMoveCard(dev);
        }
    }
}


void SdlSmtAudioPlayer::eventAddCard(const char *name)
{
    qDebug()<<"eventAddCard"<<QString(name);

    int cardId = openCard(name, mDefaultAudioSpec);
    if(cardId!=-1) {
        emit audioCardOpened(cardId);
    }
}

void SdlSmtAudioPlayer::eventMoveCard(SDL_AudioDeviceID devid)
{
    qDebug()<<"eventMoveCard"<<QString::number(devid);

    int cardId = getCardIdByDevid(devid);
    if(cardId!=-1) {
        closeCard(cardId);
        emit audioCardClosed(cardId);
    }
}
