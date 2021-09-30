#ifndef SDLAUDIOPLAYER_H
#define SDLAUDIOPLAYER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QTextCodec>
#include <QMutex>
#include "SdlAudioDevice.h"
#include <memory>

extern "C"{
    #include "SDL.h"
}

/*无法解析的外部符号 main，该符号在函数 WinMain 中被引用*/
#undef main

#define MAX_CARD_NUM 16

class SoundCard
{
public:
    bool enable;
    QString name;
    SdlAudioDevice *device;
    QMutex mutex;
    void* player;
    SoundCard() {
        enable = false;
        device = nullptr;
        player = nullptr;
    }
};

class SdlAudioPlayer : public QObject
{
    Q_OBJECT
public:
    explicit SdlAudioPlayer(QObject *parent = nullptr);
    bool initAudioSystem();
    void quiteAudioSystem();
    int openCard(QString name, int freq, SDL_AudioFormat format, Uint8 channels,  Uint16 samples);
    int openCard(QString name, SDL_AudioSpec wanted);
    int openCard(const char* name, SDL_AudioSpec wanted);
    void pauseCard(int cardId, int ispause);
    void closeCard(int cardId);
    void addData(int cardId, int sourceId, Uint8 *buf, int bufLen, double timestamp);
    void clearCardData(int cardId);
    void clearSourceData(int sourceId);
    void clearData(int cardId, int sourceId);

    int bufferSize(int cardId, int sourceId);

    QList<QString> devices();

    int getCardId(QString name);
    QString getCardName(int cardId);
    int getCardIdByDevid(SDL_AudioDeviceID devid);

    void test();

    const char* devtypestr(int iscapture);
    void printDevices();
    void print_devices(int iscapture);

protected:
    static SoundCard mCardArray[MAX_CARD_NUM];
    static void audioCallback(void *data, Uint8 *stream, int len);

    //线程调用，注意加锁和实时返回
    virtual void onCardPlaying(std::shared_ptr<QMap<int, std::shared_ptr<SampleBuffer>>> map)=0;

private:
    bool mIsInitSys;
    QTextCodec *mTextCodec;


signals:

private slots:

};

#endif // SDLAUDIOPLAYER_H
