#include "AVSynchronizer.h"

AVSynchronizer::AVSynchronizer()
{

}

void AVSynchronizer::setPlayingSample(std::shared_ptr<SampleBuffer> buffer)
{
    std::lock_guard<std::mutex> lk(mSampleMutex);

    mSampleBuffer.copy(*buffer.get());
}
