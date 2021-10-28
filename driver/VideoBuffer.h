#ifndef VIDEOBUFFER_H
#define VIDEOBUFFER_H

#include <QThread>
#include <QList>
#include <QMutex>
#include "model/MediaData.h"
#include "decoder/AVSynchronizer.h"
#include <memory>

class VideoBuffer : public QThread
{
    Q_OBJECT
public:
    explicit VideoBuffer(QString tag, QObject *parent = nullptr);
    ~VideoBuffer();

    void setSync(AVSynchronizer *sync);
    void addData(MediaData *data);

    void clear();

private:
    QString mTag;
    int mFrameRate;
    QMutex mDataMutex;
    QList<MediaData*> mDataList;
    long mMemorySize;

    QMutex mSyncMutex;
    AVSynchronizer *mSync;

    bool isThRunning;
    void run();

    long calcMemorySize(MediaData *data);

signals:
    void showFrame(std::shared_ptr<MediaData> data);

};

#endif // VIDEOBUFFER_H
