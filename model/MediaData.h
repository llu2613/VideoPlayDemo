#ifndef MEDIADATA_H
#define MEDIADATA_H

#include <memory>

#define MAX_DATA_ARRAY 8

class MediaData
{
public:
    explicit MediaData();
    ~MediaData();

    void release();

    void copy(const MediaData &src);

    unsigned char *data[MAX_DATA_ARRAY];
    int datasize[MAX_DATA_ARRAY];
    int linesize[MAX_DATA_ARRAY];

    int64_t pts;
    int64_t duration;
    int64_t timestamp;
    double time_base_d;
    //audio
    int sample_format;
    int channels;
    int nb_samples;
    int sample_rate;
    //video
    int pixel_format;
    int width;
    int height;
    int repeat_pict;
    int frame_rate;
    int key_frame;

};

#endif // MEDIADATA_H
