#include "MediaDecoder.h"
#include <QElapsedTimer>

MediaDecoder::MediaDecoder()
    : mThread(this)
{

}

MediaDecoder::~MediaDecoder()
{

}

void MediaDecoder::startPlay(QString uri, bool isHwaccels)
{
    mInputUrl = uri;
    //mIsHwaccels = isHwaccels;

    mThread.runTh();
}

bool MediaDecoder::stopPlay()
{
    mStopFlag = true;

    for(int i=0; i<100&&isRunning(); i++)
        QThread::usleep(20);

    return !isRunning();
}

bool MediaDecoder::isRunning()
{
    return mThread.isRunning();
}

void MediaDecoder::run()
{
    while (!mStopFlag) {

    }
}
