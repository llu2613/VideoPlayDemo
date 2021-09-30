#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMutex>

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture)

#define VIDEO_SIZE_ALIGN   8  //64位系统8字节对齐

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
   Q_OBJECT

public:
    explicit OpenGLWidget(QWidget *parent = 0);
     ~OpenGLWidget();
    QRect getVideoRect();
    bool isSupportGPUShow();

protected:
    void initializeGL()Q_DECL_OVERRIDE;
    void paintGL()Q_DECL_OVERRIDE;
    void resizeGL(int w, int h)Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *e)Q_DECL_OVERRIDE;

signals:
     void sigUpdateVideoPos(int xOffset,int yOffset,int w,int h);

public slots:
    void slotShowYuv(uchar *ptr,uint width,uint height); //显示一帧Yuv图像
    void slotClear();

public:
    QOpenGLShaderProgram *program;

    QOpenGLBuffer vbo;
    GLuint textureUniformY,textureUniformU,textureUniformV; //opengl中y、u、v分量位置
    QOpenGLTexture *textureY ,*textureU ,*textureV ;
    GLuint idY,idU,idV; //自己创建的纹理对象ID，创建错误返回0
    uint videoW,videoH;
    uint videoSrcW,videoSrcH;
    uchar *yuvPtr;
    bool  bShowVideo;
    QMutex mDataMuxtex;

    int mXOffset;
    int mYOffset;
    int mWidth;
    int mHeight;

    //CPU渲染
    QImage mImage;
    int mVideoShowMode;  //视频渲染方式
    bool mIsSupportGPUShow;

};

#endif // OPENGLWIDGET_H
