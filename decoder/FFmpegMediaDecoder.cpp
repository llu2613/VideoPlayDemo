#include "FFmpegMediaDecoder.h"
#include <stdio.h>
#include <stdlib.h>

//20s超时退出
#define INTERRUPT_TIMEOUT (10 * 1000 * 1000)


static char* _av_ts2str(int64_t ts) {
    char av_error[AV_TS_MAX_STRING_SIZE] = {0};
    return av_ts_make_string(av_error, ts);
}
static char* _av_ts2timestr(int64_t ts, AVRational *tb) {
    char av_error[AV_TS_MAX_STRING_SIZE] = {0};
    return av_ts_make_time_string(av_error, ts, tb);
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           _av_ts2str(pkt->pts), _av_ts2timestr(pkt->pts, time_base),
           _av_ts2str(pkt->dts), _av_ts2timestr(pkt->dts, time_base),
           _av_ts2str(pkt->duration), _av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

FFmpegMediaDecoder::FFmpegMediaDecoder()
{
    mCallback = nullptr;

    memset(mTag, 0, sizeof(mTag));
}

FFmpegMediaDecoder::~FFmpegMediaDecoder()
{

}

int FFmpegMediaDecoder::open(const char* input, bool hwaccels)
{
    audioFrameCnt = 0;
    videoFrameCnt = 0;
    audio_pts = 0;
    video_pts = 0;

    return FFMediaDecoder::open(input, NULL, hwaccels);
}

int FFmpegMediaDecoder::open(const char* input, AVDictionary *dict, bool hwaccels)
{
    return FFMediaDecoder::open(input, dict, hwaccels);
}

int FFmpegMediaDecoder::decoding()
{
    return FFMediaDecoder::decoding();
}

void FFmpegMediaDecoder::close()
{
    FFMediaDecoder::close();

    std::lock_guard<std::mutex> lk(scaler_mutex);
    scaler.freeAudioResample();
    scaler.freeVideoScale();
}

void FFmpegMediaDecoder::getOutAudioParams(enum AVSampleFormat *fmt,
                                           int *rate, int *channels)
{
    if(fmt)
        *fmt = scaler.outAudioFmt();
    if(rate)
        *rate = scaler.outAudioRate();
    if(channels)
        *channels = scaler.outAudioChannels();
}

void FFmpegMediaDecoder::getOutVideoParams(enum AVPixelFormat *fmt,
                                           int *width, int *height)
{
    if(fmt)
        *fmt = scaler.outVideoFmt();
    if(width)
        *width = scaler.outVideoWidth();
    if(height)
        *height = scaler.outVideoHeight();
}

void FFmpegMediaDecoder::setOutAudio(enum AVSampleFormat fmt, int rate, int channels)
{
    std::lock_guard<std::mutex> lk(scaler_mutex);

    scaler.setOutAudio(fmt, rate, channels);
}

void FFmpegMediaDecoder::setOutVideo(enum AVPixelFormat fmt, int width, int height)
{
    std::lock_guard<std::mutex> lk(scaler_mutex);

    scaler.setOutVideo(fmt, width, height);
}

int FFmpegMediaDecoder::audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    audioFrameCnt++;
    return FFMediaDecoder::audioRawFrame(pCodecCtx, frame, packet);
}

int FFmpegMediaDecoder::videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    videoFrameCnt++;
    return FFMediaDecoder::videoRawFrame(pCodecCtx, frame, packet);
}

void FFmpegMediaDecoder::audioDecodedData(AVFrame *frame, AVPacket *packet)
{
    int out_samples, out_buffer_size;
    uint8_t *pAudioOutBuffer = NULL;

    if (frame->format == AV_SAMPLE_FMT_NONE || frame->sample_rate == 0
            || frame->channels == 0 || frame->nb_samples == 0) {
        printError("audioDecodedData, break frame!");
        return;
    }
    scaler_mutex.lock();
    if (scaler.isAudioReady() && scaler.srcAudioFmt() == frame->format
            &&scaler.srcAudioRate() == frame->sample_rate&&scaler.srcAudioChannels() == frame->channels) {
    } else {
        scaler.freeAudioResample();
        int ret = scaler.initAudioResample((enum AVSampleFormat)frame->format, frame->sample_rate, frame->channels,
                                           scaler.outAudioFmt(), scaler.outAudioRate(), scaler.outAudioChannels());
        if(ret<0)
            printError("initAudioResample failed!");
    }

    pAudioOutBuffer = scaler.audioConvert(frame, &out_samples, &out_buffer_size);

    //写入文件进行测试
//        if(fp_pcm) {
//            fwrite(pAudioOutBuffer, 1, out_buffer_size, fp_pcm);
//        }
    if(pAudioOutBuffer) {
        audioResampledData(packet, pAudioOutBuffer, out_buffer_size, out_samples);
    }
    scaler_mutex.unlock();
}

void FFmpegMediaDecoder::videoDecodedData(AVFrame *frame, AVPacket *packet, int pixelHeight)
{
    //AVFrame转为像素格式YUV420，宽高
    //2 6输入、输出数据
    //3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
    //4 输入数据第一列要转码的位置 从0开始
    //5 输入画面的高度
    int out_height = 0;
    AVFrame* out_frame = NULL;

    if (frame->format == AV_PIX_FMT_NONE || frame->width == 0 || frame->height == 0) {
        printError("videoDecodedData, break frame!");
        return;
    }

    scaler_mutex.lock();
    if(scaler.isVideoReady()&&scaler.srcVideoFmt()==frame->format
            &&scaler.srcVideoWidth()==frame->width&&scaler.srcVideoHeight()==frame->height) {
    } else {
        scaler.freeVideoScale();
        int ret = scaler.initVideoScale((enum AVPixelFormat)frame->format, frame->width, frame->height);
        if(ret<0)
            printError("initVideoScale failed!");
    }

    if(scaler.outVideoFmt()==frame->format
            &&scaler.outVideoWidth()==frame->width&&scaler.outVideoHeight()==frame->height) {
        out_frame = frame;
        out_height = pixelHeight;
    } else {
        out_frame = scaler.videoScale(pixelHeight, frame, &out_height);
    }

//    AVFrame *pFrameYUV = out_frame;
//        if(fp_yuv) {
//            //输出到YUV文件
//            //AVFrame像素帧写入文件
//            //data解码后的图像像素数据（音频采样数据）
//            //Y 亮度 UV 色度（压缩了） 人对亮度更加敏感
//            //U V 个数是Y的1/4
//            int y_size = pVideoScale->width() * pVideoScale->height();
//            fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);
//            fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);
//            fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);
//        }

    if (out_frame) {
        videoScaledData(out_frame, packet, out_height);
    }
    scaler_mutex.unlock();
}

void FFmpegMediaDecoder::audioResampledData(AVPacket *packet, uint8_t *sampleBuffer,
    int bufferSize, int samples)
{
    try {
        std::shared_ptr<MediaData> mediaData(new MediaData);
        mediaData->sample_format = scaler.outAudioFmt();
        mediaData->nb_samples = samples;
        mediaData->sample_rate = scaler.outAudioRate();
        mediaData->channels = scaler.outAudioChannels();
        mediaData->pts = audio_pts;
        audio_pts += packet->duration;
        mediaData->duration = packet->duration;
        const AVStream *stream = audioStream();
        mediaData->time_base_d = av_q2d(stream->time_base);
        //帧数据
        mediaData->data[0] = new unsigned char[bufferSize];
        mediaData->datasize[0] = bufferSize;
        memcpy(mediaData->data[0], sampleBuffer, bufferSize);
        audioDataReady(mediaData);
    } catch(...) {
        printInfo("audioDecodedData failed share data!");
    }
}

void FFmpegMediaDecoder::videoScaledData(AVFrame *frame, AVPacket *packet, int pixelHeight)
{
    try {
        std::shared_ptr<MediaData> mediaData(new MediaData);
        mediaData->pixel_format = frame->format;
        mediaData->width = frame->width;
        mediaData->height = pixelHeight;
        mediaData->pts = video_pts;
        video_pts += packet->duration;
        mediaData->duration = packet->duration;
        mediaData->repeat_pict = frame->repeat_pict; //重复显示次数
        mediaData->best_timestamp = frame->best_effort_timestamp;
        mediaData->key_frame = (frame->key_frame==1||frame->pict_type==AV_PICTURE_TYPE_I);
        const AVStream *stream = videoStream();
        mediaData->time_base_d = av_q2d(stream->time_base);
        mediaData->frame_rate = av_q2d(stream->avg_frame_rate);
        //一帧图像（音频）的时间戳（时间戳一般以第一帧为0开始）
        //时间戳 = pts * (AVRational.num/AVRational.den)
        //int second= pFrame->pts * av_q2d(stream->time_base);
        if(frame->format==AV_PIX_FMT_YUV420P) {
            fillPixelYUV420P(mediaData.get(), frame, frame->width, pixelHeight);
        } else if(frame->format==AV_PIX_FMT_RGB24) {
            fillPixelRGB24(mediaData.get(), frame, frame->width, pixelHeight);
        }
        videoDataReady(mediaData);
    } catch(...) {
        printInfo("videoDecodedData failed share data!");
    }
}

/*
FFMPEG 实现 YUV，RGB各种图像原始数据之间的转换（swscale）
https://blog.csdn.net/x_iya/article/details/52299058
*/
void FFmpegMediaDecoder::fillPixelRGB24(MediaData *mediaData, AVFrame *frame,
                                        int pixelWidth, int pixelHeight)
{
//    fwrite(pFrameYUV->data[0],(pCodecCtx->width)*(pCodecCtx->height)*3,1,output);

    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pixelWidth, pixelHeight, 1);
    mediaData->data[0] = new uint8_t[num_bytes];
    mediaData->datasize[0] = num_bytes;
    mediaData->linesize[0] = frame->linesize[0];
    mediaData->linesize[1] = frame->linesize[1];
    mediaData->linesize[2] = frame->linesize[2];
    memcpy(mediaData->data[0], frame->data[0], num_bytes);
}

void FFmpegMediaDecoder::fillPixelYUV420P(MediaData *mediaData, AVFrame *frame,
                                         int pixelWidth, int pixelHeight)
{
//    fwrite(pFrameYUV->data[0],(pCodecCtx->width)*(pCodecCtx->height),1,output);
//    fwrite(pFrameYUV->data[1],(pCodecCtx->width)*(pCodecCtx->height)/4,1,output);
//    fwrite(pFrameYUV->data[2],(pCodecCtx->width)*(pCodecCtx->height)/4,1,output);

    int y_size = pixelWidth * pixelHeight;
    int dataSize[] = {y_size, y_size / 4, y_size / 4};
    for(int i=0; i<3; i++) {
        mediaData->data[i] = new uint8_t[dataSize[i]];
        mediaData->datasize[i] = dataSize[i];
        mediaData->linesize[i] = frame->linesize[i];
        memcpy(mediaData->data[i], frame->data[i], dataSize[i]);
    }
}

void FFmpegMediaDecoder::audioDataReady(std::shared_ptr<MediaData> data)
{
    std::lock_guard<std::mutex> lk(mCallbackMutex);

    if(mCallback)
        mCallback->onAudioDataReady(data);
}

void FFmpegMediaDecoder::videoDataReady(std::shared_ptr<MediaData> data)
{
    std::lock_guard<std::mutex> lk(mCallbackMutex);

    if(mCallback)
        mCallback->onVideoDataReady(data);
}

void FFmpegMediaDecoder::setTag(const char* tag)
{
    sprintf(mTag, "%s", tag);
}

const char* FFmpegMediaDecoder::tag()
{
    return mTag;
}

void FFmpegMediaDecoder::setCallback(FFmpegMediaDecoderCallback* callback)
{
    std::lock_guard<std::mutex> lk(mCallbackMutex);
    mCallback = callback;
}

void FFmpegMediaDecoder::printInfo(const char* message)
{
    mCallbackMutex.lock();
    if(mCallback)
        mCallback->onDecodeError(0, message);
    mCallbackMutex.unlock();
}

void FFmpegMediaDecoder::printError(const char* message)
{
    mCallbackMutex.lock();
    if(mCallback)
        mCallback->onDecodeError(0, message);
    mCallbackMutex.unlock();
}
