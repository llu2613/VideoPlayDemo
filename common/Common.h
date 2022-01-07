#ifndef COMMON_H
#define COMMON_H

#ifdef _WIN32
#ifdef FFMEDIACODEC_EXPORTS
#define FFMEDIACODEC_API __declspec(dllexport)
#else
#define FFMEDIACODEC_API __declspec(dllimport)
#endif
#else
#define FFMEDIACODEC_API
#endif

extern "C" {
#include "libavcodec/avcodec.h" //�����
#include "libavformat/avformat.h" //��װ��ʽ
#include "libavutil/common.h" //����������
#include "libavutil/imgutils.h" //ͼ�λ��洦��
#include "libswscale/swscale.h"  //��Ƶת��
#include "libswresample/swresample.h" //��Ƶת��
#include "libavutil/error.h"
#include "libavutil/time.h"  //ʱ�亯��
#include "libavutil/opt.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/mathematics.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

#endif //COMMON_H