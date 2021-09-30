#ifndef WAVEFORMPROCESSOR_H
#define WAVEFORMPROCESSOR_H

#include <QObject>
#include <QThread>
#include "LockedMap.h"
#include "SampleBuffer.h"
#include <memory>
#include <QMutex>
#include <QWaitCondition>

class WaveformProcessor : public QObject
{
    Q_OBJECT
public:
    explicit WaveformProcessor(QObject *parent = nullptr);

    void addSample(int sourceId, std::shared_ptr<SampleBuffer> buffer);
    QList<int> getShowData(int sourceId);
    void clear(int sourceId);

private:
    QThread *mThread;
    LockedMap<int, QList<std::shared_ptr<SampleBuffer>>> mDataMap;
    LockedMap<int, QList<int>> mShowData;

    void addData(int sourceId, SampleBuffer *buffer);
    void addWave(int sourceId, int wave);

signals:
    void sigRun();

private slots:
    void run();
};

#endif // WAVEFORMPROCESSOR_H
