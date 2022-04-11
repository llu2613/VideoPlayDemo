#ifndef SMTAUDIOPLAYER_H
#define SMTAUDIOPLAYER_H

#include <QObject>
#include <QTextCodec>
#include "SdlAudioPlayer.h"
#include "WaveformProcessor.h"
#include "decoder/AVSynchronizer.h"
#include "SdlEventDispatcher.h"
#include "model/MediaData.h"

class SmtAudioPlayer : public QObject, virtual public SdlAudioPlayer
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
    static SmtAudioPlayer* inst();

    void initialize();

    void setSync(int sourceId, AVSynchronizer *sync);
    void removeSync(int sourceId);
    void addData(int cardId, int sourceId, MediaData *data);

    void setSoundOpen(int sourceId, bool isOpen);
    int cardVolume(int cardId);
    void setCardVolume(int cardId, int volume);

    QList<int> getWaveformData(int sourceId);
    void clearWaveformData(int sourceId);

    int audioSamples();
    int audioChannels();
    QStringList devices();
    void clearSourceData(int sourceId);
    void clearData(int cardId, int sourceId);


protected:
    void onCardDataPrepared(int cardId, std::map<int, std::shared_ptr<SampleBuffer>> &dataMap);

private:
    explicit SmtAudioPlayer(QObject *parent = nullptr);
    ~SmtAudioPlayer();

    SDL_AudioSpec mAudioSpec;
    QTextCodec *mTextCodec;
    LockedMap<int, AVSynchronizer*> mSyncMap;
    LockedMap<int, AudioSource> mSourceInfoMap;
    WaveformProcessor mWaveformProcessor;
    SdlEventDispatcher mSdlEventDispatcher;
    //单例
    static SmtAudioPlayer *m_instance;

signals:
    //声卡信号
    void audioCardOpened(QString name, int cardId);
    void audioCardClosed(QString name, int cardId);

public slots:
    void eventAudioDeviceAdded(QString name);
    void eventAudioDeviceRemoved(uint32_t devid);
};

#endif // SMTAUDIOPLAYER_H
