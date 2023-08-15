#ifndef FFMPEGRECORDERMULTI_H
#define FFMPEGRECORDERMULTI_H

#include <string>
#include "FFmpegAudioEncoder.h"

class FFmpegRecorderMulti: private FFmpegAudioEncoderCallback
{
public:
    FFmpegRecorderMulti();
    ~FFmpegRecorderMulti();

    void setMaxSeconds(long numeral);

    int open(const char *output,
             int64_t  src_ch_layout,
             enum AVSampleFormat src_sample_fmt,
             int src_sample_rate,
             int64_t  out_ch_layout,
             enum AVSampleFormat out_sample_fmt,
             int out_sample_rate);

    int addData(AVFrame *frame, AVPacket *packet);

    void close();

    bool isRecording();

protected:
    void onRecordError(int level, const char* msg);

private:
    FFmpegAudioEncoder recorder;
    std::string filename;
    long numeral;
    long add_samples;
    int segment_count;
    bool is_open;

    int64_t  src_ch_layout;
    enum AVSampleFormat src_sample_fmt;
    int src_sample_rate;
    int64_t  out_ch_layout;
    enum AVSampleFormat out_sample_fmt;
    int out_sample_rate;

    std::string segmentFileName(std::string filename, int segment);
};

#endif // FFMPEGRECORDERMULTI_H
