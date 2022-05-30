#ifndef STREAMMEDIADECODER_H
#define STREAMMEDIADECODER_H

#include <QObject>
#include <QMutex>
#include <QMetaType>
#include <memory>
#include "FFmpegMediaDecoder.h"
#include "common/RepeatableThread.h"
#include "AVSynchronizer.h"

class StreamMediaDecoder : public QObject,
        protected FFmpegMediaDecoderCallback,
        private RepRunnable
{
    Q_OBJECT
public:
    explicit StreamMediaDecoder(QObject *parent = nullptr);
    ~StreamMediaDecoder();

    void setOutAudio2(int rate, int channels);
    void setOutVideo2(int width, int height);
    std::shared_ptr<AVSynchronizer> &getSynchronizer();

    void setInputUrl();

    void startPlay(QString url, bool isHwaccels);

    void stopPlay();

    void test();

protected:
    void onDecodeError(int code, std::string msg);
    void onAudioDataReady(std::shared_ptr<MediaData> data);
    void onVideoDataReady(std::shared_ptr<MediaData> data);

private:
    FFmpegMediaDecoder mediaDecoder;
    //线程
    RepeatableThread mThread;
    QMutex mStopMutex;
    bool mStopFlag;
    std::shared_ptr<AVSynchronizer> syncer;
    unsigned long audio_ts, video_ts;

    QString mInputUrl;
    bool mIsHwaccels;

    //TEST
    FILE *fp_pcm, *fp_yuv;

signals:
    void runDecoding();
    //发送数据
    void audioData(std::shared_ptr<MediaData>);
    void videoData(std::shared_ptr<MediaData>);

public slots:

private slots:
    void run();
};

//qRegisterMetaType<std::shared_ptr<MediaData>>("std::shared_ptr<MediaData>");
Q_DECLARE_METATYPE(std::shared_ptr<MediaData>)

#endif // STREAMMEDIADECODER_H
