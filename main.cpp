#include "mainwindow.h"
#include <QApplication>
#include "driver/sdl/SmtAudioPlayer.h"
#include <QDebug>
#include "encoder/FFmpegFileEncoder.h"
#include "encoder/FFmpegFileMerger.h"
#include "encoder/FFmpegAudioMerger.h"
#include "encoder/FFmpegH264Video.h"
#include "decoder/m3u8parsing.h"
#include "tools/FFmpegInfo.h"
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
        setLogLevel(AV_LOG_INFO);
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
/*
    FFAudioMerger merger;
    std::string outfile = "D:/test/pcm/wwwwwwwwwwww.wav";
    std::string out_fmt = "";
    enum AVCodecID out_codec;
    _ffmpeg_codec(outfile, &out_fmt, &out_codec);
    printf("==_ffmpeg_codec==%s %d\n", out_fmt.data(), out_codec);
    merger.start(outfile, out_fmt, out_codec,//AV_CODEC_ID_AAC,AV_CODEC_ID_MP3
                 0, AV_SAMPLE_FMT_NONE, 32000);
//    merger.merge("http://192.168.1.64:9080/m3u8file/AFED897F-3CFE-4106-B078-C87D36B4A65B1433.m3u8", 0, 0);
    merger.merge("D:\\test\\pcm\\20220418145503958-1-1-0A0A4783-8A63-428D-B15F-8518D0CF752D-1483434575.ts",
                 0, 0);
    merger.finish();
*/
//    useOpenGLMode(2);

    QApplication a(argc, argv);

    qRegisterMetaType<std::shared_ptr<MediaData>>("std::shared_ptr<MediaData>");
    qRegisterMetaType<std::shared_ptr<SampleBuffer>>("std::shared_ptr<SampleBuffer>");

//    FFmpegFileEncoder encoder;
//    encoder.test();

//    FFmpegFileMerger merger;
//    merger.test2();

//    FFmpegInfo info;
//    info.read_av_info("D:\\test\\webm\\00000000514000000.mp4");

//    FFmpegH264Video h264video;
//    h264video.raw_h264("D:\\test\\dddddd\\aaaaaaaaaaaa英文提测版\\english.264",
//                   "D:\\test\\dddddd\\aaaaaaaaaaaa英文提测版\\english22.264");

//    info.read_av_info("D:\\test\\webm\\ffff.mp4");

    MainWindow w;
    w.show();

    return a.exec();
}
