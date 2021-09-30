#include "I420PlayerWidget.h"

//http://yuanfeng.link/index.php/archives/10/


#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4


static const char *vertexShader = "\
        attribute vec4 vertexIn;\
attribute vec2 textureIn;\
varying vec2 textureOut;\
uniform mat4 mWorld;\
uniform mat4 mView;\
uniform mat4 mProj;\
void main(void)\
{\
    gl_Position =vertexIn * mWorld * mView * mProj  ;\
    textureOut = textureIn;\
}";

static const char *fragmentShader =
        #if defined(WIN32)
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        #else
        #endif
        "varying vec2 textureOut;\
        uniform sampler2D tex_y;\
uniform sampler2D tex_u;\
uniform sampler2D tex_v;\
void main(void)\
{\
    vec3 yuv;\
    vec3 rgb;\
    yuv.x = texture2D(tex_y, textureOut).r;\
    yuv.y = texture2D(tex_u, textureOut).r - 0.5;\
    yuv.z = texture2D(tex_v, textureOut).r - 0.5;\
    rgb = mat3( 1,       1,         1,\
                0,       -0.39465,  2.03211,\
                1.13983, -0.58060,  0) * yuv;\
    gl_FragColor = vec4(rgb, 1);\
}";

static const GLfloat vertexVertices[] = {
    -1.0f, -1.0f,
    1.0f, -1.0f,
    -1.0f,  1.0f,
    1.0f,  1.0f,
};

static const GLfloat textureVertices[] = {
    0.0f,  1.0f,
    1.0f,  1.0f,
    0.0f,  0.0f,
    1.0f,  0.0f,
};



I420PlayerWidget::I420PlayerWidget(QWidget *parent) : QOpenGLWidget(parent)
{

}

void I420PlayerWidget::setI420Data(std::shared_ptr<const std::vector<uint8_t> > d, int width, int height )
{
    m_cur_data = std::move(d);
    m_width = width;
    m_height = height;

    repaint();
}

void I420PlayerWidget::initializeGL()
{
    initializeOpenGLFunctions();

    const GLubyte* name = glGetString(GL_VENDOR); //返回负责当前OpenGL实现厂商的名字
    const GLubyte* biaoshifu = glGetString(GL_RENDERER); //返回一个渲染器标识符，通常是个硬件平台
    const GLubyte* OpenGLVersion =glGetString(GL_VERSION); //返回当前OpenGL实现的版本号
    //    const GLubyte* OpenGLExensions =glGetString(GL_EXTENSIONS); //

    /// 打印OpenGL版本，标识，厂商信息
    QString str;
    str.sprintf("%s | %s | %s",name, biaoshifu, OpenGLVersion);
    qDebug()<<str;

    initShaders();
}


void I420PlayerWidget::paintGL()
{
    const uint8_t *y= nullptr;
    if (m_cur_data)   // 需要绘制的YUV数据
    {
        y = m_cur_data->data();
    }
    else
    {
        return;
    }



    int w = m_width;
    int h = m_height;
    const uint8_t *u = y + w*h ;
    const uint8_t *v = u + w*h/4;

    // 清除缓冲区
    glClearColor(0.0,0.0,0.0,0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Y
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_y);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, y);
    glUniform1i(sampler_y, 0);

    // U
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_u);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w/2, h/2, 0, GL_RED, GL_UNSIGNED_BYTE, u);
    glUniform1i(sampler_u, 1);

    // V
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex_v);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w/2, h/2, 0, GL_RED, GL_UNSIGNED_BYTE, v);
    glUniform1i(sampler_v, 2);

    //        QOpenGLShaderProgram::setUniformValue();
    glUniformMatrix4fv(matWorld,1, GL_FALSE, mWorld.constData());
    glUniformMatrix4fv(matView,1, GL_FALSE, mView.constData());
    glUniformMatrix4fv(matProj,1, GL_FALSE, mProj.constData());

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glFlush();

}

void I420PlayerWidget::resizeGL(int w, int h)
{
    float viewWidth=2.0f;
    float viewHeight=2.0f;

    mWorld.setToIdentity();

    mView.setToIdentity();
    mView.lookAt(QVector3D(0.0f,0.0f,1.0f),QVector3D(0.f,0.f,0.f),QVector3D(0.f,1.f,0.f));

    mProj.setToIdentity();
    if(m_cur_data) /// 在这里计算宽高比
    {
        float aspectRatio = 1.0*m_width/m_height;
        //aspectRatio = float(4) / 3; // 强制长宽比
        if(float(float(w)/h > aspectRatio))
        {
            viewHeight = 2.0f;
            viewWidth = w*viewHeight / (aspectRatio * h);
        }
        else
        {
            viewWidth = 2.0f;
            viewHeight = h*viewWidth / (1/aspectRatio * w);
        }
    }

    mProj.ortho(-viewWidth/2,viewWidth/2,-viewHeight/2,viewHeight/2,-1.f,1.0f);
}




void I420PlayerWidget::initShaders()
{
    GLint vertCompiled, fragCompiled, linked;
    GLint v, f;

    //Shader: step1
    v = glCreateShader(GL_VERTEX_SHADER);
    f = glCreateShader(GL_FRAGMENT_SHADER);

    //Shader: step2
    glShaderSource(v, 1, &vertexShader,NULL);
    glShaderSource(f, 1, &fragmentShader,NULL);

    //Shader: step3
    glCompileShader(v);
    glGetShaderiv(v, GL_COMPILE_STATUS, &vertCompiled);    //Debug

    glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &fragCompiled);    //Debug

    //Program: Step1
    program = glCreateProgram();
    //Program: Step2
    glAttachShader(program,v);
    glAttachShader(program,f);

    glBindAttribLocation(program, ATTRIB_VERTEX, "vertexIn");
    glBindAttribLocation(program, ATTRIB_TEXTURE, "textureIn");
    //Program: Step3
    glLinkProgram(program);
    //Debug
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    glUseProgram(program);

    //Get Uniform Variables Location
    sampler_y = glGetUniformLocation(program, "tex_y");
    sampler_u = glGetUniformLocation(program, "tex_u");
    sampler_v = glGetUniformLocation(program, "tex_v");

    //Init Texture
    glGenTextures(1, &tex_y);
    glBindTexture(GL_TEXTURE_2D, tex_y);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &tex_u);
    glBindTexture(GL_TEXTURE_2D, tex_u);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &tex_v);
    glBindTexture(GL_TEXTURE_2D, tex_v);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glVertexAttribPointer(ATTRIB_VERTEX,2,GL_FLOAT,0,0,vertexVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);

    glVertexAttribPointer(ATTRIB_TEXTURE,2,GL_FLOAT,0,0,textureVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTURE);

    matWorld = glGetUniformLocation(program,"mWorld");
    matView = glGetUniformLocation(program,"mView");
    matProj = glGetUniformLocation(program,"mProj");
}
