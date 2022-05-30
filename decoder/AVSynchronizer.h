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
    void resetOsTime();
    void setAudioTimeBaseD(double time_base_d);
    void setVideoTimeBaseD(double time_base_d);

    void setAudioDecodingTs(long ts);
    void setVideoDecodingTs(long ts);
    void incAudioDecodingTs(long ts);
    void incVideoDecodingTs(long ts);

    void setAudioPlayingTs(long ts);
    void setVideoPlayingTs(long ts);
    void incAudioPlayingTs(long ts);
    void incVideoPlayingTs(long ts);

    long audioDecodingTs();
    long videoDecodingTs();
    long audioPlayingTs();
    long videoPlayingTs();

    long getAudioDelayTs();
    long getVideoDelayTs();

    double getAudioDelaySec();
    double getVideoDelaySec();

    double getOsDelaySec();

private:
    std::recursive_mutex mMutex;
    long audio_play_ts, video_play_ts;
    long audio_decode_ts, video_decode_ts;
    double audio_time_base_d, video_time_base_d;

    long start_os_msecond;
    long start_audio_decode_ts, start_video_decode_ts;
};

#endif // AVSYNCHRONIZER_H
