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

    memset(&audio_time_base, 0, sizeof(audio_time_base));
    memset(&video_time_base, 0, sizeof(video_time_base));

    start_audio_decode_ts = 0;
    start_video_decode_ts = 0;

    QDateTime dateTime = QDateTime::currentDateTime();
    start_os_msecond = dateTime.toMSecsSinceEpoch();
}

void AVSynchronizer::resetOsTime()
{
    QDateTime dateTime = QDateTime::currentDateTime();
    start_os_msecond = dateTime.toMSecsSinceEpoch();

    start_audio_decode_ts = audio_decode_ts;
    start_video_decode_ts = video_decode_ts;
}

void AVSynchronizer::setAudioTimeBase(AVRational time_base)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    audio_time_base = time_base;
    if(start_audio_decode_ts==0)
        start_audio_decode_ts = audio_decode_ts;
}

void AVSynchronizer::setVideoTimeBase(AVRational time_base)
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    video_time_base = time_base;
    if(start_video_decode_ts==0)
        start_video_decode_ts = video_decode_ts;
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

long AVSynchronizer::audioPlayingMs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    double audio_time_base_d = av_q2d(audio_time_base);
    long msec = (long)(audio_play_ts*audio_time_base_d*1000);
    return msec;
}

long AVSynchronizer::videoPlayingMs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    double video_time_base_d = av_q2d(video_time_base);
    long msec = (long)(video_play_ts*video_time_base_d*1000);
    return msec;
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

long AVSynchronizer::getAudioDelayMs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return getAudioDelayTs() * av_q2d(audio_time_base);
}

long AVSynchronizer::getVideoDelayMs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);
    return getVideoDelayTs() * av_q2d(video_time_base);
}

long AVSynchronizer::getOsDelayMs()
{
    std::lock_guard<std::recursive_mutex> lk(mMutex);

    long delayed = 0;
    QDateTime dateTime = QDateTime::currentDateTime();
    long current_os_msecond = dateTime.toMSecsSinceEpoch();

    double audio_time_base_d = av_q2d(audio_time_base);
    double video_time_base_d = av_q2d(video_time_base);
    long a_msec = (long)((audio_decode_ts-start_audio_decode_ts)*audio_time_base_d*1000);
    long v_msec = (long)((video_decode_ts-start_video_decode_ts)*video_time_base_d*1000);
    long os_msec = current_os_msecond-start_os_msecond;

    long a_diff = a_msec>0&&os_msec>0?a_msec-os_msec:0;
    long v_diff = v_msec>0&&os_msec>0?v_msec-os_msec:0;

    if(a_diff>0 && v_diff>0)
        delayed = (a_diff<v_diff?a_diff:v_diff);
    if(a_diff>0)
        delayed = a_diff;
    if(v_diff>0)
        delayed = v_diff;

    return delayed;
}
