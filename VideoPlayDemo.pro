#-------------------------------------------------
#
# Project created by QtCreator 2021-08-02T13:55:10
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = VideoPlayDemo
TEMPLATE = app

CONFIG += c++11

win32 {
    message("win32")
    contains(QT_ARCH, x86_64) {
        message("64 bit build")
        INCLUDEPATH += $$PWD\3rdparts\ffmpeg\Win64\include
        INCLUDEPATH += $$PWD\3rdparts\sdl2\Win64\include
        #DEPENDPATH += $$PWD\3rdparts\ffmpeg\Win64\include
        LIBS += -L$$PWD\3rdparts\ffmpeg\Win64\lib
        LIBS += -L$$PWD\3rdparts\sdl2\Win64\lib
        LIBS += -L$$PWD\3rdparts\sdl2\Win64\bin

        LIBS += avcodec.lib avformat.lib avutil.lib swresample.lib swscale.lib  avfilter.lib
        LIBS += SDL2.lib
    } else {
        message("32 bit build")
        INCLUDEPATH += $$PWD\3rdparts\ffmpeg\Win32\include
        INCLUDEPATH += $$PWD\3rdparts\sdl2\Win32\include
        #DEPENDPATH += $$PWD\3rdparts\ffmpeg\Win32\include
        LIBS += -L$$PWD\3rdparts\ffmpeg\Win32\lib
        LIBS += -L$$PWD\3rdparts\sdl2\Win32\lib
        LIBS += -L$$PWD\3rdparts\sdl2\Win32\bin

        LIBS += avcodec.lib avformat.lib avutil.lib swresample.lib swscale.lib  avfilter.lib
        LIBS += SDL2.lib
    }
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        mainwindow.cpp \
    encoder/FFmpegFileEncoder.cpp \
    decoder/FFmpegMediaDecoder.cpp \
    decoder/AVSynchronizer.cpp \
    scaler/FFmpegSwscale.cpp \
    scaler/FFmpegSwresample.cpp \
    driver/opengl/OpenGLWidget.cpp \
    model/MediaData.cpp \
    driver/VideoWidget.cpp \
    driver/opengl/OpenGLYuvWidget.cpp \
    driver/opengl/Yuv420pRender.cpp \
    driver/opengl/YUVGLWidget.cpp \
    driver/opengl/I420PlayerWidget.cpp \
    driver/opengl/OpenGLDisplay.cpp \
    driver/opengl/XVideoWidget.cpp \
    driver/opengl/OpenGLWidget0.cpp \
    decoder/FFmpegMediaScaler.cpp \
    driver/sdl/SdlAudioPlayer.cpp \
    driver/sdl/SdlAudioDevice.cpp \
    driver/VideoBuffer.cpp \
    driver/sdl/WaveformProcessor.cpp \
    driver/sdl/SmtAudioPlayer.cpp \
    driver/sdl/SdlEventDispatcher.cpp \
    decoder/StreamMediaDecoder.cpp \
    common/RepeatableThread.cpp \
    decoder/FFAvioContextHandle.cpp

HEADERS += \
        mainwindow.h \
    encoder/FFmpegFileEncoder.h \
    decoder/FFmpegMediaDecoder.h \
    decoder/AVSynchronizer.h \
    scaler/FFmpegSwscale.h \
    scaler/FFmpegSwresample.h \
    driver/opengl/OpenGLWidget.h \
    model/MediaData.h \
    model/MediaQueue.h \
    driver/VideoWidget.h \
    driver/opengl/OpenGLYuvWidget.h \
    driver/opengl/Yuv420pRender.h \
    driver/opengl/YUVGLWidget.h \
    driver/opengl/I420PlayerWidget.h \
    driver/opengl/OpenGLDisplay.h \
    driver/opengl/XVideoWidget.h \
    driver/opengl/OpenGLWidget0.h \
    decoder/FFmpegMediaScaler.h \
    driver/sdl/SdlAudioPlayer.h \
    driver/sdl/SdlAudioDevice.h \
    driver/sdl/SampleBuffer.h \
    driver/VideoBuffer.h \
    driver/sdl/WaveformProcessor.h \
    driver/sdl/LockedMap.h \
    driver/sdl/SmtAudioPlayer.h \
    driver/sdl/SdlEventDispatcher.h \
    decoder/StreamMediaDecoder.h \
    common/RepeatableThread.h \
    decoder/FFAvioContextHandle.h

FORMS += \
        mainwindow.ui

RESOURCES += \
    res.qrc
