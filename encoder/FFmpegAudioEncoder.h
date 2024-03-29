﻿#ifndef FFMPEGAUDIOENCODER_H
#define FFMPEGAUDIOENCODER_H

#include <mutex>
#include "common/ffmpeg_commons.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
}

class FFmpegAudioEncoderCallback
{
public:
    virtual void onRecordError(int level, const char* msg)=0;
};

class FFmpegAudioEncoder
{
public:
    FFmpegAudioEncoder();
    ~FFmpegAudioEncoder();

    int open(const char *output,
             int64_t  src_ch_layout,
             enum AVSampleFormat src_sample_fmt,
             int src_sample_rate);

    int open(const char *output,
             int64_t  src_ch_layout, enum AVSampleFormat src_sample_fmt, int src_sample_rate,
             int64_t  out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate);

    int open(const char *output, const char *format_name, enum AVCodecID codec_id,
             int64_t  src_ch_layout, enum AVSampleFormat src_sample_fmt, int src_sample_rate,
             int64_t  out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate);

    int addData(AVFrame *frame, AVPacket *packet);

    void close();

    void setCallback(FFmpegAudioEncoderCallback* callback);

    void setLogLevel(int level);
    bool isReady();

protected:
    virtual void print_errmsg(int level, const char* msg);

private:
    int init_output_frame(AVFrame **frame,
        AVCodecContext *output_codec_context, int frame_size);
    int write_samples_to_fifo(AVAudioFifo *fifo,
        uint8_t **converted_input_samples,
        const int nb_samples);
    int encode_write_frame_fifo(AVFrame *filt_frame, int stream_index);
    int encode_write_frame(AVFrame *filt_frame, int stream_index);
    int flush_audio_fifo(int stream_index);
    int flush_encoder(AVFormatContext *ofmt_ctx,int stream_index);
    void av_log(void *avcl, int level, const char *fmt, ...);
    void dump_codec(AVCodec* codec);
    void statistics();

    std::mutex mCallbackMutex;
    FFmpegAudioEncoderCallback *mCallback;

    AVFormatContext *ofmt_ctx;
    AVStream *out_stream;
    AVCodecContext *enc_ctx;

    SwrContext *swr_ctx;
    uint8_t **audio_data;
    int audio_data_size;

    AVAudioFifo *audio_fifo;
    int64_t audio_pts, write_pts;
    int64_t enc_samples;
    int log_level;

};

#endif // FFMPEGAUDIOENCODER_H
