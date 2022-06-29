#include "FFmpegH264Video.h"

/*
 * ffmpeg解析MP4封装的avc1编码问题
 * https://blog.csdn.net/FPGATOM/article/details/86694681
ffmpeg -framerate 24 -i 1112223334445556667778881_1656465104.mp4 -c copy output.mp4
ffmpeg -i 1112223334445556667778881_1656465104.mp4 -c copy output.mp4
*/

FFmpegH264Video::FFmpegH264Video()
{

}

FFmpegH264Video::~FFmpegH264Video()
{

}
#include <QDebug>
static void _print_averror(const char *name, int err)
{
    char errbuf[200], message[256];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", name, errbuf_ptr);

    sprintf(message, "(%s)%s", name, errbuf_ptr);
    qDebug()<<message;
}

int FFmpegH264Video::h264_avc1(const char *filename, const char *out_file,
                               bool reset_dts)
{
    int ret;
    const int MAX_STREAM = 32;
    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVBSFContext *bsf_ctx_ls[MAX_STREAM];
    memset(bsf_ctx_ls, 0, sizeof(bsf_ctx_ls));
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    enum AVMediaType media_type;
    unsigned int stream_index;
    int frame_cnt[MAX_STREAM];
    int64_t packet_last_dts[MAX_STREAM];
    int64_t packet_curr_dts[MAX_STREAM];
    memset(frame_cnt, 0, sizeof(frame_cnt));
    memset(packet_last_dts, 0, sizeof(packet_last_dts));
    memset(packet_curr_dts, 0, sizeof(packet_curr_dts));

    ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL);
    if (ret < 0) {
        _print_averror("avformat_open_input", ret);
        goto end;
    }

    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);

    ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_file);
    if (ret<0) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        goto end;
    }

    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            goto end;
        }

        /* if this stream must be remuxed */
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
            goto end;
        }
        out_stream->time_base = in_stream->time_base;

        // stream filter
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO
                && in_stream->codecpar->codec_id==AV_CODEC_ID_H264) {
            const AVBitStreamFilter *pfilter = av_bsf_get_by_name("h264_mp4toannexb");
            av_bsf_alloc(pfilter, &(bsf_ctx_ls[i]));
            if(bsf_ctx_ls[i]) {
                avcodec_parameters_copy(bsf_ctx_ls[i]->par_in, in_stream->codecpar);
                bsf_ctx_ls[i]->time_base_in = in_stream->time_base;
                av_bsf_init(bsf_ctx_ls[i]);
            }
        }
    }

    av_dump_format(ofmt_ctx, 0, out_file, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, ofmt_ctx->url, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", out_file);
            goto end;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        goto end;
    }

    packet = av_packet_alloc();
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, packet)) < 0)
            break;

        stream_index = packet->stream_index;
        media_type = ifmt_ctx->streams[stream_index]->codecpar->codec_type;

        if (stream_index >= 0 && stream_index < MAX_STREAM) {
            frame_cnt[stream_index]++;
            packet_curr_dts[stream_index] += packet_last_dts[stream_index]
                    ? (packet->dts)-packet_last_dts[stream_index] : 0;
            packet_last_dts[stream_index] = packet->dts;
        }

        if(bsf_ctx_ls[stream_index]) {
            //AVPacket处理
            ret = av_bsf_send_packet(bsf_ctx_ls[stream_index], packet);
            if (ret < 0) {
                _print_averror("av_bsf_send_packet", ret);
            }
            while(av_bsf_receive_packet(bsf_ctx_ls[stream_index], packet) == 0) {
                if(reset_dts) {
                    packet->dts = packet_curr_dts[stream_index];
                    packet->pts = packet_curr_dts[stream_index];
                }
                ret = av_interleaved_write_frame(ofmt_ctx, packet);
                if (ret < 0) {
                    _print_averror("av_interleaved_write_frame", ret);
                    break;
                }
            }
        } else {
            packet->dts = packet_curr_dts[stream_index];
            packet->pts = packet_curr_dts[stream_index];
            ret = av_interleaved_write_frame(ofmt_ctx, packet);
            if (ret < 0) {
                _print_averror("av_interleaved_write_frame", ret);
                break;
            }
        }
        av_packet_unref(packet);
    }

    av_write_trailer(ofmt_ctx);
end:
    //释放：
    for(int i=0; i<MAX_STREAM; i++) {
        av_bsf_free(&(bsf_ctx_ls[i]));
    }

    av_packet_free(&packet);
    av_frame_free(&frame);

	if(ofmt_ctx)
		avio_closep(&ofmt_ctx->pb);

    avformat_close_input(&ifmt_ctx);
    avformat_free_context(ofmt_ctx);

    return ret;
}

int FFmpegH264Video::h265_avc1(const char *filename, const char *out_file,
                               bool reset_dts)
{
//const AVBitStreamFilter *pfilter = av_bsf_get_by_name("hevc_mp4toannexb");
    return -1;
}

int FFmpegH264Video::h264_ES()
{
    return -1;
}
