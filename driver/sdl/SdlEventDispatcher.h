#ifndef SDLEVENTDISPATCHER_H
#define SDLEVENTDISPATCHER_H

#include <QThread>

class SdlEventDispatcher : public QThread
{
    Q_OBJECT
public:
    explicit SdlEventDispatcher(QObject *parent = nullptr);

protected:
    void run();

private:
    bool mIsRunning;
    void iteration();
    const char* devtypestr(int iscapture);

private:
    void eventAudioDeviceAdded(const char *name);
    void eventAudioDeviceRemoved(uint32_t devid);


signals:
    void audioDeviceAdded(QString name);
    void audioDeviceRemoved(uint32_t devid);

public slots:
};

#endif // SDLEVENTDISPATCHER_H
