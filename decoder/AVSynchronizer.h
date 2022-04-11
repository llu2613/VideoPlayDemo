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

private:
    std::mutex mMutex;
    unsigned long audio_play_ts, video_play_ts;
    unsigned long audio_decode_ts, video_decode_ts;

};

#endif // AVSYNCHRONIZER_H
