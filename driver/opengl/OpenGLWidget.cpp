#include "OpenGLWidget.h"
#include <QDebug>

// 自动加双引号
#define GET_STR(x) #x
#define A_VER 0
#define T_VER 1

// 顶点shader
const char *vvString = GET_STR(
    attribute vec4 vertexIn;
attribute vec2 textureIn;
varying vec2 textureOut;
void main(void)
{
    gl_Position = vertexIn;
    textureOut = textureIn;
}
);

// 片元shader
const char *ttString = GET_STR(
    varying vec2 textureOut;
uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;
void main(void)
{
    vec3 yuv;
    vec3 rgb;
    yuv.x = texture2D(tex_y, textureOut).r;
    yuv.y = texture2D(tex_u, textureOut).r - 0.5;
    yuv.z = texture2D(tex_v, textureOut).r - 0.5;
    rgb = mat3(1.0, 1.0, 1.0,
        0.0, -0.39465, 2.03211,
        1.13983, -0.58060, 0.0) * yuv;
    gl_FragColor = vec4(rgb, 1.0);
}
);

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{

}

OpenGLWidget::~OpenGLWidget()
{

}

// 初始化
void OpenGLWidget::init(int width, int height)
{
    qDebug()<<QString("OpenGLWidget %1 x %2").arg(width).arg(height);
    std::unique_lock<std::mutex> guard(m_mutex);
    this->width = width;
    this->height = height;
    delete datas[0];
    delete datas[1];
    delete datas[2];
    // 分配材质内存空间
    datas[0] = new unsigned char[width * height];		// Y
    datas[1] = new unsigned char[width * height / 4];	// U
    datas[2] = new unsigned char[width * height / 4];	// V
/*
    if (texs[0])
    {
        glDeleteTextures(3, texs);
    }
    // 创建材质
    glGenTextures(3, texs);

    // Y
    glBindTexture(GL_TEXTURE_2D, texs[0]);
    // 放大过滤，线性插值   GL_NEAREST(效率高，但马赛克严重)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // 创建材质显卡空间
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

    // U
    glBindTexture(GL_TEXTURE_2D, texs[1]);
    // 放大过滤，线性插值
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // 创建材质显卡空间
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

    // V
    glBindTexture(GL_TEXTURE_2D, texs[2]);
    // 放大过滤，线性插值
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // 创建材质显卡空间
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
*/
}

// 重绘数据
void OpenGLWidget::repaintView(MediaData *frame)
{
    std::unique_lock<std::mutex> guard(m_mutex);
    // 容错，保证尺寸正确
    if (!datas[0] || width*height == 0 || frame->width != this->width || frame->height != this->height)
    {
//        av_frame_free(&frame);
        printf("different display size screen:%dx%d, media:%dx%d", width, height, frame->width, frame->height);
        return;
    }
    if (width == frame->linesize[0]) // 无需对齐
    {
        memcpy(datas[0], frame->data[0], width*height);
        memcpy(datas[1], frame->data[1], width*height / 4);
        memcpy(datas[2], frame->data[2], width*height / 4);

        linesize[0] = frame->linesize[0];
        linesize[1] = frame->linesize[1];
        linesize[2] = frame->linesize[2];
    }
    else // 行对齐问题
    {
        for(int i = 0; i < height; i++) // Y
            memcpy(datas[0] + width*i, frame->data[0] + frame->linesize[0]*i, width);
        for (int i = 0; i < height/2; i++) // U
            memcpy(datas[1] + width/2*i, frame->data[1] + frame->linesize[1] * i, width);
        for (int i = 0; i < height/2; i++) // V
            memcpy(datas[2] + width/2*i, frame->data[2] + frame->linesize[2] * i, width);

        linesize[0] = frame->linesize[0];
        linesize[1] = frame->linesize[1];
        linesize[2] = frame->linesize[2];
    }

//    av_frame_free(&frame);
    //qDebug() << "刷新显示" << endl;
    // 刷新显示
    update();
}

//初始化opengl
void OpenGLWidget::initializeGL()
{
    qDebug() << "initializeGL";
    return;
    std::unique_lock<std::mutex> guard(m_mutex);
    // 初始化opengl （QOpenGLFunctions继承）函数
    initializeOpenGLFunctions();

    // program加载shader（顶点和片元）脚本
    // 片元（像素）
    qDebug() << program.addShaderFromSourceCode(QOpenGLShader::Fragment, ttString);
    // 顶点shader
    qDebug() << program.addShaderFromSourceCode(QOpenGLShader::Vertex, vvString);

    // 设置顶点坐标的变量
    program.bindAttributeLocation("vertexIn", A_VER);

    // 设置材质坐标
    program.bindAttributeLocation("textureIn", T_VER);

    // 编译shader
    qDebug() << "program.link() = " << program.link();

    qDebug() << "program.bind() = " << program.bind();

    // 传递顶点和材质坐标
    // 顶点
    static const GLfloat ver[] = {
        -1.0f,-1.0f,
        1.0f,-1.0f,
        -1.0f, 1.0f,
        1.0f,1.0f
    };

    // 材质
    static const GLfloat tex[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };

    // 顶点
    glVertexAttribPointer(A_VER, 2, GL_FLOAT, 0, 0, ver);
    glEnableVertexAttribArray(A_VER);

    // 材质
    glVertexAttribPointer(T_VER, 2, GL_FLOAT, 0, 0, tex);
    glEnableVertexAttribArray(T_VER);


    // 从shader获取材质
    unis[0] = program.uniformLocation("tex_y");
    unis[1] = program.uniformLocation("tex_u");
    unis[2] = program.uniformLocation("tex_v");

    //ue
    {
        GLuint &textureY = texs[0];
        GLuint &textureU = texs[1];
        GLuint &textureV = texs[2];
        //创建纹理
        glGenTextures(1, &textureY);
        glBindTexture(GL_TEXTURE_2D, textureY);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &textureU);
        glBindTexture(GL_TEXTURE_2D, textureU);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &textureV);
        glBindTexture(GL_TEXTURE_2D, textureV);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

// 刷新显示
/*
void OpenGLWidget::paintGL()
{
    qDebug()<<"------------paintGL------------";
    std::unique_lock<std::mutex> guard(m_mutex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texs[0]); // 0层绑定到Y材质
                                           // 修改材质内容(复制内存内容)
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, datas[0]);
    // 与shader uni遍历关联
    glUniform1i(unis[0], 0);


    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, texs[1]); // 1层绑定到U材质
                                           // 修改材质内容(复制内存内容)
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, datas[1]);
    // 与shader uni遍历关联
    glUniform1i(unis[1], 1);


    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, texs[2]); // 2层绑定到V材质
                                           // 修改材质内容(复制内存内容)
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, datas[2]);
    // 与shader uni遍历关联
    glUniform1i(unis[2], 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // qDebug() << "paintGL";
}
*/

void OpenGLWidget::paintGL()
{
    return;
    GLuint &textureY = texs[0];
    GLuint &textureU = texs[1];
    GLuint &textureV = texs[2];
    GLint textureUniformY = unis[0];
    GLint textureUniformU = unis[1];
    GLint textureUniformV = unis[2];
    GLint &linesizeY = linesize[0];
    GLint &linesizeU = linesize[1];
    GLint &linesizeV = linesize[2];
    GLvoid *dataY = datas[0];
    GLvoid *dataU = datas[1];
    GLvoid *dataV = datas[2];

    if (!dataY || !dataU || !dataV || width == 0 || height == 0) {
//        this->initColor();
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureY);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesizeY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, dataY);
    glUniform1i(textureUniformY, 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, textureU);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesizeU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width >> 1, height >> 1, 0, GL_RED, GL_UNSIGNED_BYTE, dataU);
    glUniform1i(textureUniformU, 1);

    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, textureV);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesizeV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width >> 1, height >> 1, 0, GL_RED, GL_UNSIGNED_BYTE, dataV);
    glUniform1i(textureUniformV, 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// 窗口尺寸变化
void OpenGLWidget::resizeGL(int width, int height)
{
    std::unique_lock<std::mutex> guard(m_mutex);
    qDebug() << "resizeGL " << width << ":" << height;
}
/*
void OpenGLWidget::initColor()
{
    //取画板背景颜色
    QColor color = palette().background().color();
    //设置背景清理色
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    //清理颜色背景
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLWidget::initShader()
{
    //加载顶点和片元脚本
    program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertShader);
    program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragShader);

    //设置顶点位置
    program.bindAttributeLocation("vertexIn", 0);
    //设置纹理位置
    program.bindAttributeLocation("textureIn", 1);
    //编译shader
    program.link();
    program.bind();

    //从shader获取地址
    unis[0] = program.uniformLocation("tex_y");
    unis[1] = program.uniformLocation("tex_u");
    unis[2] = program.uniformLocation("tex_v");
}

void OpenGLWidget::initTextures()
{
    GLuint &textureY = texs[0];
    GLuint &textureU = texs[1];
    GLuint &textureV = texs[2];
    //创建纹理
    glGenTextures(1, &textureY);
    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &textureU);
    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &textureV);
    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
*/
