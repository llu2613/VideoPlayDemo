#ifndef OPENGLYUVWIDGET_H
#define OPENGLYUVWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include "Yuv420pRender.h"
#include "model/MediaData.h"

class OpenGLYuvWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit OpenGLYuvWidget(QWidget *parent = nullptr);

    void repaintView(MediaData *data);

protected:
    void initializeGL(); // 初始化gl
    void paintGL(); // 刷新显示
    void resizeGL(int width, int height); // 窗口尺寸变化

private:
    Yuv420pRender render;
    QMutex mutex;
    int width;
    int height;
    uchar *yuv;
    int yuvsize;

signals:

public slots:
};

#endif // OPENGLYUVWIDGET_H
