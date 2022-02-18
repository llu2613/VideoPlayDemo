﻿#ifndef FFMPEGAUDIORECORDER_H
#define FFMPEGAUDIORECORDER_H

#include "common/ffmpeg_commons.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
}

class FFmpegAudioRecorder
{
public:
    FFmpegAudioRecorder();
    ~FFmpegAudioRecorder();

    int open(const char *output,
             int64_t  src_ch_layout,
             enum AVSampleFormat src_sample_fmt,
             int src_sample_rate);

    int open(const char *output,
             int64_t  src_ch_layout,
             enum AVSampleFormat src_sample_fmt,
             int src_sample_rate,
             int64_t  out_ch_layout,
             enum AVSampleFormat out_sample_fmt,
             int out_sample_rate);

    int addData(AVFrame *frame, AVPacket *packet);

    void close();

    bool isReady();

private:
    int init_output_frame(AVFrame **frame,
        AVCodecContext *output_codec_context, int frame_size);
    int write_samples_to_fifo(AVAudioFifo *fifo,
        uint8_t **converted_input_samples,
        const int nb_samples);
    int encode_write_frame_fifo(AVFrame *filt_frame, unsigned int stream_index);
    int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index);
    int flush_audio_fifo(int stream_index);
    int flush_encoder(AVFormatContext *ofmt_ctx,int stream_index);

    AVFormatContext *ofmt_ctx;
    AVStream *out_stream;
    AVCodecContext *enc_ctx;

    SwrContext *swr_ctx;
    uint8_t **audio_data;
    int audio_data_size;

    AVAudioFifo *audio_fifo;
    int audio_pts;

};

#endif // FFMPEGAUDIORECORDER_H