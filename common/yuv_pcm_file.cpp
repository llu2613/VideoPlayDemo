#include "yuv_pcm_file.h"

yuv_pcm_file::yuv_pcm_file()
{
    enable_pcm = false;
    enable_yuv = false;
    fp_pcm = NULL;
    fp_yuv = NULL;
    pcm_file[0] = '\0';
    yuv_file[0] = '\0';
}

void yuv_pcm_file::set_pcm_file(const char* filename)
{
    if(fp_pcm) return;

    int namelen = strlen(filename)+1;
    size_t len = namelen<sizeof(pcm_file)?namelen:sizeof(pcm_file);
    memcpy(pcm_file, filename, len);

    fp_pcm = fopen(pcm_file, "wb");
}

void yuv_pcm_file::set_yuv_file(const char* filename)
{
    int namelen = strlen(filename)+1;
    size_t len = namelen<sizeof(yuv_file)?namelen:sizeof(yuv_file);
    memcpy(yuv_file, filename, len);
    enable_yuv = true;
}

void yuv_pcm_file::write_pcm_data(unsigned char *buf, int len)
{
    if(fp_pcm) {
//        int out_buffer_size = av_samples_get_buffer_size(NULL, 2, frame->nb_samples,
//                AV_SAMPLE_FMT_S16, 1);
//        fwrite(frame->data[0], 1, out_buffer_size, fp_pcm);
        fwrite(buf, 1, len, fp_pcm);
    }
}

void yuv_pcm_file::write_yuv_data(unsigned char **data, int width, int height)
{
    if(fp_yuv) {
        //输出到YUV文件
        //AVFrame像素帧写入文件
        //data解码后的图像像素数据（音频采样数据）
        //Y 亮度 UV 色度（压缩了） 人对亮度更加敏感
        //U V 个数是Y的1/4
        int y_size = width * height;
        fwrite(data[0], 1, y_size, fp_yuv);
        fwrite(data[1], 1, y_size / 4, fp_yuv);
        fwrite(data[2], 1, y_size / 4, fp_yuv);
    }
}

void yuv_pcm_file::finish_pcm_file()
{
    if(fp_pcm) {
        fclose(fp_pcm);
        fp_pcm = NULL;
    }
}

void yuv_pcm_file::finish_yuv_file()
{
    if(fp_yuv) {
        fclose(fp_yuv);
        fp_yuv = NULL;
    }
}
