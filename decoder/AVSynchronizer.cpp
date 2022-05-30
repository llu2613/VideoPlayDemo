#include "AVSynchronizer.h"
#include <QDateTime>

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
    std::lock_guard<std::recursive_mutex> lk(mMutex);

    audio_play_ts = 0;
    video_play_ts = 0;
    audio_decode_ts = 0;
    video_decode_ts = 0;
    audio_time_base_d = 0;
    video_time_base_d = 0;

    start_os_msecond = 0;
    start_audio_decode_ts = 0;
    start_video_decode_ts = 0;
}

void AVSynchronizer::resetOsTime()
{
    QDateTime dateTime = QDateTime::currentDateTime();
    start_os_msecond = dateTime.toMSecsSinceEpoch();

    start_audio_decode_ts = audio_decode_ts;
    start_video_decode_ts = video_decode_ts;
}

void AVSynchronizer::setAudioTimeBaseD(double time_base_d)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    audio_time_base_d = time_base_d;
}

void AVSynchronizer::setVideoTimeBaseD(double time_base_d)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    video_time_base_d = time_base_d;
}

void AVSynchronizer::setAudioDecodingTs(long ts)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    audio_decode_ts = ts;
}

void AVSynchronizer::setVideoDecodingTs(long ts)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    video_decode_ts = ts;
}

void AVSynchronizer::incAudioDecodingTs(long ts)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    audio_decode_ts += ts;
}

void AVSynchronizer::incVideoDecodingTs(long ts)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    video_decode_ts += ts;
}

void AVSynchronizer::setAudioPlayingTs(long ts)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    audio_play_ts = ts;
}

void AVSynchronizer::setVideoPlayingTs(long ts)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    video_play_ts = ts;
}

void AVSynchronizer::incAudioPlayingTs(long ts)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    audio_play_ts += ts;
}

void AVSynchronizer::incVideoPlayingTs(long ts)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    video_play_ts += ts;
}

long AVSynchronizer::audioDecodingTs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return audio_decode_ts;
}

long AVSynchronizer::videoDecodingTs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return video_decode_ts;
}

long AVSynchronizer::audioPlayingTs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return audio_play_ts;
}

long AVSynchronizer::videoPlayingTs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return video_play_ts;
}

long AVSynchronizer::getAudioDelayTs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return audio_play_ts!=0?audio_decode_ts-audio_play_ts:0;
}

long AVSynchronizer::getVideoDelayTs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return video_play_ts!=0?video_decode_ts-video_play_ts:0;
}

double AVSynchronizer::getAudioDelaySec()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return getAudioDelayTs()*audio_time_base_d;
}

double AVSynchronizer::getVideoDelaySec()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return getVideoDelayTs()*video_time_base_d;
}

double AVSynchronizer::getOsDelaySec()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);

    QDateTime dateTime = QDateTime::currentDateTime();
    long current_os_msecond = dateTime.toMSecsSinceEpoch();

    long a_msec = (long)((audio_decode_ts-start_audio_decode_ts)*audio_time_base_d*1000);
    long v_msec = (long)((video_decode_ts-start_video_decode_ts)*video_time_base_d*1000);
    long os_msec = current_os_msecond-start_os_msecond;

    long a_diff = a_msec>0&&os_msec>0?a_msec-os_msec:0;
    long v_diff = v_msec>0&&os_msec>0?v_msec-os_msec:0;

    if(a_diff>0 && v_diff>0)
        return (a_diff<v_diff?a_diff:v_diff)*1.0/1000;
    if(a_diff>0)
        return a_diff*1.0/1000;
    if(v_diff>0)
        return v_diff*1.0/1000;

    return 0;
}
