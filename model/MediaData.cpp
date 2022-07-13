#include "MediaData.h"

MediaData::MediaData()
{
    for(int i=0; i<MAX_DATA_ARRAY; i++) {
        data[i] = nullptr;
        datasize[i] = 0;
        linesize[i] = 0;
    }

    //audio
    sample_format = 0;
    channels = 0;
    nb_samples = 0;
    sample_rate = 0;

    //video
    pixel_format = 0;
    width = 0;
    height = 0;
    frame_rate = 0;
    key_frame = 0;

    pts = 0;
    duration = 0;
    time_base_d = 0;
}

MediaData::~MediaData()
{
    release();
}

void MediaData::release()
{
    for(int i=0; i<MAX_DATA_ARRAY; i++) {
        if(data[i]) {
            delete data[i];
            data[i] = nullptr;
            datasize[i] = 0;
            linesize[i] = 0;
        }
    }
}

void MediaData::copy(const MediaData &src)
{
    pts = src.pts;
    duration = src.duration;
    time_base_d = src.time_base_d;

    //audio
    sample_format = src.sample_format;
    channels = src.channels;
    nb_samples = src.nb_samples;
    sample_rate = src.sample_rate;
    //video
    pixel_format = src.pixel_format;
    width = src.width;
    height = src.height;
    frame_rate = src.frame_rate;

    for(int i=0; i<MAX_DATA_ARRAY; i++) {
        if(src.data[i]) {
            if(data[i]) {
                delete data[i];
            }
            data[i] = new uint8_t[src.datasize[i]];
            if(data[i]) {
                memcpy(data[i], src.data[i], src.datasize[i]);
                datasize[i] = src.datasize[i];
            } else {
                datasize[i] = 0;
            }
        }
        linesize[i] = src.linesize[i];
    }
}
