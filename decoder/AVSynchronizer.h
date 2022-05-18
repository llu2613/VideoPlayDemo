#ifndef AVSYNCHRONIZER_H
#define AVSYNCHRONIZER_H

#include <mutex>
#include <memory>

class AVSynchronizer
{
public:
    explicit AVSynchronizer();
    ~AVSynchronizer();

    void reset();
    void setAudioTimeBaseD(double time_base_d);
    void setVideoTimeBaseD(double time_base_d);

    void setAudioDecodingTs(unsigned long ts);
    void setVideoDecodingTs(unsigned long ts);
    void setAudioPlayingTs(unsigned long ts);
    void setVideoPlayingTs(unsigned long ts);

    unsigned long audioDecodingTs();
    unsigned long videoDecodingTs();
    unsigned long audioPlayingTs();
    unsigned long videoPlayingTs();

    unsigned long getAudioDelayTs();
    unsigned long getVideoDelayTs();

    double getAudioDelaySec();
    double getVideoDelaySec();

private:
    std::recursive_mutex mMutex;
    unsigned long audio_play_ts, video_play_ts;
    unsigned long audio_decode_ts, video_decode_ts;
    double audio_time_base_d, video_time_base_d;

};

#endif // AVSYNCHRONIZER_H
