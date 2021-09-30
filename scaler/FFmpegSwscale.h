#ifndef FFMPEGSWSCALE_H
#define FFMPEGSWSCALE_H

#include <QObject>

extern "C" {
//编码
#include "libavcodec/avcodec.h"
//封装格式处理
#include "libavformat/avformat.h"
//像素处理
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

class FFmpegSwscale : public QObject
{
    Q_OBJECT
public:
    explicit FFmpegSwscale(QObject *parent = nullptr);
    ~FFmpegSwscale();

    int init(AVCodecContext *pCodecCtx,
                            int out_width, int out_height,
                            enum AVPixelFormat out_fmt);

    int mallocOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size);

    void freeOutFrame(AVFrame **out_frame, uint8_t **out_buffer, int *out_buffer_size);

    int scale(AVFrame *pFrame, int srcSliceH, AVFrame *pOutFrame);

    int outHeight();
    int outWidth();

private:
    struct SwsContext *swsCtx;

    int out_frame_width, out_frame_height;
    enum AVPixelFormat out_frame_fmt;
    int out_count;

signals:

public slots:
};

#endif // FFMPEGSWSCALE_H
