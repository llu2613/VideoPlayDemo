﻿#ifndef SDLAUDIOPLAYER_H
#define SDLAUDIOPLAYER_H

#include <list>
#include <map>
#include <mutex>
#include <string>
#include "SdlAudioDevice.h"
#include <memory>

extern "C"{
    #include "SDL.h"
}

/*无法解析的外部符号 main，该符号在函数 WinMain 中被引用*/
#undef main

#define MAX_CARD_NUM 16
#define MAX_CARD_VOLUME SDL_MIX_MAXVOLUME

class SoundCard
{
public:
    int id;
    bool enable;
    std::string name;
    SdlAudioDevice *device;
    std::mutex mutex;
    void* player;
    SoundCard() {
        id = -1;
        enable = false;
        device = nullptr;
        player = nullptr;
    }
};

class SdlAudioPlayer
{
public:
    explicit SdlAudioPlayer();
    ~SdlAudioPlayer();

    int openCard(const char* name,
                 int freq,
                 SDL_AudioFormat format,
                 Uint8 channels,
                 Uint16 samples, int maxMemory);
    int openCard(const char* name, SDL_AudioSpec wanted, int maxMemory);
    void pauseCard(int cardId, int ispause);
    void closeCard(int cardId);
    void addData(int cardId, int sourceId, Uint8 *buf, int bufLen, unsigned long timestamp);
    int volume(int cardId);
    void setVolume(int cardId, int volume);
    void clearCardData(int cardId);
    void clearSourceData(int sourceId);
    void clearData(int cardId, int sourceId);

    long bufferSize(int cardId, int sourceId);
    long memorySize(int cardId, int sourceId);
    long sampleSize(int cardId, int sourceId);

    std::list<std::string> devices();

    int getCardId(const char* name);
    std::string getCardName(int cardId);
    int getCardIdByDevid(SDL_AudioDeviceID devid);

    const char* devtypestr(int iscapture);
    void print_card_info(int cardId);
    void printDevices();
    void print_devices(int iscapture);

protected:
    static SoundCard mCardArray[MAX_CARD_NUM];
    static void audioCallback(void *data, Uint8 *stream, int len);

    bool initAudioSystem();
    void quiteAudioSystem();

    //线程调用，注意加锁和实时返回
    virtual void onCardDataPrepared(int cardId, std::map<int, std::shared_ptr<SampleBuffer>> &dataMap);

private:
    bool mIsInitSys;

};

#endif // SDLAUDIOPLAYER_H
