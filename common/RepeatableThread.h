#ifndef REPEATABLETHREAD_H
#define REPEATABLETHREAD_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <atomic>

class RepRunnable
{
public:
    virtual void run()=0;
};

class RepeatableThread : public QObject
{
    Q_OBJECT
public:
    explicit RepeatableThread(RepRunnable *runnable);
    ~RepeatableThread();

    void runTh();

    bool isRunning();

private:
    QThread *mThread;
    RepRunnable *mRunnable;
    QMutex mIsRunningMutex;
    bool mIsRunning;

signals:
    void thRun();

private slots:
    void run();

};

#endif // REPEATABLETHREAD_H
