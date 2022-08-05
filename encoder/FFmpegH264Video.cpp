#include "FFmpegH264Video.h"
#include <QDebug>

/*
 * ffmpeg解析MP4封装的avc1编码问题
 * https://blog.csdn.net/FPGATOM/article/details/86694681
ffmpeg -framerate 24 -i 1112223334445556667778881_1656465104.mp4 -c copy output.mp4
ffmpeg -i 1112223334445556667778881_1656465104.mp4 -c copy output.mp4

FFmpeg DTS、PTS和时间戳TIME_BASE详解
https://blog.csdn.net/aiynmimi/article/details/121231246
*/

FFmpegH264Video::FFmpegH264Video()
{

}

FFmpegH264Video::~FFmpegH264Video()
{

}

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
    int ret, i;
    const int MAX_STREAM = 32;
    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVBSFContext *bsf_ctx_ls[MAX_STREAM];
    memset(bsf_ctx_ls, 0, sizeof(bsf_ctx_ls));
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    int istream_index = 0;
    int ostream_index = 0;
    enum AVMediaType media_type;

    int frame_cnt[MAX_STREAM];
    int64_t packet_last_dts[MAX_STREAM];
    int64_t packet_curr_dts[MAX_STREAM];
    memset(frame_cnt, 0, sizeof(frame_cnt));
    memset(packet_last_dts, 0, sizeof(packet_last_dts));
    memset(packet_curr_dts, 0, sizeof(packet_curr_dts));
    int in_to_out_index[MAX_STREAM];
    for(i=0; i<MAX_STREAM;i++)
        in_to_out_index[i] = -1;

    ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL);
    if (ret < 0) {
        _print_averror("avformat_open_input", ret);
        goto end;
    }

    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if (ret < 0) {
        _print_averror("avformat_find_stream_info", ret);
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);

    ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_file);
    if (ret<0) {
        _print_averror("avformat_alloc_output_context2", ret);
        goto end;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVCodecID in_codec_id = in_stream->codecpar->codec_id;

        if(av_codec_get_tag(ofmt_ctx->oformat->codec_tag, in_codec_id)==0) {
            av_log(NULL, AV_LOG_WARNING, "ignore stream %d, codec_tag is invalid\n", i);
            continue;
        }

        AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            goto end;
        }
        in_to_out_index[i] = out_stream->index;

        /* if this stream must be remuxed */
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
            goto end;
        }

        // fix time base
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {
            AVRational time_base = {1, in_stream->codecpar->sample_rate};
            out_stream->time_base = time_base;
        } else {
            out_stream->time_base = in_stream->time_base;
        }

        //qDebug() <<"#"<<i<<in_stream->time_base.num<<"/"<<in_stream->time_base.den;

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
		_print_averror("avformat_write_header", ret);
        goto end;
    }

    packet = av_packet_alloc();
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, packet)) < 0)
            break;

        istream_index = packet->stream_index;
        ostream_index = in_to_out_index[istream_index];

        if(ostream_index==-1) {
            av_packet_unref(packet);
            continue;
        }

        media_type = ifmt_ctx->streams[istream_index]->codecpar->codec_type;

        //qDebug()<<"#"<<stream_index<<" dts "<<packet->dts<<" pts "<<packet->pts;

        if (istream_index >= 0 && istream_index < MAX_STREAM) {
            frame_cnt[istream_index]++;
            packet_curr_dts[istream_index] += packet_last_dts[istream_index]
                    ? (packet->dts)-packet_last_dts[istream_index] : 0;
            packet_last_dts[istream_index] = packet->dts;

            double s = packet_curr_dts[istream_index] * av_q2d(ifmt_ctx->streams[istream_index]->time_base);
            double s2 = packet_curr_dts[istream_index] * av_q2d(ofmt_ctx->streams[istream_index]->time_base);
            //qDebug()<<"#"<<stream_index<<" dts "<<packet_curr_dts[stream_index]
            //          <<" frames "<<frame_cnt[stream_index]<<" dur "<<s<<s2;
        }

        if(bsf_ctx_ls[istream_index]) {
            //AVPacket处理
            ret = av_bsf_send_packet(bsf_ctx_ls[istream_index], packet);
            if (ret < 0) {
                _print_averror("av_bsf_send_packet", ret);
            }
            while(av_bsf_receive_packet(bsf_ctx_ls[istream_index], packet) == 0) {
                if(reset_dts) {
                    packet->dts = packet_curr_dts[istream_index];
                    packet->pts = packet_curr_dts[istream_index];
                }
                packet->stream_index = ostream_index;
                av_packet_rescale_ts(packet, ifmt_ctx->streams[istream_index]->time_base,
                                     ofmt_ctx->streams[ostream_index]->time_base);
                ret = av_interleaved_write_frame(ofmt_ctx, packet);
                if (ret < 0) {
                    _print_averror("av_interleaved_write_frame", ret);
                    break;
                }
            }
        } else {
            if(reset_dts) {
                packet->dts = packet_curr_dts[istream_index];
                packet->pts = packet_curr_dts[istream_index];
            }
            packet->stream_index = ostream_index;
            av_packet_rescale_ts(packet, ifmt_ctx->streams[istream_index]->time_base,
                                 ofmt_ctx->streams[ostream_index]->time_base);
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
    for(i=0; i<MAX_STREAM; i++) {
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
