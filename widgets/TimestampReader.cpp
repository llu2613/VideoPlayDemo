#include "TimestampReader.h"

#include "decoder/FFmpegMediaDecoder.h"
//#include <libavutil/avutil.h>
//#include <libavcodec/avcodec.h>
//#include <libavformat/avformat.h>
//#include <libswscale/swscale.h>

/**
 * https://blog.csdn.net/cjqqschoolqq/article/details/88976112
 */

TimestampReader::TimestampReader(QString url, QObject *parent) : QThread(parent)
{
    mIsRunning = false;
    mUrl = url;

//    av_register_all();
//    avformat_network_init();
    //avformat_network_deinit(); //thread_queue_size
}

void TimestampReader::run()
{
    mIsRunning = true;
    readtimestamp();
}

int TimestampReader::readtimestamp()
{
    AVFormatContext *pFormatCtx;
    int videoindex;
    AVCodecContext *pCodecCtx;
    AVFrame *pFrame;
    AVPacket *packet;
    char filepath[256];
    strcpy_s(filepath, mUrl.toLocal8Bit().data());

    pFormatCtx = avformat_alloc_context();

    AVDictionary* options = NULL;
    av_dict_set(&options, "buffer_size", "1024000", 0);
    av_dict_set(&options, "max_delay", "500000", 0);
    av_dict_set(&options, "stimeout", "20000000", 0);  //设置超时断开连接时间
    av_dict_set(&options, "rtsp_transport", "tcp", 0);  //以udp方式打开，如果以tcp方式打开将udp替换为tcp

    if (avformat_open_input(&pFormatCtx, filepath, NULL, &options) != 0) //打开网络流或文件流
    {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    videoindex = -1;

    videoindex = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                     NULL, 0);
    if (videoindex == -1) {
        printf("Didn't find a video stream.\n");
        return -1;
    }
    pCodecCtx = pFormatCtx->streams[videoindex]->codec;

    pFrame = av_frame_alloc();

    //Output Info---输出一些文件（RTSP）信息
    //        log_d("---------------- File Information (%s)---------------\n",
    //                avcodec_get_name(pCodecCtx->codec_id));

    //        av_dump_format(pFormatCtx, 0, filepath, 0);
    //        log_d("-------------------------------------------------\n");

    packet = (AVPacket *) av_malloc(sizeof(AVPacket));

//    FILE *fpSave;
//    if ((fpSave = fopen("/tmp/geth264.h264", "wb")) == NULL) //h264保存的文件名
//        return 0;
    while(mIsRunning&&!isInterruptionRequested()) {
        //------------------------------
        if (av_read_frame(pFormatCtx, packet) >= 0) {
//            if (packet->stream_index == videoindex) {
//                fwrite(packet->data, 1, packet->size, fpSave); //写数据到文件中
//            }
            emit sigStreamTimestamp(packet->stream_index, packet->pts);
            av_packet_unref(packet);
        } else {
            break;
        }
    }
    //--------------
    //av_frame_free(&pFrameYUV);
    av_free(packet);
    av_frame_free(&pFrame);
    avformat_close_input(&pFormatCtx);

//    log_d("quit");
    return EXIT_SUCCESS;
}
