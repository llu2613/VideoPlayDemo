#include "RepeatableThread.h"

RepeatableThread::RepeatableThread(RepRunnable *runnable)
{
    mThread = nullptr;
    mRunnable = runnable;
    mIsRunning = false;

    mThread = new QThread();
    this->moveToThread(mThread);
    connect(this, &RepeatableThread::thRun, this, &RepeatableThread::run, Qt::QueuedConnection);

    mThread->start();
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
