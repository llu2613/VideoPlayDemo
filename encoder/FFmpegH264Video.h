#ifndef FFMPEGH264VIDEO_H
#define FFMPEGH264VIDEO_H

#include "common/ffmpeg_commons.h"

class FFmpegH264Video
{
public:
    FFmpegH264Video();
    virtual ~FFmpegH264Video();

    int h264_avc1(const char *filename, const char *out_file,
                  bool reset_dts=false);

    int h265_avc1(const char *filename, const char *out_file,
                  bool reset_dts=false);

    int h264_ES();
};

#endif // FFMPEGH264VIDEO_H
