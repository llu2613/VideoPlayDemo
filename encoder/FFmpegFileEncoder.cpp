#include "FFmpegFileEncoder.h"
#include<iostream>

/*
  Ffmpeg视频开发教程(一)——实现视频格式转换功能超详细版
  https://blog.csdn.net/zhangamxqun/article/details/80304494
  FFmpeg 示例转封装转码-transcoding
  https://www.jianshu.com/p/f04e0028dd14
  FFmpeg 音频重采样
  https://blog.csdn.net/qq_44623068/article/details/109702950
*/

//#pragma comment(lib,"../../3rdparts/ffmpeg/Win64/lib/avcodec.lib")
//#pragma comment(lib,"../../3rdparts/ffmpeg/Win64/lib/avformat.lib")
//#pragma comment(lib,"../../3rdparts/ffmpeg/Win64/lib/avfilter.lib")
//#pragma comment(lib,"../../3rdparts/ffmpeg/Win64/lib/avutil.lib")
//#pragma comment(lib,"../../3rdparts/ffmpeg/Win64/lib/swresample.lib")

using namespace std;

void ffmpegLogCallback(void *avcl, int level, const char *fmt,
	va_list vl) {

//    if (level >= av_log_get_level())
//        return;

	char logBuffer[2048];
	int cnt = vsnprintf(logBuffer, sizeof(logBuffer) / sizeof(char), fmt, vl);
	cout << logBuffer;
}

FFmpegFileEncoder::FFmpegFileEncoder()
{
	av_register_all();
	avfilter_register_all();
    av_log_set_level(AV_LOG_INFO);
	av_log_set_callback(ffmpegLogCallback);
}


FFmpegFileEncoder::~FFmpegFileEncoder()
{
}

int FFmpegFileEncoder::open_input_file(const char *filename)
{
	int ret;
	unsigned int i;

	//ifmt_ctx = NULL;
	if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
		print_error("OpenInput", ret);
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return ret;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return ret;
	}

	stream_ctx = (StreamContext *)av_mallocz_array(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
	if (!stream_ctx)
		return AVERROR(ENOMEM);

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *stream = ifmt_ctx->streams[i];
		AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
		AVCodecContext *codec_ctx;
		if (!dec) {
			av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
			return AVERROR_DECODER_NOT_FOUND;
		}
		codec_ctx = avcodec_alloc_context3(dec);
		if (!codec_ctx) {
			av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
			return AVERROR(ENOMEM);
		}
		ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
				"for stream #%u\n", i);
			return ret;
		}
		/* Reencode video & audio and remux subtitles etc. */
		if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
			|| codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
				codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
			/* Open decoder */
			ret = avcodec_open2(codec_ctx, dec, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
				return ret;
			}
		}
		stream_ctx[i].dec_ctx = codec_ctx;
	}

	av_dump_format(ifmt_ctx, 0, filename, 0);
	return 0;
}

int FFmpegFileEncoder::open_output_file(const char *filename)
{
	AVStream *out_stream;
	AVStream *in_stream;
	AVCodecContext *dec_ctx, *enc_ctx;
	AVAudioFifo *audio_fifo;
	AVCodec *encoder;
	int ret;
	unsigned int i;

	//ofmt_ctx = NULL;
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
	if (!ofmt_ctx) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		return AVERROR_UNKNOWN;
	}

	AVOutputFormat *fmt = av_guess_format(NULL, filename, NULL);

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		out_stream = avformat_new_stream(ofmt_ctx, NULL);
		if (!out_stream) {
			av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
			return AVERROR_UNKNOWN;
		}

		in_stream = ifmt_ctx->streams[i];
		dec_ctx = stream_ctx[i].dec_ctx;

		if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
			|| dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			/* in this example, we choose transcoding to same codec */
			//encoder = avcodec_find_encoder(dec_ctx->codec_id);
			if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
				encoder = avcodec_find_encoder(fmt->video_codec);
			} else {
				encoder = avcodec_find_encoder(fmt->audio_codec);
			}
			if (!encoder) {
				av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
				return AVERROR_INVALIDDATA;
			}
			enc_ctx = avcodec_alloc_context3(encoder);
			if (!enc_ctx) {
				av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
				return AVERROR(ENOMEM);
			}

			/* In this example, we transcode to same properties (picture size,
			* sample rate etc.). These properties can be changed for output
			* streams easily using filters */
			if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
				enc_ctx->height = dec_ctx->height;
				enc_ctx->width = dec_ctx->width;
				enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
				/* take first format from list of supported formats */
				if (encoder->pix_fmts)
					enc_ctx->pix_fmt = encoder->pix_fmts[0];
				else
					enc_ctx->pix_fmt = dec_ctx->pix_fmt;
				/* video time_base can be set to whatever is handy and supported by encoder */
				//enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
				enc_ctx->time_base.num = 1;
				enc_ctx->time_base.den = 25; //fix 25 fps
			}
			else {
				if (encoder->id == AV_CODEC_ID_OPUS) {
					enc_ctx->sample_rate = 48000;
				} else {
					enc_ctx->sample_rate = dec_ctx->sample_rate;
				}
				enc_ctx->channel_layout = dec_ctx->channel_layout;
				enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
				/* take first format from list of supported formats */
				enc_ctx->sample_fmt = encoder->sample_fmts[0];
				//enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
				enc_ctx->time_base.num = 1;
				enc_ctx->time_base.den = enc_ctx->sample_rate;
			}

			/* Third parameter can be used to pass settings to encoder */
			ret = avcodec_open2(enc_ctx, encoder, NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
				return ret;
			}
			ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", i);
				return ret;
			}
//			cout << "#" << i << " dec_ctx sp_rate " << dec_ctx->sample_rate << " f_size " << dec_ctx->frame_size << endl;
//			cout << "#" << i << " enc_ctx sp_rate " << enc_ctx->sample_rate << " f_size " << enc_ctx->frame_size << endl;
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			out_stream->time_base = enc_ctx->time_base;
			stream_ctx[i].enc_ctx = enc_ctx;

			if (enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
				/* Create the FIFO buffer based on the specified output sample format. */
				audio_fifo = av_audio_fifo_alloc(enc_ctx->sample_fmt, enc_ctx->channels, enc_ctx->frame_size);
				if (audio_fifo == NULL) {
					av_log(NULL, AV_LOG_ERROR, "Could not allocate FIFO\n");
					return AVERROR(ENOMEM);
				}
				stream_ctx[i].audio_fifo = audio_fifo;
			}
			stream_ctx[i].video_pts = 0;
			stream_ctx[i].audio_pts = 0;
		}
		else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
			av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
			return AVERROR_INVALIDDATA;
		}
		else {
			/* if this stream must be remuxed */
			ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
				return ret;
			}
			out_stream->time_base = in_stream->time_base;
		}

	}
	av_dump_format(ofmt_ctx, 0, filename, 1);

	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
			return ret;
		}
	}

	/* init muxer, write output file header */
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
		return ret;
	}

	return 0;
}

void FFmpegFileEncoder::print_error(const char *name, int err)
{
	char errbuf[128];
	const char *errbuf_ptr = errbuf;

	if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
		errbuf_ptr = strerror(AVUNERROR(err));
	av_log(NULL, AV_LOG_ERROR, "%s: %s\n", name, errbuf_ptr);
}

void FFmpegFileEncoder::print_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
/*	printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		pkt->stream_index);
		*/
}

int FFmpegFileEncoder::init_swr_context()
{
	unsigned int i;
	int ret;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVCodecContext *dec_ctx = stream_ctx[i].dec_ctx;
		AVCodecContext *enc_ctx = stream_ctx[i].enc_ctx;

		if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			ret = init_swr_ctx(dec_ctx, enc_ctx);
			if (ret < 0) {
				return ret;
			}
			break;
		}
	}
	std::cout << "can not find audio codec in stream_ctx.\n";
	return ret;
}

int FFmpegFileEncoder::init_swr_ctx(AVCodecContext *dec_ctx,
	AVCodecContext *enc_ctx)
{
	swr_ctx = swr_alloc_set_opts(NULL, //ctx
		enc_ctx->channel_layout,     //输出的channel 布局
		enc_ctx->sample_fmt,         //输出的采样格式
		enc_ctx->sample_rate,        //输出的采样率
		dec_ctx->channel_layout,     //输入的 channel 布局
		dec_ctx->sample_fmt,         //输入的采样格式
		dec_ctx->sample_rate,        //出入的采样率
		0, NULL
	);

	std::cout << "init_swr_ctx out ch" << enc_ctx->channel_layout << " fmt " << enc_ctx->sample_fmt << " sp " << enc_ctx->sample_rate << std::endl;
	std::cout << "init_swr_ctx in  ch" << dec_ctx->channel_layout << " fmt " << dec_ctx->sample_fmt << " sp " << dec_ctx->sample_rate << std::endl;

	if (swr_ctx == nullptr) {
		av_log(NULL, AV_LOG_ERROR, "init SwrContext failed\n");
		return -1;
	}

	int ret = swr_init(swr_ctx);

	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, wrap_av_err2str(ret));
		return ret;
	}
	
	//创建输入缓冲区
	av_samples_alloc_array_and_samples(
		&src_data,          //缓冲区的地址
		&src_linesize,		//缓冲区的大小
		av_get_channel_layout_nb_channels(dec_ctx->channel_layout),	//通道个数
		dec_ctx->sample_rate,   //单通道采样大小
		dec_ctx->sample_fmt,    //采样的格式
		0);
	//创建输出缓冲区
	av_samples_alloc_array_and_samples(
		&dst_data,          //缓冲区的地址
		&dst_linesize,		//缓冲区的大小
		av_get_channel_layout_nb_channels(enc_ctx->channel_layout),	//通道个数
		enc_ctx->sample_rate,   //单通道采样大小
		enc_ctx->sample_fmt,	//采样的格式
		0);

	std::cout << "输入的缓冲区大小为 : " << src_linesize  << "\n";
	std::cout << "输出的缓冲区大小为 : " << dst_linesize << "\n";

	return ret;
}

int FFmpegFileEncoder::swr_convert_packet(AVPacket* packet, 
	AVCodecContext *dec_ctx, AVCodecContext *enc_ctx)
{
	memcpy(src_data[0], packet->data, packet->size);

	int samples = swr_convert(swr_ctx, dst_data, enc_ctx->sample_rate, 
		(const uint8_t**)src_data, dec_ctx->sample_rate);

	//fwrite(dst_data[0], dst_linesize, 1, outAudioFile);
	return samples;
}

void FFmpegFileEncoder::free_swr_ctx()
{
	//释放资源
	if (src_data) {
		av_freep(&src_data[0]);
		av_freep(src_data);
	}

	if (dst_data) {
		av_freep(&dst_data[0]);
		av_freep(dst_data);
	}

	if(swr_ctx)
		swr_free(&swr_ctx);
}

/**
* Add converted input audio samples to the FIFO buffer for later processing.
* @param fifo                    Buffer to add the samples to
* @param converted_input_samples Samples to be added. The dimensions are channel
*                                (for multi-channel audio), sample.
* @param frame_size              Number of samples to be converted
* @return Error code (0 if successful)
*/
int FFmpegFileEncoder::write_samples_to_fifo(AVAudioFifo *fifo,
	uint8_t **converted_input_samples, 
	const int frame_size)
{
	int error;

	/* Make the FIFO as large as it needs to be to hold both,
	* the old and the new samples. */
	if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Could not reallocate FIFO\n");
		return error;
	}

	/* Store the new samples in the FIFO buffer. */
	if (av_audio_fifo_write(fifo, (void **)converted_input_samples,
		frame_size) < frame_size) {
		av_log(NULL, AV_LOG_ERROR, "Could not write data to FIFO\n");
		return AVERROR_EXIT;
	}
	return 0;
}

/**
* Initialize one input frame for writing to the output file.
* The frame will be exactly frame_size samples large.
* @param[out] frame                Frame to be initialized
* @param      output_codec_context Codec context of the output file
* @param      frame_size           Size of the frame
* @return Error code (0 if successful)
*/
int FFmpegFileEncoder::init_output_frame(AVFrame **frame,
	AVCodecContext *output_codec_context,
	int frame_size)
{
	int error;

	/* Create a new frame to store the audio samples. */
	if (!(*frame = av_frame_alloc())) {
		av_log(NULL, AV_LOG_ERROR, "Could not allocate output frame\n");
		return AVERROR_EXIT;
	}

	/* Set the frame's parameters, especially its size and format.
	* av_frame_get_buffer needs this to allocate memory for the
	* audio samples of the frame.
	* Default channel layouts based on the number of channels
	* are assumed for simplicity. */
	(*frame)->nb_samples = frame_size;
	(*frame)->channel_layout = output_codec_context->channel_layout;
	(*frame)->format = output_codec_context->sample_fmt;
	(*frame)->sample_rate = output_codec_context->sample_rate;

	/* Allocate the samples of the created frame. This call will make
	* sure that the audio frame can hold as many samples as specified. */
	if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Could not allocate output frame samples (error '%s')\n",
			wrap_av_err2str(error));
		av_frame_free(frame);
		return error;
	}

	return 0;
}

int FFmpegFileEncoder::init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
	AVCodecContext *enc_ctx, const char *filter_spec)
{
	char args[512];
	int ret = 0;
	const AVFilter *buffersrc = NULL;
	const AVFilter *buffersink = NULL;
	AVFilterContext *buffersrc_ctx = NULL;
	AVFilterContext *buffersink_ctx = NULL;
	AVFilterInOut *outputs = avfilter_inout_alloc();
	AVFilterInOut *inputs = avfilter_inout_alloc();
	AVFilterGraph *filter_graph = avfilter_graph_alloc();

	if (!outputs || !inputs || !filter_graph) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		buffersrc = avfilter_get_by_name("buffer");
		buffersink = avfilter_get_by_name("buffersink");
		if (!buffersrc || !buffersink) {
			av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		snprintf(args, sizeof(args),
			"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
			dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
			dec_ctx->time_base.num, dec_ctx->time_base.den,
			dec_ctx->sample_aspect_ratio.num,
			dec_ctx->sample_aspect_ratio.den);

		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
			args, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
			goto end;
		}

		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
			NULL, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
			(uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
			goto end;
		}
	}
	else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
		buffersrc = avfilter_get_by_name("abuffer");
		buffersink = avfilter_get_by_name("abuffersink");
		if (!buffersrc || !buffersink) {
			av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		if (!dec_ctx->channel_layout)
			dec_ctx->channel_layout =
			av_get_default_channel_layout(dec_ctx->channels);
		snprintf(args, sizeof(args),
			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
			dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
			av_get_sample_fmt_name(dec_ctx->sample_fmt),
			dec_ctx->channel_layout);
		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
			args, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
			goto end;
		}

		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
			NULL, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
			(uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
			(uint8_t*)&enc_ctx->channel_layout,
			sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
			(uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
			goto end;
		}
	}
	else {
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	/* Endpoints for the filter graph. */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if (!outputs->name || !inputs->name) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
		&inputs, &outputs, NULL)) < 0)
		goto end;

	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
		goto end;

	/* Fill FilteringContext */
	fctx->buffersrc_ctx = buffersrc_ctx;
	fctx->buffersink_ctx = buffersink_ctx;
	fctx->filter_graph = filter_graph;

end:
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);

	return ret;
}

int FFmpegFileEncoder::init_filters(void)
{
	const char *filter_spec;
	unsigned int i;
	int ret;
	filter_ctx = (FilteringContext *)av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
	if (!filter_ctx)
		return AVERROR(ENOMEM);

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		filter_ctx[i].buffersrc_ctx = NULL;
		filter_ctx[i].buffersink_ctx = NULL;
		filter_ctx[i].filter_graph = NULL;
		if (!(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
			|| ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
			continue;


		if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			filter_spec = "null"; /* passthrough (dummy) filter for video */
		else
			filter_spec = "anull"; /* passthrough (dummy) filter for audio */
		ret = init_filter(&filter_ctx[i], stream_ctx[i].dec_ctx,
			stream_ctx[i].enc_ctx, filter_spec);
		if (ret)
			return ret;
	}
	return 0;
}

int FFmpegFileEncoder::encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame) {
	int ret;
	int got_frame_local;
	AVPacket enc_pkt;

	if (!got_frame)
		got_frame = &got_frame_local;

	//av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
	/* encode filtered frame */
	enc_pkt.data = NULL;
	enc_pkt.size = 0;
	av_init_packet(&enc_pkt);

	if (stream_ctx[stream_index].enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
		if (filt_frame) {
			filt_frame->pts = stream_ctx[stream_index].audio_pts;
			stream_ctx[stream_index].audio_pts += filt_frame->nb_samples;
		}

		ret = avcodec_send_frame(stream_ctx[stream_index].enc_ctx, filt_frame);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error submitting the frame to the encoder, %s\n", wrap_av_err2str(ret));
			return ret;
		}

		while (1) {
			ret = avcodec_receive_packet(stream_ctx[stream_index].enc_ctx, &enc_pkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				//av_log(NULL, AV_LOG_INFO, "Error of EAGAIN or EOF\n");
				return 0;
			}
			else if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Error during encoding, %s\n", wrap_av_err2str(ret));
				return ret;
			}
			/* prepare packet for muxing */
			enc_pkt.stream_index = stream_index;
			av_packet_rescale_ts(&enc_pkt,
				stream_ctx[stream_index].enc_ctx->time_base,
				ofmt_ctx->streams[stream_index]->time_base);
			AVRational enc_timebase = stream_ctx[stream_index].enc_ctx->time_base;
			AVRational ofmt_timebase = ofmt_ctx->streams[stream_index]->time_base;
			av_log(NULL, AV_LOG_DEBUG, "Muxing frame #%d, enc_pkt->dts=%ld, enc_pkt->pts=%ld,"\
				" enc_ctx->time_base=%d/%d, ofmt_ctx->time_base=%d/%d\n", stream_index, enc_pkt.dts, enc_pkt.pts,
				enc_timebase.num, enc_timebase.den, ofmt_timebase.num, ofmt_timebase.den);
			/* mux encoded frame */
			ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
			av_packet_unref(&enc_pkt);
			if (ret < 0)
				return ret;
		}
	} else {
		ret = avcodec_encode_video2(stream_ctx[stream_index].enc_ctx, &enc_pkt,
			filt_frame, got_frame);
		if (ret < 0)
			return ret;
		if (!(*got_frame))
			return 0;

		{
			AVRational enc_timebase = stream_ctx[stream_index].enc_ctx->time_base;
			AVRational ofmt_timebase = ofmt_ctx->streams[stream_index]->time_base;
			av_log(NULL, AV_LOG_DEBUG, "Muxing before #%d, enc_pkt->dts=%ld, enc_pkt->pts=%ld, enc_pkt->duration=%ld,"\
				" enc_ctx->time_base=%d/%d, ofmt_ctx->time_base=%d/%d\n", stream_index, enc_pkt.dts, enc_pkt.pts, enc_pkt.duration,
				enc_timebase.num, enc_timebase.den, ofmt_timebase.num, ofmt_timebase.den);
		}

		/* prepare packet for muxing */
		enc_pkt.stream_index = stream_index;
		av_packet_rescale_ts(&enc_pkt,
			stream_ctx[stream_index].enc_ctx->time_base,
			ofmt_ctx->streams[stream_index]->time_base);

		{
			AVRational dec_timebase = stream_ctx[stream_index].dec_ctx->time_base;
			AVRational enc_timebase = stream_ctx[stream_index].enc_ctx->time_base;
			AVRational ifmt_timebase = ifmt_ctx->streams[stream_index]->time_base;
			AVRational ofmt_timebase = ofmt_ctx->streams[stream_index]->time_base;
			AVRational ifmtc_timebase = ifmt_ctx->streams[stream_index]->codec->time_base;
			AVRational ofmtc_timebase = ofmt_ctx->streams[stream_index]->codec->time_base;
			AVRational ifmt_avg_frame_rate = ifmt_ctx->streams[stream_index]->avg_frame_rate;
			AVRational ofmt_avg_frame_rate = ofmt_ctx->streams[stream_index]->avg_frame_rate;
			av_log(NULL, AV_LOG_DEBUG, "Rational  #%d, dec_ctx->time_base=%d/%d, ifmt->time_base=%d/%d, ifmtc->time_base=%d/%d,"\
				" enc_ctx->time_base=%d/%d, ofmt->time_base=%d/%d, ofmtc->time_base=%d/%d,"\
				" ifmt->frame_rate=%d/%d, ofmt->frame_rate=%d/%d\n", stream_index,
				dec_timebase.num, dec_timebase.den,
				ifmt_timebase.num, ifmt_timebase.den,
				ifmtc_timebase.num, ifmtc_timebase.den,
				enc_timebase.num, enc_timebase.den,
				ofmt_timebase.num, ofmt_timebase.den,
				ofmtc_timebase.num, ofmtc_timebase.den,
				ifmt_avg_frame_rate.num, ifmt_avg_frame_rate.den,
				ofmt_avg_frame_rate.num, ofmt_avg_frame_rate.den);
		}
		
		enc_pkt.pts = stream_ctx[stream_index].video_pts;
		enc_pkt.dts = enc_pkt.pts;
		enc_pkt.duration = av_rescale_q(1,
			stream_ctx[stream_index].enc_ctx->time_base,
			ofmt_ctx->streams[stream_index]->time_base);
		stream_ctx[stream_index].video_pts += enc_pkt.duration;
		
		//av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
		AVRational enc_timebase = stream_ctx[stream_index].enc_ctx->time_base;
		AVRational ofmt_timebase = ofmt_ctx->streams[stream_index]->time_base;
		av_log(NULL, AV_LOG_DEBUG, "Muxing frame #%d, enc_pkt->dts=%ld, enc_pkt->pts=%ld, enc_pkt->duration=%ld,"\
			" enc_ctx->time_base=%d/%d, ofmt_ctx->time_base=%d/%d\n", stream_index, enc_pkt.dts, enc_pkt.pts, enc_pkt.duration,
			enc_timebase.num, enc_timebase.den, ofmt_timebase.num, ofmt_timebase.den);
		/* mux encoded frame */
		ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
		av_packet_unref(&enc_pkt);
		return ret;
	}
}

int FFmpegFileEncoder::encode_write_frame_fifo(AVFrame *filt_frame, unsigned int stream_index) {
	int ret = 0;
	if (stream_ctx[stream_index].enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
		const int output_frame_size = stream_ctx[stream_index].enc_ctx->frame_size;

		/* Make sure that there is one frame worth of samples in the FIFO
		* buffer so that the encoder can do its work.
		* Since the decoder's and the encoder's frame size may differ, we
		* need to FIFO buffer to store as many frames worth of input samples
		* that they make up at least one frame worth of output samples. */
		write_samples_to_fifo(stream_ctx[stream_index].audio_fifo, filt_frame->data, filt_frame->nb_samples);
		int audio_fifo_size = av_audio_fifo_size(stream_ctx[stream_index].audio_fifo);
		if (audio_fifo_size < stream_ctx[stream_index].enc_ctx->frame_size) {
			/* Decode one frame worth of audio samples, convert it to the
			* output sample format and put it into the FIFO buffer. */
			return 0;
		}

		/* If we have enough samples for the encoder, we encode them.
		* At the end of the file, we pass the remaining samples to
		* the encoder. */
		while (av_audio_fifo_size(stream_ctx[stream_index].audio_fifo) >= stream_ctx[stream_index].enc_ctx->frame_size) {
			/* Take one frame worth of audio samples from the FIFO buffer,
			* encode it and write it to the output file. */

			/* Use the maximum number of possible samples per frame.
			* If there is less than the maximum possible frame size in the FIFO
			* buffer use this number. Otherwise, use the maximum possible frame size. */
			const int frame_size = FFMIN(av_audio_fifo_size(stream_ctx[stream_index].audio_fifo), stream_ctx[stream_index].enc_ctx->frame_size);

			AVFrame *output_frame;
			/* Initialize temporary storage for one output frame. */
			if (init_output_frame(&output_frame, stream_ctx[stream_index].enc_ctx, frame_size) < 0) {
				av_log(NULL, AV_LOG_ERROR, "init_output_frame failed\n");
				return AVERROR_EXIT;
			}

			/* Read as many samples from the FIFO buffer as required to fill the frame.
			* The samples are stored in the frame temporarily. */
			if (av_audio_fifo_read(stream_ctx[stream_index].audio_fifo, (void **)output_frame->data, frame_size) < frame_size) {
				av_log(NULL, AV_LOG_ERROR, "Could not read data from FIFO\n");
				av_frame_free(&output_frame);
				return AVERROR_EXIT;
			}
			
			//cout << "audo output_frame pts " << output_frame->pts << " dts " << output_frame->pkt_dts << " be " << output_frame->best_effort_timestamp << endl;
			ret = encode_write_frame(output_frame, stream_index);
			av_frame_free(&output_frame);
		}
	}
	else {
		ret = encode_write_frame(filt_frame, stream_index);
		av_frame_free(&filt_frame);
	}
	return ret;
}

int FFmpegFileEncoder::filter_encode_write_frame(AVFrame *frame, unsigned int stream_index)
{
	int ret;
	AVFrame *filt_frame;

	//av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
	/* push the decoded frame into the filtergraph */
	ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx,
		frame, 0);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
		return ret;
	}

	/* pull filtered frames from the filtergraph */
	while (1) {
		filt_frame = av_frame_alloc();
		if (!filt_frame) {
			return AVERROR(ENOMEM);
		}
		//av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
		ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx,
			filt_frame);
		if (ret < 0) {
			/* if no more frames for output - returns AVERROR(EAGAIN)
			* if flushed and no more frames for output - returns AVERROR_EOF
			* rewrite retcode to 0 to show it as normal procedure completion
			*/
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				ret = 0;
			goto cleanup;
		}
		filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
		ret = encode_write_frame_fifo(filt_frame, stream_index);
		if (ret < 0)
			goto cleanup;
	}
cleanup:
	av_frame_free(&filt_frame);
	return ret;
}

int FFmpegFileEncoder::flush_encoder(unsigned int stream_index)
{
	int ret;
	int got_frame;

	if (!(stream_ctx[stream_index].enc_ctx->codec->capabilities &
		AV_CODEC_CAP_DELAY))
		return 0;

	while (1) {
		//av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
		ret = encode_write_frame(NULL, stream_index, &got_frame);
		if (ret < 0)
			break;
		if (!got_frame)
			return 0;
	}
	return ret;
}

//goto语句仅仅限于退出时使用

int FFmpegFileEncoder::test()
{
#if 0
	//输入要进行格式转换的文件
    char intput_file[] = "D:\\test\\86E13DDC-7CFA-4B9C-A875-476257CD6A13_1625045310.flv";
	//输出转换后的文件
    char output_file[] = "D:\\test\\qinghuaci_out0.webm";
#else //网络文件
	//输入要进行格式转换的文件
    char intput_file[] = "D:\\test\\qinghuaci.mp4";
	//输出转换后的文件
    char output_file[] = "D:\\test\\qinghuaci_out2.webm";
#endif

	cout << "transcoding, waiting..."<<endl;

	int ret = transcoding(intput_file, output_file);

	bool success = (ret >= 0);

	cout << "transcoding " << (success ? "SUCCESS.": "FAILURE!")<<endl;

	return ret;
}

int FFmpegFileEncoder::transcoding(const char *intput_file, const char *output_file)
{
	int ret;
	AVPacket *packet = NULL;
	AVFrame *frame = NULL;
	enum AVMediaType type;
	unsigned int stream_index;
	unsigned int i;
	int got_frame;
	int(*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);

	//av_register_all();
	//avfilter_register_all();
	//av_log_set_level(AV_LOG_DEBUG);

	if ((ret = open_input_file(intput_file)) < 0)
		goto end;
	if ((ret = open_output_file(output_file)) < 0)
		goto end;
	if ((ret = init_filters()) < 0)
		goto end;
	packet = av_packet_alloc();
	if (!packet) {
		av_log(NULL, AV_LOG_ERROR, "No memory for av_packet_alloc\n");
		goto end;
	}

	/* read all packets */
	while (1) {
		if ((ret = av_read_frame(ifmt_ctx, packet)) < 0)
			break;
		stream_index = packet->stream_index;
		type = ifmt_ctx->streams[packet->stream_index]->codecpar->codec_type;
		av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
			stream_index);

		if (filter_ctx[stream_index].filter_graph) {
			av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");
			frame = av_frame_alloc();
			if (!frame) {
				ret = AVERROR(ENOMEM);
				break;
			}
			av_packet_rescale_ts(packet,
				ifmt_ctx->streams[stream_index]->time_base,
				stream_ctx[stream_index].dec_ctx->time_base);
			dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 :
				avcodec_decode_audio4;
			ret = dec_func(stream_ctx[stream_index].dec_ctx, frame,
				&got_frame, packet);
			if (ret < 0) {
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}

			if (got_frame) {
				/*cout <<"#"<< stream_index << " got_frame pts " << frame->pts <<" pkt_pts "<< frame->pkt_pts 
					<< " pkt_dts " << frame->pkt_dts << " pkt_dur " << frame->pkt_duration <<" be "<< frame->best_effort_timestamp << endl;*/
				frame->pts = frame->best_effort_timestamp;
				ret = filter_encode_write_frame(frame, stream_index);
				av_frame_free(&frame);
				if (ret < 0)
					goto end;
			}
			else {
				av_frame_free(&frame);
			}
		}
		else {
			/* remux this frame without reencoding */
			av_packet_rescale_ts(packet,
				ifmt_ctx->streams[stream_index]->time_base,
				ofmt_ctx->streams[stream_index]->time_base);

			ret = av_interleaved_write_frame(ofmt_ctx, packet);
			if (ret < 0)
				goto end;
		}
		av_packet_unref(packet);
	}

	/* flush filters and encoders */
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		/* flush filter */
		if (!filter_ctx[i].filter_graph)
			continue;
		ret = filter_encode_write_frame(NULL, i);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
			goto end;
		}

		/* flush encoder */
		ret = flush_encoder(i);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
			goto end;
		}
	}

	av_write_trailer(ofmt_ctx);
end:
	av_packet_free(&packet);
	av_frame_free(&frame);
	for (i = 0; ifmt_ctx && i < ifmt_ctx->nb_streams; i++) {
		if(stream_ctx && stream_ctx[i].dec_ctx)
			avcodec_free_context(&stream_ctx[i].dec_ctx);
		if(stream_ctx && stream_ctx[i].audio_fifo)
			av_audio_fifo_free(stream_ctx[i].audio_fifo);
		if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && stream_ctx[i].enc_ctx)
			avcodec_free_context(&stream_ctx[i].enc_ctx);
		if (filter_ctx && filter_ctx[i].filter_graph)
			avfilter_graph_free(&filter_ctx[i].filter_graph);
	}
	av_free(filter_ctx);
	av_free(stream_ctx);
	avformat_close_input(&ifmt_ctx);
	if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	char errorstr[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	if (ret < 0)
		av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_make_error_string(errorstr, AV_ERROR_MAX_STRING_SIZE, ret));

	filter_ctx = NULL;
	stream_ctx = NULL;
	ifmt_ctx = NULL;
	ofmt_ctx = NULL;
	return ret;
}
