#include "SdlAudioPlayer.h"
#include <string.h>
#include <QThread>
#include <QMutexLocker>
#include <QDebug>

#define SUB_SYSTEM (SDL_INIT_AUDIO|SDL_INIT_TIMER)

//初始化
SoundCard SdlAudioPlayer::mCardArray[MAX_CARD_NUM];


void SdlAudioPlayer::audioCallback(void *data, Uint8 *stream, int len)
{
    SDL_memset(stream, 0, len);

    SoundCard *card = static_cast<SoundCard*>(data);

    if(!card) {
        qDebug()<<"sdl_audio_callback *data is broken!";
        return;
    }

    SdlAudioPlayer *player = static_cast<SdlAudioPlayer*>(card->player);
/*
    card->mutex.lock();
    if(card->device) {
        int count = 0;
        std::shared_ptr<QMap<int, std::shared_ptr<SampleBuffer>>> map(
                    new QMap<int, std::shared_ptr<SampleBuffer>>);

        SampleBuffer *temp = new SampleBuffer;
        for(int idx: card->device->indexs()) {
            int cp = card->device->takeIndex(idx, temp, len);

            SDL_MixAudioFormat(stream, temp->data(),
                               card->device->format(), cp>len?len:cp, SDL_MIX_MAXVOLUME);

            if(cp>0&&player) {
                std::shared_ptr<SampleBuffer> b(new SampleBuffer);
                b->copy(*temp);
                map->insert(idx, b);
                count++;
            }
        }
        delete temp;

        if(player&&count) {
            player->onCardPlaying(map);
        }
    }
    card->mutex.unlock();
*/

    card->mutex.lock();
    int dataCnt = card->device->take(stream, len);
    if(!dataCnt) {
        card->device->pause();
    }
    card->mutex.unlock();
}

SdlAudioPlayer::SdlAudioPlayer(QObject *parent) : QObject(parent)
{
    mIsInitSys = false;
    mTextCodec = QTextCodec::codecForName("UTF-8");

    for(int i=0; i<MAX_CARD_NUM; i++) {
        mCardArray[i].player = this;
        mCardArray[i].name = QString("NO.%1").arg(i);
    }
}

bool SdlAudioPlayer::initAudioSystem()
{
    /* Load the SDL library */
    if (SDL_InitSubSystem(SUB_SYSTEM) < 0) {
        printf("Couldn't initialize SDL: %s\n", SDL_GetError());
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return false;
    } else{
        printf("init SDL success.\n");
        mIsInitSys = true;
        /* Enable standard application logging */
        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
        return true;
    }
}

void SdlAudioPlayer::quiteAudioSystem()
{
    if(mIsInitSys) {
        SDL_QuitSubSystem(SUB_SYSTEM);
    }
}

void SdlAudioPlayer::addData(int cardId, int sourceId, Uint8 *buf, int bufLen,
                             double timestamp)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        qDebug()<<"Invalid sound card id:"<<cardId;
        return;
    }

    SoundCard &card = mCardArray[cardId];

    card.mutex.lock();
    if(card.enable) {
        if(card.device) {
            card.device->addData(sourceId, buf, bufLen, timestamp);
            card.device->resume();
        } else {
            qDebug()<<"Sound card is not initiailizing, cardId:"<<cardId;
        }
    } else {
        qDebug()<<"SoundCard "<<cardId<<" is disable!";
    }
    card.mutex.unlock();
}

void SdlAudioPlayer::clearData(int cardId, int sourceId)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        qDebug()<<"Invalid sound card id:"<<cardId;
        return;
    }

    SoundCard &card = mCardArray[cardId];

    card.mutex.lock();
    if(card.device) {
        card.device->clear(sourceId);
    } else {
        qDebug()<<"Sound card is not initiailizing, cardId:"<<cardId;
    }
    card.mutex.unlock();
}

void SdlAudioPlayer::clearCardData(int cardId)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        qDebug()<<"Invalid sound card id:"<<cardId;
        return;
    }

    SoundCard &card = mCardArray[cardId];

    card.mutex.lock();
    if(card.device) {
        card.device->clearAll();
    } else {
        qDebug()<<"Sound card is not initiailizing, cardId:"<<cardId;
    }
    card.mutex.unlock();
}

void SdlAudioPlayer::clearSourceData(int sourceId)
{
    for(int i=0; i<MAX_CARD_NUM; i++) {
        SoundCard &card = mCardArray[i];
        card.mutex.lock();
        if(card.device) {
            card.device->clear(sourceId);
        }
        card.mutex.unlock();
    }
}

int SdlAudioPlayer::openCard(QString name,
             int freq,
             SDL_AudioFormat format,
             Uint8 channels,
             Uint16 samples)
{
    SDL_AudioSpec wanted;
    SDL_zero(wanted);
    wanted.freq = freq;
    wanted.format = format;
    wanted.channels = channels;
    wanted.samples = samples;

    return openCard(name, wanted);
}

int SdlAudioPlayer::openCard(QString name, SDL_AudioSpec wanted)
{
    QByteArray name_c = mTextCodec->fromUnicode(name);

    return openCard(name_c.data(), wanted);
}

int SdlAudioPlayer::openCard(const char* name, SDL_AudioSpec wanted)
{
    //找卡
    int findIndex = -1;
    int emptyIndex = -1;
    for(int i=0; i<MAX_CARD_NUM; i++) {
        if(findIndex==-1 && mCardArray[i].device) {
            SoundCard &card = mCardArray[i];
            card.mutex.lock();
            if(!strcmp(card.device->name(), name))
                findIndex = i;
            card.mutex.unlock();
        }
        if(emptyIndex==-1 && !mCardArray[i].device) {
            emptyIndex = i;
        }
    }

    if(findIndex!=-1) {
        SoundCard &card = mCardArray[findIndex];
        card.mutex.lock();
        if(card.device && card.device->isOpened()) {
            qDebug()<<"SoundCard "<<card.name<<" is already opened!";
            return findIndex;
        }
        card.mutex.unlock();
    }

    if(findIndex!=-1) {
        SoundCard &card = mCardArray[findIndex];
        card.mutex.lock();
        if(card.device) {
            wanted.callback = SdlAudioPlayer::audioCallback;
            wanted.userdata = &card;
            card.device->close();
            int ret = card.device->open(name, wanted)>0;
            card.enable = ret>0;
            card.name = mTextCodec->toUnicode(name);
        }
        card.mutex.unlock();
        qDebug()<<"openCard"<<card.name<<findIndex;
        return findIndex;
    } else if(emptyIndex!=-1) {
        SoundCard &card = mCardArray[emptyIndex];
        card.mutex.lock();
        card.device = new SdlAudioDevice(SDL_FALSE);
        if(card.device) {
            wanted.callback = SdlAudioPlayer::audioCallback;
            wanted.userdata = &card;
            int ret = card.device->open(name, wanted)>0;
            card.enable = ret>0;
            card.name = mTextCodec->toUnicode(name);
        }
        card.mutex.unlock();
        qDebug()<<"openCard"<<card.name<<emptyIndex;
        return emptyIndex;
    }

    return -1;
}

void SdlAudioPlayer::pauseCard(int cardId, int ispause)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        qDebug()<<"Invalid sound card id:"<<cardId;
        return;
    }

    SoundCard &card = mCardArray[cardId];
    card.mutex.lock();
    if(card.device) {
        if(ispause)
            card.device->pause();
        else
            card.device->resume();
    }
    card.mutex.unlock();
}

void SdlAudioPlayer::closeCard(int cardId)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        qDebug()<<"Invalid sound card id:"<<cardId;
        return;
    }

    SoundCard &card = mCardArray[cardId];
    card.mutex.lock();
    card.enable = false;
    if(card.device) {
        card.device->close();
        delete card.device;
        card.device = nullptr;
    }
    card.mutex.unlock();
}

int SdlAudioPlayer::bufferSize(int cardId, int sourceId)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        qDebug()<<"Invalid sound card id:"<<cardId;
        return 0;
    }

    SoundCard &card = mCardArray[cardId];

    int size = 0;

    card.mutex.lock();
    if(card.device) {
        size = card.device->bufferSize(sourceId);
    } else {
        qDebug()<<"Sound card is not initiailizing, cardId:"<<cardId;
    }
    card.mutex.unlock();

    return size;
}

void SdlAudioPlayer::test()
{
    SDL_AudioSpec wanted;
    SDL_zero(wanted);
    wanted.freq = 44100;
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 1;
    wanted.samples = 4096;
    wanted.callback = NULL;

    SDL_AudioSpec spec;

    SDL_AudioDeviceID devid_out = SDL_OpenAudioDevice(NULL, SDL_FALSE, &wanted, &spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!devid_out) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for playback: %s!\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }
}

QList<QString> SdlAudioPlayer::devices()
{
    QStringList cardList;
    int n = SDL_GetNumAudioDevices(SDL_FALSE);
    for(int i=0; i<n; i++) {
        const char* name_c = SDL_GetAudioDeviceName(i, SDL_FALSE);
        cardList.append(mTextCodec->toUnicode(name_c));
    }

    return cardList;
}

int SdlAudioPlayer::getCardId(QString name)
{
    QByteArray name_c = mTextCodec->fromUnicode(name);
    for(int i=0; i<MAX_CARD_NUM; i++) {
        SoundCard &card = mCardArray[i];
        QMutexLocker(&card.mutex);
        if(card.device
                && !strcmp(card.device->name(), name_c)) {
            return i;
        }
    }

    return -1;
}

int SdlAudioPlayer::getCardIdByDevid(SDL_AudioDeviceID devid)
{
    if(devid<2) return -1;

    for(int i=0; i<MAX_CARD_NUM; i++) {
        SoundCard &card = mCardArray[i];
        QMutexLocker(&card.mutex);
        if(card.device
                && card.device->devid()==devid) {
            return i;
        }
    }

    return -1;
}

QString SdlAudioPlayer::getCardName(int cardId)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
        SoundCard &card = mCardArray[cardId];
        QMutexLocker(&card.mutex);
        if(card.device) {
            return mTextCodec->toUnicode(card.device->name());
        }
    }

    return "";
}

const char* SdlAudioPlayer::devtypestr(int iscapture)
{
    return iscapture ? "capture" : "output";
}

void SdlAudioPlayer::printDevices()
{
    /* Print available audio drivers */
    int n = SDL_GetNumAudioDrivers();
    if (n == 0) {
        printf("No built-in audio drivers\n\n");
    } else {
        int i;
        printf("Built-in audio drivers:\n");
        for (i = 0; i < n; ++i) {
            printf("  %d: %s\n", i, SDL_GetAudioDriver(i));
        }
        printf("Select a driver with the SDL_AUDIODRIVER environment variable.\n");
    }
    printf("Using audio driver: %s\n\n", SDL_GetCurrentAudioDriver());

    //参数:0表示回放设备,1表示录制设备
    print_devices(0);
    print_devices(1);
}

void SdlAudioPlayer::print_devices(int iscapture)
{
    const char *typestr = ((iscapture) ? "capture" : "output");
    int n = SDL_GetNumAudioDevices(iscapture);

    printf("Found %d %s device%s:\n", n, typestr, n != 1 ? "s" : "");

    if (n == -1)
        printf("  Driver can't detect specific %s devices.\n\n", typestr);
    else if (n == 0)
        printf("  No %s devices found.\n\n", typestr);
    else {
        int i;
        for (i = 0; i < n; i++) {
            const char *name = SDL_GetAudioDeviceName(i, iscapture);
            if (name != NULL)
                printf("  %d: %s\n", i, name);
            else
                printf("  %d Error: %s\n", i, SDL_GetError());
        }
        printf("\n");
    }
}
