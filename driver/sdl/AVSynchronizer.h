#ifndef AVSYNCHRONIZER_H
#define AVSYNCHRONIZER_H

#include <mutex>
#include <memory>
#include "SampleBuffer.h"

class AVSynchronizer
{
public:
    explicit AVSynchronizer();

    void setPlayingSample(std::shared_ptr<SampleBuffer> buffer);

private:
    std::mutex mSampleMutex;
    SampleBuffer mSampleBuffer;

};

#endif // AVSYNCHRONIZER_H
