#ifndef SDLAUDIODEVICE_H
#define SDLAUDIODEVICE_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include "samplebuffer.h"

extern "C"{
    #include "SDL.h"
}

/*无法解析的外部符号 main，该符号在函数 WinMain 中被引用*/
#undef main


class SdlAudioDevice : public QObject
{
    Q_OBJECT
public:
    enum CardState{
        Close, Open, Pause
    };

    explicit SdlAudioDevice(int iscapture=0, QObject *parent = nullptr);

    const char* name();

    SDL_AudioDeviceID devid();

    SDL_AudioFormat format();

    int open(const char *device, SDL_AudioSpec spec);

    void pause();

    void resume();

    void close();

    void addData(int index, Uint8* buf, int len, double timestamp=0);

    QList<int> indexs();

    int bufferSize(int index);

    int takeIndex(int index, Uint8 *stream, int len);
    int takeIndex(int index, SampleBuffer *buffer, int len);

    int take(Uint8 *stream, int len);

    void clear(int index);

    void clearAll();

    bool isOpened();

    void print_status();

signals:

public slots:


private:
    CardState mState;
    int mIscapture;
    QTextCodec *mTextCodec;
    char mDeviceName[256];
    SDL_AudioDeviceID mDeviceId;
    SDL_AudioSpec mDesiredSpec, mObtainedSpec;
    QMutex mDataMutex;
    QMap<int, QList<SampleBuffer*>> mDataMap;

};

#endif // SDLAUDIODEVICE_H