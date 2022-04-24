#include "mainwindow.h"
#include <QApplication>
#include "driver/sdl/SmtAudioPlayer.h"
#include <QDebug>
#include "encoder/FFmpegFileEncoder.h"
#include "encoder/FFmpegFileMerger.h"
#include "encoder/FFmpegAudioMerger.h"
#include "decoder/m3u8parsing.h"
//#include "vld.h"

void _ffmpeg_codec(std::string outfile, std::string *out_fmt, enum AVCodecID *out_codec)
{
    std::string ext_str;
    for (int i = outfile.size()-1; i >=0; i--) {
        if (outfile[i] == '.')
			ext_str = outfile.substr(i+1, outfile.size()-(i+1));
    }

    if (!strcmp(ext_str.data(), "mp3")) {
        *out_fmt = "mp4";
        *out_codec = AV_CODEC_ID_MP3;
    } else if (!strcmp(ext_str.data(), "aac")) {
        *out_fmt = "mp4";
        *out_codec = AV_CODEC_ID_AAC;
    }
}

class FFAudioMerger : public FFmpegAudioMerger, private FFMergerCallback
{
public:
    FFAudioMerger() {
        setCallback(this);
    }
    void onMergerProgress(long current, long total) {
        int ratio = (int)(100*current/total);
        if(progress!=ratio) {
            progress = ratio;
            qDebug("#current:%ld duration:%ld, %d%%", current, total, ratio);
        }
    }
    void onMergerError(int code, const char* msg) {
        qDebug("FFAudioMerger Error:%s", msg);
    }
private:
    int progress;
};

void useOpenGLMode(int openGLMode)
{
    // webengine设置,必须在QApplication之前运行
    switch (openGLMode) {
    case 1:
//        QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
//        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
//        QGuiApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
//        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
        break;
    case 2:
//        QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
//        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
//        QGuiApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
//        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        break;
    case 3:
//        QApplication::setAttribute(Qt::AA_UseOpenGLES);
//        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
//        QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
//        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        break;
    default:
        break;
    }
}

void sdlTest(){
    SmtAudioPlayer *player = SmtAudioPlayer::inst();
    player->initialize();
    FFmpegMediaDecoder decoder;
    for(;;) {
        decoder.open("rtsp://192.168.1.151:554/audio_stereo", NULL, false);
        decoder.close();
    }
}

int main(int argc, char *argv[])
{
//    M3u8Parsing parsing;
//    parsing.test();

    FFAudioMerger merger;
    std::string outfile = "D:/test/1hour_test0_.mp3";
    std::string out_fmt = "";
    enum AVCodecID out_codec;
    _ffmpeg_codec(outfile, &out_fmt, &out_codec);
    printf("==_ffmpeg_codec==%s %d\n", out_fmt.data(), out_codec);
    merger.start(outfile, out_fmt, out_codec,//AV_CODEC_ID_AAC,AV_CODEC_ID_MP3
                 0, AV_SAMPLE_FMT_NONE, 32000);
    merger.merge("http://192.168.1.64:9080/m3u8file/DD79CDE0-21A6-4251-AE28-65A8096D574E1359.m3u8", 0, 0);
//    merger.merge("D:\\test\\devrecorda02c237ee64441e5b90f5e6c7ab616cf2\\E7519A80-8BB2-4955-BBD4-D0F1DFD26CD81285.m3u8",
//                 40, 60*60);
    merger.finish();

//    useOpenGLMode(2);

    QApplication a(argc, argv);

    qRegisterMetaType<std::shared_ptr<MediaData>>("std::shared_ptr<MediaData>");
    qRegisterMetaType<std::shared_ptr<SampleBuffer>>("std::shared_ptr<SampleBuffer>");

//    FFmpegFileEncoder encoder;
//    encoder.test();

//    FFmpegFileMerger merger;
//    merger.test2();

    MainWindow w;
    w.show();

    return a.exec();
}
