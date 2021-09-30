#ifndef XVIDEOWIDGET_H
#define XVIDEOWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include "model/MediaData.h"

class XVideoWidget : public QOpenGLWidget,protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit XVideoWidget(QWidget *parent = nullptr);

    void repaintView(MediaData *mediaData);

protected:
    void initializeGL();
    void paintGL();
    void resizeGL(int width,int height);

private:
    GLuint texs[3] = {0};
    GLuint unis[3] = {0};
    unsigned char* datas[3] = {0};

    QOpenGLShaderProgram program;

    int width = 1280;
    int height = 720;

};

#endif // XVIDEOWIDGET_H
