#include "FFAvioContextHandle.h"

int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    return 0;
}

int write_packet(void *opaque, uint8_t *buf, int buf_size)
{
    return 0;
}

int64_t seek(void *opaque, int64_t offset, int whence)
{
    return 0;
}

FFAvioContextHandle::FFAvioContextHandle(int bufferSize)
{
    pInputFmt = nullptr;
    pIoBuffer = (unsigned char*)av_malloc(bufferSize);
    if(pIoBuffer) {
        ioBufferSize = bufferSize;
    }

    pAvioContext = avio_alloc_context(pIoBuffer, ioBufferSize, 0, this,
                                      read_packet, write_packet, seek);
}

FFAvioContextHandle::~FFAvioContextHandle()
{
    if(pIoBuffer)
        av_free(pIoBuffer);

    if(pAvioContext)
        avio_context_free(&pAvioContext);
}

int FFAvioContextHandle::probe(const char *url)
{
    //step1:申请一个AVIOContext
//    pb = avio_alloc_context(buf, BUF_SIZE, 0, NULL, read_data, NULL, NULL);
//    if (!pb) {
//    fprintf(stderr, "avio alloc failed!\n");
//    return -1;
//    }

    //step2:探测流格式
    if (av_probe_input_buffer(pAvioContext, &pInputFmt, url, NULL, 0, 0) < 0) {
        fprintf(stderr, "probe failed!\n");
        return -1;
    } else {
        fprintf(stdout, "probe success!\n");
        fprintf(stdout, "format: %s[%s]\n", pInputFmt->name, pInputFmt->long_name);
    }
    return 0;
}
