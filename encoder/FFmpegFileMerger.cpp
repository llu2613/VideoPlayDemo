#include "FFmpegFileMerger.h"
#include <QDir>
#include <QFile>
#include <QDebug>

FFmpegFileMerger::FFmpegFileMerger()
{
    has_opened = false;
    extra_audio_data = true;
    extra_video_data = true;
    output_format_ctx = nullptr;
    out_audio_stream = nullptr;
    out_video_stream = nullptr;
    out_audio_codec_ctx = nullptr;
    out_video_codec_ctx = nullptr;
    audio_bsf_ctx = nullptr;
    video_bsf_ctx = nullptr;
}

FFmpegFileMerger::~FFmpegFileMerger()
{

}


void FFmpegFileMerger::start(std::string out_file)
{
    this->out_file = out_file;
}

int FFmpegFileMerger::merge(std::string in_file)
{
    if(open(in_file.c_str(), NULL, false)>=0) {
        if(!has_opened) {
            openOutput();
        }
        while(decoding()>=0);
        close();
    }
    return 0;
}

void FFmpegFileMerger::finish()
{
    if(has_opened) {
        closeOutput();
    }
}

int FFmpegFileMerger::audioRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int error = -1;
//    if(extra_audio_data == false)
//    {
//        av_bitstream_filter_filter(out_audio_bsf, out_audio_stream->codec, NULL,
//                                   &(packet->data), &(packet->size), packet->data, packet->size,
//                                   packet->flags & AV_PKT_FLAG_KEY);
//        extra_audio_data = true;
//    }

    //packet.pts = av_rescale_q_rnd(packet.pts,merge_ctx->in_stream->time_base,new_timebase, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    //packet.dts = av_rescale_q_rnd(packet.dts,merge_ctx->in_stream->time_base,new_timebase, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    //packet.duration = av_rescale_q(packet.duration, merge_ctx->in_stream->time_base, new_timebase);

    av_packet_rescale_ts(packet, audioStream()->time_base, out_audio_stream->time_base);
    packet->pts = audio_total_pts;
    packet->dts = audio_total_pts;
    audio_total_pts += packet->duration;
    samples_count += frame->nb_samples;

    //error = av_write_frame(output_format_ctx, packet);

    if (av_bsf_send_packet(audio_bsf_ctx, packet) != 0) {
        return 0;
    }
    AVPacket *pkt = av_packet_alloc();;
    av_init_packet(pkt);
    while(av_bsf_receive_packet(audio_bsf_ctx, pkt) == 0) {
        av_write_frame(output_format_ctx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    return 0;
}

int FFmpegFileMerger::videoRawFrame(AVCodecContext *pCodecCtx, AVFrame *frame, AVPacket *packet)
{
    int error = -1;
//    if(extra_video_data == false)
//    {
//        av_bitstream_filter_filter(out_video_bsf, out_video_stream->codec, NULL,
//                                   &(packet->data), &(packet->size), packet->data, packet->size,
//                                   packet->flags & AV_PKT_FLAG_KEY);
//        extra_video_data = true;
//    }

    //packet.pts = av_rescale_q_rnd(packet.pts,merge_ctx->in_stream->time_base,new_timebase, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    //packet.dts = av_rescale_q_rnd(packet.dts,merge_ctx->in_stream->time_base,new_timebase, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    //packet.duration = av_rescale_q(packet.duration, merge_ctx->in_stream->time_base, new_timebase);

//    error = av_interleaved_write_frame(output_format_ctx, packet);
//    error = av_write_frame(output_format_ctx, packet);

    if (av_bsf_send_packet(video_bsf_ctx, packet) != 0) {
        return 0;
    }
    AVPacket *pkt = av_packet_alloc();;
    av_init_packet(pkt);
    while(av_bsf_receive_packet(video_bsf_ctx, pkt) == 0) {
        av_write_frame(output_format_ctx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    return 0;
}

int FFmpegFileMerger::openOutput()
{
    frame_count = 0;
    samples_count = 0;
    audio_total_pts = 0;
    video_total_pts = 0;
    has_opened = false;
    extra_audio_data = true;
    extra_video_data = true;

    if(!out_file.size()) {
        const char* in_file_c = inputfile();
        char fi[256], fi2[256];
        if(strlen(in_file_c)) {
            memccpy(fi, in_file_c, '.', strlen(in_file_c)+1);
            sprintf(fi2, "%s%s", fi, "mp4");
            out_file = fi2;
        }
    }

    const char *output_filename = out_file.c_str();
    int error = avformat_alloc_output_context2(&output_format_ctx, NULL, "mp4",output_filename);
    if(error != 0)
    {
        printf("avformat_open_input file error\n");
        return -1;
    }

    const AVFormatContext* input_format_ctx = formatContext();
    av_dump_format(const_cast<AVFormatContext*>(input_format_ctx),0,inputfile(),0);
    if(audioStream()) {
        const AVStream *in_stream = audioStream();
        out_audio_stream = avformat_new_stream(output_format_ctx, NULL);
        if (!out_audio_stream) {
            printf("avformat_new_stream out stream failed ##\n");
            return -1;
        }
        avcodec_parameters_copy(out_audio_stream->codecpar, in_stream->codecpar);

        int ret = 0;
        out_audio_codec_ctx = avcodec_alloc_context3(NULL);
        ret = avcodec_parameters_to_context(out_audio_codec_ctx, in_stream->codecpar);
        if (ret < 0) {
            printf("failed to copy context from input to output stream codec context\n");
            return -1;
        }
        out_audio_codec_ctx->codec_tag = 0;
        if (output_format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_audio_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        ret = avcodec_parameters_from_context(out_audio_stream->codecpar, out_audio_codec_ctx);
        if (ret < 0) {
            printf("failed to avcodec_parameters_from_context\n");
            return -1;
        }

        //Use av_bsf_get_by_name(), av_bsf_alloc(), and av_bsf_init()
        const AVBitStreamFilter *absfilter = av_bsf_get_by_name("aac_adtstoasc");
        av_bsf_alloc(absfilter, &audio_bsf_ctx);
        if(audio_bsf_ctx == NULL) {
            printf("av_bitstream_filter_init failed ##\n");
            return -1;
        }
        avcodec_parameters_copy(audio_bsf_ctx->par_in, in_stream->codecpar);
        av_bsf_init(audio_bsf_ctx);
    }
    if(videoStream()) {
        const AVStream *in_stream = videoStream();
        out_video_stream = avformat_new_stream(output_format_ctx, NULL);
        if (!out_video_stream) {
            printf("avformat_new_stream out stream failed ##\n");
            return -1;
        }
        avcodec_parameters_copy(out_video_stream->codecpar, in_stream->codecpar);

        int ret = 0;
        out_video_codec_ctx = avcodec_alloc_context3(NULL);
        ret = avcodec_parameters_to_context(out_video_codec_ctx, in_stream->codecpar);
        if (ret < 0) {
            printf("failed to copy context from input to output stream codec context\n");
            return -1;
        }
        out_video_codec_ctx->codec_tag = 0;
        if (output_format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        ret = avcodec_parameters_from_context(out_video_stream->codecpar, out_video_codec_ctx);
        if (ret < 0) {
            printf("failed to avcodec_parameters_from_context\n");
            return -1;
        }

        //Use av_bsf_get_by_name(), av_bsf_alloc(), and av_bsf_init()
        const AVBitStreamFilter *vbsfilter = av_bsf_get_by_name("h264_mp4toannexb");
        av_bsf_alloc(vbsfilter, &video_bsf_ctx);
        if(video_bsf_ctx == NULL) {
            printf("av_bitstream_filter_init failed ##\n");
            return -1;
        }
        avcodec_parameters_copy(video_bsf_ctx->par_in, in_stream->codecpar);
        av_bsf_init(video_bsf_ctx);
    }
    av_dump_format(output_format_ctx, 0, output_filename, 1);

    error = avio_open(&(output_format_ctx->pb), output_filename, AVIO_FLAG_WRITE);
    if(error != 0)
    {
        printf("avformat_open_input file error\n");
        return -1;
    }

    has_opened = true;

    if(has_opened) {
        extra_audio_data = false;
        extra_video_data = false;
    }

    error = avformat_write_header(output_format_ctx, NULL);

    return error;
}

void FFmpegFileMerger::closeOutput()
{
    if(output_format_ctx) {
        av_write_trailer(output_format_ctx);
        avio_close(output_format_ctx->pb);
        avformat_free_context(output_format_ctx);
        output_format_ctx = nullptr;
    }
    if(out_audio_codec_ctx) {
        avcodec_free_context(&out_audio_codec_ctx);
        out_audio_codec_ctx = nullptr;
    }
    if(out_video_codec_ctx) {
        avcodec_free_context(&out_video_codec_ctx);
        out_video_codec_ctx = nullptr;
    }
    if(audio_bsf_ctx) {
        av_bsf_free(&audio_bsf_ctx);
        audio_bsf_ctx = nullptr;
    }
    if(video_bsf_ctx) {
        av_bsf_free(&video_bsf_ctx);
        video_bsf_ctx = nullptr;
    }
}

void FFmpegFileMerger::run(std::list<std::string> in_files, std::string out_file)
{
    {
        printf("InputList:\n");
        std::list<std::string>::iterator it;
        for(it=in_files.begin();it!=in_files.end();it++) {
            std::string file = *it;
            printf("  %s\n", file.c_str());
        }
        printf("Output:%s\n", out_file);
    }

    start(out_file);
    std::list<std::string>::iterator it;
    for(it=in_files.begin();it!=in_files.end();it++) {
        std::string in_file = *it;
        merge(in_file);
    }
    finish();
}

void FFmpegFileMerger::test()
{
#if 1
    QDir cfgPathDir = QDir("D:\\test\\devrecorda02c237ee64441e5b90f5e6c7ab616cf2");
    if(!cfgPathDir.exists()){
        return;
    }
    QStringList filters;
    filters << QString("*.ts");
    cfgPathDir.setFilter(QDir::Files | QDir::NoSymLinks); //设置类型过滤器，只为文件格式
    cfgPathDir.setNameFilters(filters);                   //设置文件名称过滤器，只为filters格式
    int dirCount = cfgPathDir.count();
    if(dirCount <= 0){
        return;
    }
    char tmpname[256];
    std::list<std::string> stringList;
    for(int i=0; i<dirCount&&i<5; i++) {
        QString filName = cfgPathDir[i];
        qDebug() << filName;
        sprintf(tmpname, "%s\\%s", cfgPathDir.absolutePath().toStdString().c_str(),
                filName.toStdString().c_str());
        stringList.push_back(tmpname);
    }
//    run(stringList, "D:\\test\\hls-\\outfile.mp4");
    run(stringList, "D:\\test\\devrecorda02c237ee64441e5b90f5e6c7ab616cf2\\merged.mp4");
#endif

#if 0
    std::list<std::string> stringList;
    char tmpname[256];
    for(int i=0; i<15; i++) {
        sprintf(tmpname, "D:\\test\\hls-\\out%03d.ts", i);
//        sprintf(tmpname, "D:\\test\\qinghuaci_ts\\course-%04d.ts", i);
        stringList.push_back(tmpname);
    }

    FFmpegFileMerger merger;
    merger.merge(stringList, "D:\\test\\hls-\\outfile.mp4");
//    merger.merge(list, "D:\\test\\qinghuaci_ts\\merged.mp4");
#endif

}
