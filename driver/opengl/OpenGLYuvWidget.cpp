#include "OpenGLYuvWidget.h"

OpenGLYuvWidget::OpenGLYuvWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    yuv = nullptr;
    yuvsize = 0;
}

void OpenGLYuvWidget::repaintView(MediaData *data)
{
    int y_size = data->width * data->height;

    mutex.lock();
    if(!yuv || yuvsize<(y_size+y_size/2)) {
        if(yuv) delete yuv;
        yuv = new uchar[y_size*3];
        yuvsize = y_size*3;
    }
    memcpy(yuv, data->data[0], y_size);
    memcpy(yuv + y_size, data->data[1], y_size/4);
    memcpy(yuv + y_size + y_size/4, data->data[2], y_size/4);
    width = data->width;
    height = data->height;
    mutex.unlock();

    repaint();
}

void OpenGLYuvWidget::initializeGL()
{
    render.initialize();
}

void OpenGLYuvWidget::paintGL()
{
    if(yuv && width*height>0) {
        mutex.lock();
        render.render(yuv, width, height, 0);
        mutex.unlock();
    }
}

void OpenGLYuvWidget::resizeGL(int width, int height)
{
//    glViewport(0, 0, w, h);
}
