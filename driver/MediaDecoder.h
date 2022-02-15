#ifndef MEDIADECODER_H
#define MEDIADECODER_H

#include <QObject>
#include "decoder/FFmpegMediaDecoder.h"
#include "common/RepeatableThread.h"

class MediaDecoder : public FFmpegMediaDecoder,
        virtual private RepRunnable
{
public:
    explicit MediaDecoder();
    ~MediaDecoder();

    void startPlay(QString uri, bool isHwaccels);
    bool stopPlay();

    bool isRunning();

protected:
    void run();

private:
    RepeatableThread mThread;
    QString mInputUrl;
    bool mStopFlag;
    int mReconnectCount;

signals:

public slots:
};

#endif // MEDIADECODER_H
