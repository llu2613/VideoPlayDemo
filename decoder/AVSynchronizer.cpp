#include "AVSynchronizer.h"

/**
 * ffmpeg pts*base算出来是秒
 */
AVSynchronizer::AVSynchronizer()
{
    reset();
}

AVSynchronizer::~AVSynchronizer()
{

}

void AVSynchronizer::reset()
{
    std::lock_guard<std::mutex> lk(mMutex);

    audio_timestamp = 0;
    video_timestamp = 0;
    decode_timestamp = 0;
}

void AVSynchronizer::setDecodingTs(double time)
{
    std::lock_guard<std::mutex> lk(mMutex);

    decode_timestamp = time;
}

void AVSynchronizer::setAudioPlayingTs(int64_t pts, double time_base)
{
    setAudioPlayingTs(pts*time_base);
}
void AVSynchronizer::setAudioPlayingTs(double time)
{
    std::lock_guard<std::mutex> lk(mMutex);

    audio_timestamp = time;
}

void AVSynchronizer::setVideoPlayingTs(int64_t pts, double time_base)
{
    setVideoPlayingTs(pts*time_base);
}
void AVSynchronizer::setVideoPlayingTs(double time)
{
    std::lock_guard<std::mutex> lk(mMutex);

    video_timestamp = time;
}

double AVSynchronizer::getAudioDecodeDelay()
{
    std::lock_guard<std::mutex> lk(mMutex);

    if(audio_timestamp) {
        return decode_timestamp-audio_timestamp;
    }
    return 0;
}

double AVSynchronizer::getVideoDecodeDelay()
{
    std::lock_guard<std::mutex> lk(mMutex);

    if(video_timestamp) {
        return decode_timestamp-video_timestamp;
    }
    return 0;
}

double AVSynchronizer::getAudioVideoDelay()
{
    std::lock_guard<std::mutex> lk(mMutex);

    if(audio_timestamp&&video_timestamp) {
        return audio_timestamp-video_timestamp;
    }
    return 0;
}
