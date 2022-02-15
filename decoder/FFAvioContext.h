#ifndef FFAVIOCONTEXT_H
#define FFAVIOCONTEXT_H

#include <stdint.h>

extern "C" {
//封装格式
#include "libavformat/avformat.h"
//解码
#include "libavcodec/avcodec.h"
//工具
#include "libavutil/error.h"
#include "libavutil/time.h"
}

class FFAvioContext
{
public:
    FFAvioContext(int bufferSize);
    ~FFAvioContext();

    int probe(const char *url);

private:
    AVInputFormat *pInputFmt;
    AVIOContext *pAvioContext;
    unsigned char *pIoBuffer;
    int ioBufferSize;
};

#endif // FFAVIOCONTEXT_H
