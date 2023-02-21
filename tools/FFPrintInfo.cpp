#include "ffprintinfo.h"

static char* _av_ts2str(int64_t ts) {
    char buf[AV_TS_MAX_STRING_SIZE];
    return av_ts_make_string(buf, ts);
}
static char* _av_ts2timestr(int64_t ts, AVRational *tb) {
    char buf[AV_TS_MAX_STRING_SIZE];
    return av_ts_make_time_string(buf, ts, tb);
}

static char* _av_fourcc2str(uint32_t fourcc) {
    char buf[AV_FOURCC_MAX_STRING_SIZE];
    return av_fourcc_make_string(buf, fourcc);
}

static char* BytesToSize( double Bytes )
{
    float tb = 1099511627776;
    float gb = 1073741824;
    float mb = 1048576;
    float kb = 1024;

    char returnSize[256];

    if( Bytes >= tb )
        sprintf(returnSize, "%.2f TB", (float)Bytes/tb);
    else if( Bytes >= gb && Bytes < tb )
        sprintf(returnSize, "%.2f GB", (float)Bytes/gb);
    else if( Bytes >= mb && Bytes < gb )
        sprintf(returnSize, "%.2f MB", (float)Bytes/mb);
    else if( Bytes >= kb && Bytes < mb )
        sprintf(returnSize, "%.2f KB", (float)Bytes/kb);
    else if ( Bytes < kb)
        sprintf(returnSize, "%.2f Bytes", Bytes);
    else
        sprintf(returnSize, "%.2f Bytes", Bytes);

    static char ret[256];
    strcpy(ret, returnSize);
    return ret;
}

FFPrintInfo::FFPrintInfo(const char *tag)
{
    memcpy(m_tag, tag, strlen(tag));
}

void FFPrintInfo::printFormatContext(AVFormatContext* pfmtCxt, int is_output)
{
    printf("==============================av_dump_format==================================\n");

    for(int i=0; i<pfmtCxt->nb_streams; i++) {
        av_dump_format(pfmtCxt, i, 0, is_output);
    }
    printf("文件名 : %s \n",pfmtCxt->url);
    printf("输入格式 : %s \n全称 : %s \n",pfmtCxt->iformat->name,pfmtCxt->iformat->long_name);
    /*******************************输出相关media文件信息*******************************/
    printf("===========================================================================\n");

    float duration_ms;
    char duration_fmt[512];
    if (pfmtCxt->duration != AV_NOPTS_VALUE) {
        int64_t hours, mins, secs, us;
        int64_t duration = pfmtCxt->duration + (pfmtCxt->duration <= INT64_MAX - 5000 ? 5000 : 0);
        secs = duration / AV_TIME_BASE;
        us = duration % AV_TIME_BASE;
        mins = secs / 60;
        secs %= 60;
        hours = mins / 60;
        mins %= 60;
        //sprintf(duration_fmt, "duration: %02"PRId64":%02"PRId64":%02"PRId64".%02"PRId64"", hours, mins, secs,
        //        (100 * us) / AV_TIME_BASE);
        sprintf(duration_fmt, "duration: %02:%02:%02.%02", hours, mins, secs,
            (100 * us) / AV_TIME_BASE);
        duration_ms = (pfmtCxt->duration * 1.0 / AV_TIME_BASE) * 1000;
    } else {
        sprintf(duration_fmt, "N/A");
        duration_ms = 0;
    }
    printf("总时长 : %f ms,fmt:%s \n总比特率 : %f kbs\n",duration_ms,duration_fmt,pfmtCxt->bit_rate / 1000.0);//1000 bit/s = 1 kbit/s
    double fsize = pfmtCxt->duration!=AV_NOPTS_VALUE?(pfmtCxt->duration * 1.0 / AV_TIME_BASE * pfmtCxt->bit_rate / 8.0):0;
    printf("文件大小 : %s\n",BytesToSize(fsize));
#if 0
    int64_t tns, thh, tmm, tss;
    tns  = pfmtCxt->duration / 1000000;
    thh  = tns / 3600;
    tmm  = (tns % 3600) / 60;
    tss  = (tns % 60);

    printf("总时长 : %f ms,fmt:%02lld:%02lld:%02lld \n总比特率 : %f kbs\n",(pfmtCxt->duration * 1.0 / AV_TIME_BASE) * 1000,thh,tmm,tss,pfmtCxt->bit_rate / 1000.0);//1000 bit/s = 1 kbit/s
    double fsize = (pfmtCxt->duration * 1.0 / AV_TIME_BASE * pfmtCxt->bit_rate / 8.0);
    printf("文件大小 : %s\n",BytesToSize(fsize));
#endif
    printf("协议白名单 : %s \n协义黑名单 : %s\n",pfmtCxt->protocol_whitelist,pfmtCxt->protocol_blacklist);
    printf("数据包的最大数量 : %d\n",pfmtCxt->max_ts_probe);
    printf("最大缓冲时间 : %lld\n",pfmtCxt->max_interleave_delta);
    printf("缓冲帧的最大缓冲 : %u Bytes\n",pfmtCxt->max_picture_buffer);
    printf("metadata:\n");
    AVDictionary *metadata = pfmtCxt->metadata;
    if (metadata) {
        AVDictionaryEntry *entry = NULL;
        while ((entry = av_dict_get(metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            printf("\t%s : %s\n",entry->key,entry->value);
        }
    }

    for(int i=0; i<pfmtCxt->nb_streams; i++) {
        AVStream* stream = pfmtCxt->streams[i];
        printStream(stream);
    }


}

void FFPrintInfo::printStream(AVStream *stream)
{
    enum AVMediaType codec_type = stream->codecpar->codec_type;
    if(codec_type == AVMEDIA_TYPE_AUDIO) {
        printAudioStream(stream);
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
        printVideoStream(stream);
    } else {
        printf("Unrecognized stream #%d, type %d\n", stream->index, codec_type);
    }
}

void FFPrintInfo::printAudioStream(AVStream* audiostream)
{
    int audioStreamIdx = audiostream->index;
    printf("音频流信息(%s):\n",av_get_media_type_string(audiostream->codecpar->codec_type));
    printf("\tStream #%d\n",audioStreamIdx);
    printf("\t音频时长 : %f ms\n",audiostream->duration * av_q2d(audiostream->time_base) * 1000);
    const char *avcodocname = avcodec_get_name(audiostream->codecpar->codec_id);
    const char *profilestring = avcodec_profile_name(audiostream->codecpar->codec_id,audiostream->codecpar->profile);
    char * codec_fourcc = _av_fourcc2str(audiostream->codecpar->codec_tag);
    printf("\t编码格式 %s (%s,%s)\n",avcodocname,profilestring,codec_fourcc);
    printf("\t音频采样率 : %d Hz\n",audiostream->codecpar->sample_rate);
    printf("\t音频声道数 : %d \n",audiostream->codecpar->channels);
    printf("\t音频流比特率 : %f kbps\n",audiostream->codecpar->bit_rate / 1000.0);
    double s = audiostream->duration * av_q2d(audiostream->time_base);
    int64_t tbits = audiostream->codecpar->bit_rate * s;
    double stsize = tbits / 8;
    printf("\t音频流大小(Bytes) : %s\n",BytesToSize(stsize));
}

void FFPrintInfo::printVideoStream(AVStream* videostream)
{
    int videoStreamIdx = videostream->index;
    printf("视频流信息(%s):\n",av_get_media_type_string(videostream->codecpar->codec_type));
    printf("\tStream #%d\n",videoStreamIdx);
    printf("\t总帧数 : %lld\n",videostream->nb_frames);
    const char *avcodocname = avcodec_get_name(videostream->codecpar->codec_id);
    const char *profilestring = avcodec_profile_name(videostream->codecpar->codec_id,videostream->codecpar->profile);
    char * codec_fourcc = _av_fourcc2str(videostream->codecpar->codec_tag);
    printf("\t编码方式 : %s\n\tCodec Profile : %s\n\tCodec FourCC : %s\n",avcodocname,profilestring,codec_fourcc);
    ///如果是C++引用(AVPixelFormat)注意下强转类型
    const char *pix_fmt_name = videostream->codecpar->format == AV_PIX_FMT_NONE ?
                "none" : av_get_pix_fmt_name((AVPixelFormat)videostream->codecpar->format);

    printf("\t显示编码格式(color space) : %s \n",pix_fmt_name);
    printf("\t宽 : %d pixels,高 : %d pixels \n",videostream->codecpar->width,videostream->codecpar->height);
    AVRational display_aspect_ratio;
    av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
              videostream->codecpar->width  * (int64_t)videostream->sample_aspect_ratio.num,
              videostream->codecpar->height * (int64_t)videostream->sample_aspect_ratio.den,
              1024 * 1024);
    printf("\tsimple_aspect_ratio(SAR) : %d : %d\n\tdisplay_aspect_ratio(DAR) : %d : %d \n",videostream->sample_aspect_ratio.num,
           videostream->sample_aspect_ratio.den,display_aspect_ratio.num,display_aspect_ratio.den);
    printf("\t最低帧率 : %f fps\n\t平均帧率 : %f fps\n",av_q2d(videostream->r_frame_rate),av_q2d(videostream->avg_frame_rate));
    printf("\t每个像素点的比特数 : %d bits\n",videostream->codecpar->bits_per_raw_sample);
    printf("\t每个像素点编码比特数 : %d bits\n",videostream->codecpar->bits_per_coded_sample); //YUV三个分量每个分量是8,即24
    printf("\t视频流比特率 : %f kbps\n",videostream->codecpar->bit_rate / 1000.0);
    printf("\t基准时间 : %d / %d = %f \n",videostream->time_base.num,videostream->time_base.den,av_q2d(videostream->time_base));
    printf("\t视频流时长 : %f ms\n",videostream->duration * av_q2d(videostream->time_base) * 1000);
    printf("\t帧率(tbr) : %f\n",av_q2d(videostream->r_frame_rate));
    printf("\t文件层的时间精度(tbn) : %f\n",1/av_q2d(videostream->time_base));
    printf("\t视频层的时间精度(tbc) : %f\n",1/av_q2d(videostream->codec ->time_base));

    double s = videostream->duration * av_q2d(videostream->time_base);
    int64_t tbits = videostream->codecpar->bit_rate * s;
    double stsize = tbits / 8;
    printf("\t视频流大小(Bytes) : %s \n",BytesToSize(stsize));
    printf("\tmetadata:\n");

    AVDictionary *metadata = videostream->metadata;
    if (metadata) {
        AVDictionaryEntry *entry = NULL;
        while ((entry = av_dict_get(metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            printf("\t\t%s : %s\n",entry->key,entry->value);
        }
    }
}


void FFPrintInfo::log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    char ptsbuf[64], ptstimebuf[64], dtsbuf[64], dtstimebuf[64];
    char durationbuf[64], durationtimebuf[64];;
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    av_ts_make_string(ptsbuf, pkt->pts);
    av_ts_make_time_string(ptstimebuf, pkt->pts, time_base);
    av_ts_make_string(dtsbuf, pkt->dts);
    av_ts_make_time_string(dtstimebuf, pkt->dts, time_base);
    av_ts_make_string(durationbuf, pkt->duration);
    av_ts_make_time_string(durationtimebuf, pkt->duration, time_base);
    printf("%s: pts: %s pkt_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           m_tag, ptsbuf, ptstimebuf, dtsbuf, dtstimebuf, durationbuf, durationtimebuf, pkt->stream_index);
}
