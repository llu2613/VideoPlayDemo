#include "RepeatableThread.h"

RepeatableThread::RepeatableThread(RepRunnable *runnable)
{
    mThread = nullptr;
    mRunnable = runnable;
    mIsRunning = false;
}

RepeatableThread::~RepeatableThread()
{
    if(mThread) {
        mThread->quit();
        mThread->terminate();
        mThread->deleteLater();
    }
}

void RepeatableThread::runTh()
{
    if(!mThread) {
        mThread = new QThread();
        this->moveToThread(mThread);
        mThread->start();
        connect(this, &RepeatableThread::thRun, this, &RepeatableThread::run);
    }
    emit thRun();
}

bool RepeatableThread::isRunning()
{
    QMutexLocker locker(&mIsRunningMutex);
    return mIsRunning;
}

void RepeatableThread::run()
{
    mIsRunningMutex.lock();
    mIsRunning = true;
    mIsRunningMutex.unlock();

    if(mRunnable)
        mRunnable->run();

    mIsRunningMutex.lock();
    mIsRunning = false;
    mIsRunningMutex.unlock();
}
