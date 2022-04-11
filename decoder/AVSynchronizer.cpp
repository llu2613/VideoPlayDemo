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

    audio_play_ts = 0;
    video_play_ts = 0;
    audio_decode_ts = 0;
    video_decode_ts = 0;
}

void AVSynchronizer::setAudioDecodingTs(unsigned long ts)
{
    std::lock_guard<std::mutex> lk(mMutex);

    audio_decode_ts = ts;
}

void AVSynchronizer::setVideoDecodingTs(unsigned long ts)
{
    std::lock_guard<std::mutex> lk(mMutex);

    video_decode_ts = ts;
}

void AVSynchronizer::setAudioPlayingTs(unsigned long ts)
{
    std::lock_guard<std::mutex> lk(mMutex);

    audio_play_ts = ts;
}

void AVSynchronizer::setVideoPlayingTs(unsigned long ts)
{
    std::lock_guard<std::mutex> lk(mMutex);

    video_play_ts = ts;
}

unsigned long AVSynchronizer::audioDecodingTs()
{
    std::lock_guard<std::mutex> lk(mMutex);

    return audio_decode_ts;
}

unsigned long AVSynchronizer::videoDecodingTs()
{
    std::lock_guard<std::mutex> lk(mMutex);

    return video_decode_ts;
}

unsigned long AVSynchronizer::audioPlayingTs()
{
    std::lock_guard<std::mutex> lk(mMutex);

    return audio_play_ts;
}

unsigned long AVSynchronizer::videoPlayingTs()
{
    std::lock_guard<std::mutex> lk(mMutex);

    return video_play_ts;
}

unsigned long AVSynchronizer::getAudioDelayTs()
{
    std::lock_guard<std::mutex> lk(mMutex);

    return audio_play_ts!=0?audio_decode_ts-audio_play_ts:0;
}

unsigned long AVSynchronizer::getVideoDelayTs()
{
    std::lock_guard<std::mutex> lk(mMutex);

    return video_play_ts!=0?video_decode_ts-video_play_ts:0;
}
