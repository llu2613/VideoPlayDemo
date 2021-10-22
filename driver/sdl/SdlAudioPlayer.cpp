#include "SdlAudioPlayer.h"
#include <string.h>

#define INFO(fmt,...) SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)
#define WARNING(fmt,...) SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)
#define ERROR(fmt,...) SDL_LogError(SDL_LOG_CATEGORY_AUDIO, fmt, ##__VA_ARGS__)

#define SUB_SYSTEM (SDL_INIT_AUDIO|SDL_INIT_TIMER)

//初始化
SoundCard SdlAudioPlayer::mCardArray[MAX_CARD_NUM];


void SdlAudioPlayer::audioCallback(void *data, Uint8 *stream, int len)
{
    SDL_memset(stream, 0, len);

    SoundCard *card = static_cast<SoundCard*>(data);

    if(!card) {
        ERROR("sdl_audio_callback *data is broken!");
        return;
    }

    SdlAudioPlayer *player = static_cast<SdlAudioPlayer*>(card->player);
//方法一
    card->mutex.lock();
    if(card->device) {
        std::map<int, std::shared_ptr<SampleBuffer>> dataMap;

        SampleBuffer *temp = new SampleBuffer;
        for(int idx: card->device->indexs()) {
            int cp = card->device->takeIndex(idx, temp, len);
            if(cp>0) {
                std::shared_ptr<SampleBuffer> b(new SampleBuffer);
                b->copy(*temp);
                dataMap.insert(std::pair<int, std::shared_ptr<SampleBuffer>>(idx,b));
            }
        }
        delete temp;

        if(player&&dataMap.size()) {
            player->onCardDataPrepared(card->id, dataMap);
        }

        for(std::map<int, std::shared_ptr<SampleBuffer>>::iterator it=dataMap.begin();
                 it!=dataMap.end(); ++it) {
            std::shared_ptr<SampleBuffer> buffer = it->second;
            SDL_MixAudioFormat(stream, buffer->data(),
                               card->device->format(), buffer->len(), SDL_MIX_MAXVOLUME);
        }
    }
    card->mutex.unlock();
//方法二
//    card->mutex.lock();
//    int dataCnt = card->device->take(stream, len);
//    if(!dataCnt) {
//        card->device->pause();
//    }
//    card->mutex.unlock();
}

SdlAudioPlayer::SdlAudioPlayer()
{
    mIsInitSys = false;

    for(int i=0; i<MAX_CARD_NUM; i++) {
        mCardArray[i].player = this;
        mCardArray[i].id = i;
    }
    initAudioSystem();
}

SdlAudioPlayer::~SdlAudioPlayer()
{
    quiteAudioSystem();
}

bool SdlAudioPlayer::initAudioSystem()
{
    if(mIsInitSys)
        return true;

    /* Load the SDL library */
    if (SDL_InitSubSystem(SUB_SYSTEM) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return false;
    } else{
        mIsInitSys = true;
        /* Enable standard application logging */
        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
        return true;
    }
}

void SdlAudioPlayer::quiteAudioSystem()
{
    if(mIsInitSys) {
        mIsInitSys = false;
        SDL_QuitSubSystem(SUB_SYSTEM);
    }
}

void SdlAudioPlayer::addData(int cardId, int sourceId, Uint8 *buf, int bufLen,
                             double timestamp)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        WARNING("Invalid sound card id:%d", cardId);
        return;
    }

    SoundCard &card = mCardArray[cardId];

    card.mutex.lock();
    if(card.enable) {
        if(card.device) {
            card.device->addData(sourceId, buf, bufLen, timestamp);
            card.device->resume();
        } else {
            WARNING("Sound card is not initiailizing, cardId:%d", cardId);
        }
    } else {
        WARNING("SoundCard %d is disable!", cardId);
    }
    card.mutex.unlock();
}

void SdlAudioPlayer::clearData(int cardId, int sourceId)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        WARNING("Invalid sound card id:%d", cardId);
        return;
    }

    SoundCard &card = mCardArray[cardId];

    card.mutex.lock();
    if(card.device) {
        card.device->clear(sourceId);
    } else {
        INFO("Sound card is not initiailizing, cardId:%d", cardId);
    }
    card.mutex.unlock();
}

void SdlAudioPlayer::clearCardData(int cardId)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        WARNING("Invalid sound card id:%d", cardId);
        return;
    }

    SoundCard &card = mCardArray[cardId];

    card.mutex.lock();
    if(card.device) {
        card.device->clearAll();
    } else {
        INFO("Sound card is not initiailizing, cardId:%d", cardId);
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

int SdlAudioPlayer::openCard(
            std::string name,
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

int SdlAudioPlayer::openCard(std::string name, SDL_AudioSpec wanted)
{
    const char* name_c = name.c_str();
    //找卡
    int findIndex = -1;
    int emptyIndex = -1;
    for(int i=0; i<MAX_CARD_NUM; i++) {
        if(findIndex==-1 && mCardArray[i].device) {
            SoundCard &card = mCardArray[i];
            card.mutex.lock();
            if(!strcmp(card.device->name(), name_c))
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
            INFO("SoundCard %s had been opened!", name);
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
            int ret = card.device->open(name_c, wanted)>0;
            card.enable = ret>0;
            card.name = name;
            INFO("openCard: %s, cardId: %d", name, findIndex);
        }
        card.mutex.unlock();
        return findIndex;
    } else if(emptyIndex!=-1) {
        SoundCard &card = mCardArray[emptyIndex];
        card.mutex.lock();
        card.device = new SdlAudioDevice(SDL_FALSE);
        if(card.device) {
            wanted.callback = SdlAudioPlayer::audioCallback;
            wanted.userdata = &card;
            int ret = card.device->open(name_c, wanted)>0;
            card.enable = ret>0;
            card.name = name;
            INFO("openCard: %s, cardId: %d", name, emptyIndex);
        }
        card.mutex.unlock();

        return emptyIndex;
    }

    return -1;
}

void SdlAudioPlayer::pauseCard(int cardId, int ispause)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
    } else {
        WARNING("Invalid sound card id:%d", cardId);
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
        WARNING("Invalid sound card id:%d", cardId);
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
        WARNING("Invalid sound card id:%d", cardId);
        return 0;
    }

    SoundCard &card = mCardArray[cardId];

    int size = 0;

    card.mutex.lock();
    if(card.device) {
        size = card.device->bufferSize(sourceId);
    } else {
        INFO("Sound card is not initiailizing, cardId:%d", cardId);
    }
    card.mutex.unlock();

    return size;
}

void SdlAudioPlayer::onCardDataPrepared(int cardId, std::map<int, std::shared_ptr<SampleBuffer>> &dataMap)
{
    //播放数据回传
    //线程调用，注意加锁和实时返回
}

std::list<std::string> SdlAudioPlayer::devices()
{
    std::list<std::string> list;
    int n = SDL_GetNumAudioDevices(SDL_FALSE);
    for(int i=0; i<n; i++) {
        std::string name = SDL_GetAudioDeviceName(i, SDL_FALSE);
        list.push_back(name);
    }

    return list;
}

int SdlAudioPlayer::getCardId(std::string name)
{
    for(int i=0; i<MAX_CARD_NUM; i++) {
        SoundCard &card = mCardArray[i];

        std::lock_guard<std::mutex> lk(card.mutex);

        if(card.device
                && !name.compare(card.device->name())) {
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

        std::lock_guard<std::mutex> lk(card.mutex);

        if(card.device
                && card.device->devid()==devid) {
            return i;
        }
    }

    return -1;
}

std::string SdlAudioPlayer::getCardName(int cardId)
{
    if(cardId>=0&&cardId<MAX_CARD_NUM) {
        SoundCard &card = mCardArray[cardId];

        std::lock_guard<std::mutex> lk(card.mutex);

        if(card.device) {
            return card.device->name();
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
