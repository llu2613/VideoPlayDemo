#ifndef YUV_PCM_FILE_H
#define YUV_PCM_FILE_H

#include <string.h>
#include <stdio.h>
#include "ffmpeg_commons.h"

class yuv_pcm_file
{
public:
    yuv_pcm_file();

    void set_pcm_file(const char* filename);
    void set_yuv_file(const char* filename);

    void write_pcm_data(unsigned char *buf, int len);
    void write_yuv_data(unsigned char **data, int width, int height);

    void finish_pcm_file();
    void finish_yuv_file();

private:
    bool enable_pcm;
    bool enable_yuv;
    FILE *fp_pcm, *fp_yuv;
    char pcm_file[1024];
    char yuv_file[1024];
};

#endif // YUV_PCM_FILE_H
