#ifndef FFPRINTINFO_H
#define FFPRINTINFO_H

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/mathematics.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/timestamp.h"
}

class FFPrintInfo
{
public:
    FFPrintInfo(const char *tag);

    void printFormatContext(AVFormatContext* pfmtCxt, int is_output);
    void printStream(AVStream* stream);
    void printAudioStream(AVStream* stream);
    void printVideoStream(AVStream* stream);

    void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt);

private:
    char m_tag[128];
};

#endif // FFPRINTINFO_H
