#include "FFmpegRecorderMulti.h"

FFmpegRecorderMulti::FFmpegRecorderMulti()
{
    filename.clear();
    src_ch_layout = 0;
    src_sample_fmt = AV_SAMPLE_FMT_NONE;
    src_sample_rate = 0;
    out_ch_layout = 0;
    out_sample_fmt = AV_SAMPLE_FMT_NONE;
    out_sample_rate = 0;

    recorder.setCallback(this);
}

FFmpegRecorderMulti::~FFmpegRecorderMulti()
{
    if(recorder.isReady())
        recorder.close();
}

void FFmpegRecorderMulti::setMaxSeconds(long numeral)
{
    this->numeral = numeral;
}

int FFmpegRecorderMulti::open(const char *output,
                            int64_t  src_ch_layout,
                            enum AVSampleFormat src_sample_fmt,
                            int src_sample_rate,
                            int64_t  out_ch_layout,
                            enum AVSampleFormat out_sample_fmt,
                            int out_sample_rate) {
    filename = output;
    segment_count = 1;

    this->src_ch_layout = src_ch_layout;
    this->src_sample_fmt = src_sample_fmt;
    this->src_sample_rate = src_sample_rate;
    this->out_ch_layout = out_ch_layout;
    this->out_sample_fmt = out_sample_fmt;
    this->out_sample_rate = out_sample_rate;

    return 0;
}

int FFmpegRecorderMulti::addData(AVFrame *frame, AVPacket *packet)
{
    if(!recorder.isReady()) {
        recorder.open(segmentFileName(filename, segment_count).data(),
                      src_ch_layout, src_sample_fmt, src_sample_rate,
                      out_ch_layout, out_sample_fmt, out_sample_rate);
        add_samples = 0;
    }
    int ret = recorder.addData(frame, packet);
    if(ret>=0)
        add_samples += frame->nb_samples;
    else
        add_samples = 0;

    int channels = av_get_channel_layout_nb_channels(src_ch_layout);
    long seconds = add_samples/src_sample_rate/(channels>0?channels:1);
    if(seconds>=numeral) {
        recorder.close();
        segment_count++;
    }
    return ret;
}

void FFmpegRecorderMulti::close()
{
    recorder.close();

    filename.clear();
    src_ch_layout = 0;
    src_sample_fmt = AV_SAMPLE_FMT_NONE;
    src_sample_rate = 0;
    out_ch_layout = 0;
    out_sample_fmt = AV_SAMPLE_FMT_NONE;
    out_sample_rate = 0;
}

#include <QDebug>
void FFmpegRecorderMulti::onRecordError(int level, const char* msg)
{
    qDebug()<<"onRecordError"<<msg;
}

std::string FFmpegRecorderMulti::segmentFileName(std::string filename, int segment)
{
    int index = -1;
    for(int i=filename.size()-1; i>=0; i--) {
        char ch = filename[i];
        if(ch=='\\'||ch=='/')
            break;
        else if(ch=='.')
            index = i;
    }
    if(index!=-1) {
        char seg[32];
        sprintf(seg, "_%d", segment);
        std::string newname = filename;
        newname.insert(index, seg);
        return newname;
    }
    return filename;
}
