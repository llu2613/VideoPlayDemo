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
        protected FFmpegMediaDecoder,
        private RepRunnable
{
    Q_OBJECT
public:
    enum Event{
        Unknown=0,
        OpenSuccess, //打开文件成功
        OpenFailure, //打开文件失败
        DecodingFault, //解码失败
        DecoderFinish, //解码完成
        SeekFrame, //定位播放
        Reconnect, //尝试重连
        ReconnectMaxFauilure //重连失败
    };
    explicit StreamMediaDecoder(QObject *parent = nullptr);
    ~StreamMediaDecoder();

    void setOutVideoFmt(enum AVPixelFormat fmt);
    void setOutVideo(enum AVPixelFormat fmt, int width, int height);
    void setOutAudio2(int rate, int channels);
    void setOutVideo2(int width, int height);
    std::shared_ptr<AVSynchronizer> &getSynchronizer();

    void startPlay(QString url, bool isHwaccels);

    void stopPlay();

    void seek(int64_t seekFrame);

    void test();

protected:
    void onDecodeError(int code, std::string msg);
    void printInfo(const char* message) override;
    void printError(const char* message) override;
    void audioDecodedData(AVFrame *frame, AVPacket *packet) override;
    void videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight) override;

    void audioDataReady(std::shared_ptr<MediaData> data) override;
    void videoDataReady(std::shared_ptr<MediaData> data) override;

private:
    //线程
    RepeatableThread mThread;
    QMutex mStopMutex;
    bool mStopFlag;
    std::shared_ptr<AVSynchronizer> syncer;
    int64_t start_audio_ts, start_video_ts;
    int64_t audio_ts, video_ts;

    int64_t seek_time;

    QString mInputUrl;
    bool mIsHwaccels;

    //TEST
    FILE *fp_pcm, *fp_yuv;

signals:
    void mediaDecoderEvent(int event, QString msg);
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
