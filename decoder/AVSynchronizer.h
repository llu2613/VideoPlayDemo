#ifndef AVSYNCHRONIZER_H
#define AVSYNCHRONIZER_H

#include <mutex>
#include <memory>
#include "libavutil\rational.h"

class AVSynchronizer
{
public:
    explicit AVSynchronizer();
    ~AVSynchronizer();

    void reset();
    void resetOsTime();
    void setAudioTimeBase(AVRational time_base);
    void setVideoTimeBase(AVRational time_base);

    void setAudioDecodingTs(long ts);
    void setVideoDecodingTs(long ts);

    void setAudioPlayingTs(long ts);
    void setVideoPlayingTs(long ts);

    long audioDecodingTs();
    long videoDecodingTs();

    long audioPlayingTs();
    long videoPlayingTs();

    long audioPlayingMs();
    long videoPlayingMs();

    long getAudioDelayTs();
    long getVideoDelayTs();

    long getAudioDelayMs();
    long getVideoDelayMs();

    long getOsDelayMs();

private:
    std::recursive_mutex mMutex;
    long audio_play_ts, video_play_ts;
    long audio_decode_ts, video_decode_ts;
    AVRational audio_time_base, video_time_base;

    long start_os_msecond;
    long start_audio_decode_ts, start_video_decode_ts;
};

#endif // AVSYNCHRONIZER_H
