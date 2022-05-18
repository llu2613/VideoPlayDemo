#ifndef SDLEVENTDISPATCHER_H
#define SDLEVENTDISPATCHER_H

#include <QThread>

class SdlEventDispatcher : public QThread
{
    Q_OBJECT
public:
    explicit SdlEventDispatcher(QObject *parent = nullptr);
    ~SdlEventDispatcher();

protected:
    void run();

private:
    bool mIsRunning;
    void iteration();
    const char* devtypestr(int iscapture);

private:
    void eventAudioDeviceAdded(int index, int iscapture);
    void eventAudioDeviceRemoved(int devid);


signals:
    void audioDeviceAdded(int index, int iscapture);
    void audioDeviceRemoved(int devid);

public slots:
};

#endif // SDLEVENTDISPATCHER_H
