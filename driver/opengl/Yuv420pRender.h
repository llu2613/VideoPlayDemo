﻿#ifndef YUV420PRENDER_H
#define YUV420PRENDER_H

#include <QObject>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

class Yuv420pRender : protected QOpenGLFunctions
{

public:
    Yuv420pRender();
    ~Yuv420pRender();

    //初始化gl
    void initialize();
    //刷新显示
    void render(uchar* py,uchar* pu,uchar* pv,int width,int height,int type);
    void render(uchar* ptr,int width,int height,int type);

private:
    //shader程序
    QOpenGLShaderProgram m_program;
    //shader中yuv变量地址
    GLuint m_textureUniformY, m_textureUniformU , m_textureUniformV;
    //创建纹理
    GLuint m_idy , m_idu , m_idv;

};

#endif // YUV420PRENDER_H
