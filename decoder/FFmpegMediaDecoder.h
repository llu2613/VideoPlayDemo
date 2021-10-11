#ifndef FFMPEGMEDIADECODER_H
#define FFMPEGMEDIADECODER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QSize>
#include "FFmpegMediaScaler.h"
#include "model/MediaData.h"
#include <memory>
#include <QMetaType>

extern "C" {
//封装格式
#include "libavformat/avformat.h"
//解码
#include "libavcodec/avcodec.h"
//工具
#include "libavutil/error.h"
#include "libavutil/time.h"
}

class FFmpegMediaDecoder : public QObject
{
    Q_OBJECT
public:
    explicit FFmpegMediaDecoder(QObject *parent = nullptr);
    ~FFmpegMediaDecoder();

    void setOutAudio2(int rate, int channels);
    void setOutVideo2(int width, int height);
    void setOutAudio(enum AVSampleFormat fmt, int rate, uint64_t ch_layout);
    void setOutVideo(enum AVPixelFormat fmt, int width, int height);

    void setInputUrl(QString url);

    void startDecoding();

    void stopDecoding();

    void test();

protected:
    int64_t lastFrameRealtime;

private:
    AVFormatContext* pFormatCtx;
    AVCodecContext *pAudioCodecCtx, *pVideoCodecCtx;
    int audio_stream_idx, video_stream_idx;

    long audioFrameCnt, videoFrameCnt;

    FFmpegMediaScaler scaler;

    static int interruptCallback(void *pCtx);

    int open(const char* input);
    void close();
    void decoding();

    void printCodecInfo(AVCodecContext *pCodeCtx);
    void printError(int code, const char* message);
    void print_error(const char *name, int err);

    int openCodec(AVFormatContext *pFormatCtx, int stream_idx,
                  AVCodecContext **pCodeCtx);
    int audioFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    int videoFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);

    QString inputUrl;
    //线程
    QThread *mThread;
    QMutex mStopMutex;
    bool mStopFlag;

    //错误记录
    QString errorMsg;
    int errorCode;

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

#endif // FFMPEGMEDIADECODER_H
