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
    void start(std::string out_file,
               int out_nb_channels,
               enum AVSampleFormat out_sample_fmt,
               int out_sample_rate);

    void setCallback(FFMergerCallback *callback);

    int merge(std::string in_file);
    int merge(std::string in_file, int skipSec, int totalSec);

    void finish();

protected:
    int audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    int videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    void audioDecodedData(AVFrame *frame, AVPacket *packet);
    void videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight);

private:
    int openOutput();
    void closeOutput();
    int init_input_frame(AVFrame **frame,
            AVCodecContext *input_codec_context, int frame_size);
    int write_frame_fifo(AVFrame *frame);
    int write_samples_to_fifo(AVAudioFifo *fifo,
                              uint8_t **converted_input_samples,
                              const int nb_samples);
    int fifo_write_encoder(int frame_size);
    int flush_sample_fifo();

    void progress(long current, long total);
    void print_error(int code, std::string msg);

    std::mutex m_mutex;
    std::string out_file;
    int out_nb_channels;
    enum AVSampleFormat out_sample_fmt;
    int out_sample_rate;
    int64_t skipSamples;
    int64_t mergeSamples;

    bool has_opened;
    FFmpegAudioRecorder mRecoder;
    FFMergerCallback *mCallback;
    AVAudioFifo *sample_fifo;
    int64_t duration;
    int64_t samples;

};

#endif // FFMPEGAUDIOMERGER_H
