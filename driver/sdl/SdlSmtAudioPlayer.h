#ifndef SDLSMTAUDIOPLAYER_H
#define SDLSMTAUDIOPLAYER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include "LockedMap.h"
#include "SdlAudioPlayer.h"
#include "model/MediaData.h"
#include "WaveformProcessor.h"
#include "AVSynchronizer.h"

class SdlSmtAudioPlayer : public SdlAudioPlayer
{
    Q_OBJECT
    struct AudioSource
    {
        bool isOpenSound;
        AudioSource() {
            isOpenSound = false;
        }
    };
public:
    //单例
    static SdlSmtAudioPlayer* inst();

    void setSync(int sourceId, AVSynchronizer *sync);
    void removeSync(int sourceId);
    void addData(int cardId, int sourceId, MediaData *data);

    void setSourceOpen(int sourceId, bool isOpen);

    QList<int> getWaveformData(int sourceId);
    void clearWaveformData(int sourceId);

    int audioSamples();
    int audioChannels();


protected:
    void onCardPlaying(std::shared_ptr<QMap<int, std::shared_ptr<SampleBuffer>>> map);

private:
    explicit SdlSmtAudioPlayer();
    ~SdlSmtAudioPlayer();

    QThread *mThread;
    bool mRunLooping = false;
    SDL_AudioSpec mDefaultAudioSpec;
    LockedMap<int, AVSynchronizer*> mSyncMap;
    LockedMap<int, AudioSource> mSourceInfoMap;
    WaveformProcessor mWaveformProcessor;

    void iteration();
    void eventAddCard(const char *name);
    void eventMoveCard(SDL_AudioDeviceID devid);

    //单例
    static SdlSmtAudioPlayer *m_instance;

signals:
    void sigRun();
    //声卡信号
    void audioCardOpened(int cardId);
    void audioCardClosed(int cardId);

private slots:
    void run();

};

#endif // SDLSMTAUDIOPLAYER_H
