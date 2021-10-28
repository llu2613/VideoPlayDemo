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
    void setDecodingTs(double time);
    void setAudioPlayingTs(int64_t pts, double time_base);
    void setAudioPlayingTs(double time);
    void setVideoPlayingTs(int64_t pts, double time_base);
    void setVideoPlayingTs(double time);
    double getAudioDecodeDelay();
    double getVideoDecodeDelay();
    double getAudioVideoDelay();

private:
    std::mutex mMutex;
    double audio_timestamp, video_timestamp, decode_timestamp;

};

#endif // AVSYNCHRONIZER_H
