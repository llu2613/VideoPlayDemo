﻿#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QDialog>
#include <QWidget>
#include <QTimer>
#include <QSlider>
#include "opengl/OpenGLWidget0.h"
#include "opengl/OpenGLYuvWidget.h"
#include "opengl/YUVGLWidget.h"
#include "opengl/I420PlayerWidget.h"
#include "opengl/OpenGLDisplay.h"
#include "opengl/XVideoWidget.h"
#include "model/MediaData.h"
#include "model/MediaQueue.h"
#include "decoder/StreamMediaDecoder.h"
#include "driver/sdl/SmtAudioPlayer.h"
#include "VideoBuffer.h"
#include <memory>

class VideoWidget : public QDialog
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    void setId(int sourceId) {
        mSourceId = sourceId;
    }

    int exec();


protected:
    void closeEvent(QCloseEvent *) override;

private:
    YUVGLWidget *video;
    VideoBuffer videoBuffer;
    QSlider *timeSlider;
    QTimer *mSliderTimer;

    StreamMediaDecoder *decoder;

    SmtAudioPlayer *smtAudioPlayer;

    int mCardId;
    int mSourceId;
    QList<QSize> sizeList;
    QList<AVPixelFormat> fmtList;

    bool m_is_m_rgb24_updated;
    QImage m_rgb24_image;

protected:
    void paintEvent(QPaintEvent *event) override;

signals:

public slots:
    void onAudioData(std::shared_ptr<MediaData> data);
    void onVideoData(std::shared_ptr<MediaData> data);
    void onShowVideoData(std::shared_ptr<MediaData> data);
    void onAudioCardOpened(QString name, int cardId);
    void onMediaDecoderEvent(int event, QString msg);
    void onTimeSliderValueChanged(int value);
    void onSliderTimeout();

private slots:


};

#endif // VIDEOWIDGET_H
