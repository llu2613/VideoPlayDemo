#ifndef FFMPEGFILEENCODER_H
#define FFMPEGFILEENCODER_H

extern "C" {
#include "libavcodec/avcodec.h" //编解码
#include "libavformat/avformat.h" //封装格式
#include "libavutil/common.h" //其他工具类
#include "libavutil/imgutils.h" //图形缓存处理
//#include "libswscale/swscale.h"  //音频转换
#include "libswresample/swresample.h" //视频转换
#include "libavutil/error.h"
#include "libavutil/time.h"  //时间函数
#include "libavutil/opt.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/mathematics.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}


class FFmpegFileEncoder
{
	typedef struct FilteringContext {
		AVFilterContext *buffersink_ctx;
		AVFilterContext *buffersrc_ctx;
		AVFilterGraph *filter_graph;
	} FilteringContext;
	
	typedef struct StreamContext {
		AVCodecContext *dec_ctx;
		AVCodecContext *enc_ctx;
		AVAudioFifo *audio_fifo;
		int64_t audio_pts;
		int64_t video_pts;
        int64_t video_last_pts;
	} StreamContext;
	
public:
	FFmpegFileEncoder();
    virtual ~FFmpegFileEncoder();

    int test();
	int transcoding(const char *intput_file, const char *output_file);

private:
	AVFormatContext *ifmt_ctx = NULL;
	AVFormatContext *ofmt_ctx = NULL;
	FilteringContext *filter_ctx = NULL;
	StreamContext *stream_ctx = NULL;

	//重采样
	SwrContext* swr_ctx = NULL;
	//存放的数据
	uint8_t** src_data = NULL;
	uint8_t** dst_data = NULL;
	//存放的数据大小
	int src_linesize = 0;
	int dst_linesize = 0;

	int open_input_file(const char *filename);
	int open_output_file(const char *filename);
	
	int init_swr_context();
	int init_swr_ctx(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx);
	int swr_convert_packet(AVPacket* packet,
		AVCodecContext *dec_ctx, AVCodecContext *enc_ctx);
	void free_swr_ctx();

	int init_output_frame(AVFrame **frame,
		AVCodecContext *output_codec_context,
		int frame_size);
	int write_samples_to_fifo(AVAudioFifo *fifo,
		uint8_t **converted_input_samples,
		const int frame_size);
	int encode_write_frame_fifo(AVFrame *filt_frame, unsigned int stream_index);

	int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
		AVCodecContext *enc_ctx, const char *filter_spec);
	int init_filters(void);
	void print_error(const char *name, int err);
	void print_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt);
    int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index);
	int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index);
    int flush_audio_fifo(unsigned int stream_index);
	int flush_encoder(unsigned int stream_index);
    void av_log(void *avcl, int level, const char *fmt, ...);

	inline char* wrap_av_err2str(int errnum)
	{
		static char str[AV_ERROR_MAX_STRING_SIZE];
		memset(str, 0, sizeof(str));
		return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
	}
};

#endif // FFMPEGFILEENCODER_H
