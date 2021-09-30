#include "AVSynchronizer.h"

AVSynchronizer::AVSynchronizer(QObject *parent) : QObject(parent)
{

}

void AVSynchronizer::setPlayingSample(std::shared_ptr<SampleBuffer> buffer)
{
    QMutexLocker locker(&mSampleMutex);
    mSampleBuffer.copy(*buffer.get());
}
