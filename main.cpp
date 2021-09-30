#include "mainwindow.h"
#include <QApplication>
#include "driver/sdl/SdlAudioPlayer.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qRegisterMetaType<std::shared_ptr<MediaData>>("std::shared_ptr<MediaData>");
    qRegisterMetaType<std::shared_ptr<SampleBuffer>>("std::shared_ptr<SampleBuffer>");

    MainWindow w;
    w.show();

    return a.exec();
}
