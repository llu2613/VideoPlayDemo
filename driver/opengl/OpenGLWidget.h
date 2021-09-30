#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <mutex>
#include "model/MediaData.h"

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

    void init(int width, int height);

    void repaintView(MediaData *mediaData);

protected:
    void initializeGL(); // 初始化gl
    void paintGL(); // 刷新显示
    void resizeGL(int width, int height); // 窗口尺寸变化

private:
    QOpenGLShaderProgram program; // shader程序
    GLuint unis[3] = { 0 }; // shader中yuv变量地址
    GLuint texs[3] = { 0 }; // openg的 texture地址

    GLint linesize[3] = { 0 };

    // 材质内存空间
    unsigned char *datas[3] = { 0 };
    int width = 240;
    int height = 128;

    std::mutex m_mutex;

};

#endif // OPENGLWIDGET_H
