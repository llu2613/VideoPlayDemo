#ifndef AVSYNCHRONIZER_H
#define AVSYNCHRONIZER_H

#include <QObject>
#include <QMutex>
#include <memory>
#include "SampleBuffer.h"

class AVSynchronizer : public QObject
{
    Q_OBJECT
public:
    explicit AVSynchronizer(QObject *parent = nullptr);

    void setPlayingSample(std::shared_ptr<SampleBuffer> buffer);

private:
    QMutex mSampleMutex;
    SampleBuffer mSampleBuffer;

signals:

public slots:
};

#endif // AVSYNCHRONIZER_H
