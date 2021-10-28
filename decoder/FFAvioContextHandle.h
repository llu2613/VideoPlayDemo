#ifndef FFAVIOCONTEXTHANDLE_H
#define FFAVIOCONTEXTHANDLE_H

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

class FFAvioContextHandle
{
public:
    FFAvioContextHandle(int bufferSize);
    ~FFAvioContextHandle();

    int probe(const char *url);

private:
    AVInputFormat *pInputFmt;
    AVIOContext *pAvioContext;
    unsigned char *pIoBuffer;
    int ioBufferSize;
};

#endif // FFAVIOCONTEXTHANDLE_H
