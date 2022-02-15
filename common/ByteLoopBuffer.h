#ifndef BYTELOOPBUFFER_H
#define BYTELOOPBUFFER_H

#include <cstring>
#include <stdio.h>
#include "ring_buffer.h"

class ByteLoopBuffer
{
public:
    ByteLoopBuffer() {
        mRingBuffer = NULL;
        element_count = 0;
    }
    ByteLoopBuffer(int size) {
        mRingBuffer = NULL;
        element_count = 0;
        alloc(size);
    }
    ~ByteLoopBuffer() {
		if(mRingBuffer)
			WebRtc_FreeBuffer(mRingBuffer);
        mRingBuffer = NULL;
        element_count = 0;
    }

    void alloc(int size) {
        if (mRingBuffer==NULL) {
            mRingBuffer = aWebRtc_CreateBuffer(size, sizeof(unsigned char));
            WebRtc_InitBuffer(mRingBuffer);
            element_count = size;
        }
    }

    int push(unsigned char *pda, int len) {
        int wLen = WebRtc_WriteBuffer(mRingBuffer, pda, len);
        if (wLen != len) {
            int availablewritesize = WebRtc_available_write(mRingBuffer);
            printf("ringbuffer availablewritesize %d \n", availablewritesize);
        }
        return wLen;
    }

    int pop(unsigned char *pda, int len) {
        int readavailable = WebRtc_available_read(mRingBuffer);
        int cpLen = readavailable>len?len:readavailable;
        int rLen = WebRtc_ReadBuffer(mRingBuffer, NULL, pda, cpLen);
        return rLen;
    }

    int len() {
        return WebRtc_available_read(mRingBuffer);
    }
    int siz() {
        return element_count;
    }
    int remain() {
        return element_count-len();
    }
    void reset() {
        WebRtc_MoveReadPtr(mRingBuffer, element_count);
    }

private:
    RingBuffer* mRingBuffer;
    int element_count;
};

#endif // BYTELOOPBUFFER_H
