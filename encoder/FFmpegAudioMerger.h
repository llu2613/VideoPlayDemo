#ifndef FFMPEGAUDIOMERGER_H
#define FFMPEGAUDIOMERGER_H

#include "../decoder/FFmpegMediaDecoder.h"
#include "FFmpegAudioRecorder.h"

class FFmpegAudioMerger : private FFmpegMediaDecoder
{
public:
    FFmpegAudioMerger();
    ~FFmpegAudioMerger();

    void start(std::string out_file);

    int merge(std::string in_file);

    void finish();

protected:
    void audioDecodedData(AVFrame *frame, AVPacket *packet);
    void videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight);

private:
    int openOutput();
    void closeOutput();

    std::string out_file;

    bool has_opened;
    FFmpegAudioRecorder mRecoder;

};

#endif // FFMPEGAUDIOMERGER_H
