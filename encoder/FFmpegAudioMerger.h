#ifndef FFMPEGAUDIOMERGER_H
#define FFMPEGAUDIOMERGER_H

#include "../decoder/FFmpegMediaDecoder.h"
#include "FFmpegAudioRecorder.h"
#include "FFMergerCallback.h"
#include <mutex>

class FFmpegAudioMerger : private FFmpegMediaDecoder
{
public:
    FFmpegAudioMerger();
    virtual ~FFmpegAudioMerger();

    void start(std::string out_file);

    void setCallback(FFMergerCallback *callback);

    int merge(std::string in_file);

    void finish();

protected:
    int audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    int videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    void audioDecodedData(AVFrame *frame, AVPacket *packet);
    void videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight);

private:
    int openOutput();
    void closeOutput();

    void progress(long current, long total);
    void print_error(int code, std::string msg);

    std::mutex m_mutex;
    std::string out_file;

    bool has_opened;
    FFmpegAudioRecorder mRecoder;
    FFMergerCallback *mCallback;
    int64_t duration;
    int64_t samples;

};

#endif // FFMPEGAUDIOMERGER_H
