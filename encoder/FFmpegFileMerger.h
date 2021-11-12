#ifndef FFMPEGFILEMERGER_H
#define FFMPEGFILEMERGER_H

#include <list>
#include <string>
#include <mutex>
#include "../decoder/FFmpegMediaDecoder.h"


class FFmpegFileMerger : private FFmpegMediaDecoder
{
public:
    explicit FFmpegFileMerger();
    virtual ~FFmpegFileMerger();

    void start(std::string out_file);

    int merge(std::string in_file);

    void finish();

    void test();

protected:
    int audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);
    int videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet);

private:
    //std::list<std::string> in_file_list;
    std::string out_file;

    bool has_opened;
    bool extra_audio_data;
    bool extra_video_data;
    int frame_count;
    int samples_count;
    long audio_total_pts;
    long video_total_pts;
    AVFormatContext *output_format_ctx;
    AVStream *out_audio_stream;
    AVStream *out_video_stream;
    AVCodecContext *out_audio_codec_ctx;
    AVCodecContext *out_video_codec_ctx;
    AVBSFContext *video_bsf_ctx;
    AVBSFContext *audio_bsf_ctx;

    int openOutput();
    void closeOutput();

    void run(std::list<std::string> in_files, std::string out_file);

};

#endif // FFMPEGFILEMERGER_H
