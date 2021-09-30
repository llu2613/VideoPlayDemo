#include "OpenGLWidget.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QDebug>
#include<QPainter>
#include<QPen>
#define VERTEXIN 0
#define TEXTUREIN 1


OpenGLWidget::OpenGLWidget(QWidget *parent):QOpenGLWidget(parent)
{
    textureY=NULL;
    textureU=NULL;
    textureV=NULL;
    yuvPtr=NULL;
    bShowVideo=false;
    mXOffset=0;
    mYOffset=0;
    mWidth=1;
    mHeight=1;
    videoW = 0;
    videoH = 0;
    program=NULL;
    mIsSupportGPUShow=true;
//    mVideoShowMode=ConfigHelper::getConfigHelper()->getConfigFileModel().mVideoShowMode;

    this->setWindowFlags(Qt::FramelessWindowHint);
}

QRect OpenGLWidget::getVideoRect()
{
    QRect rect;
    rect.setX(mXOffset);
    rect.setY(mYOffset);
    rect.setWidth(mWidth);
    rect.setHeight(mHeight);
    return rect;
}



bool OpenGLWidget::isSupportGPUShow()
{
    return mIsSupportGPUShow;
}

OpenGLWidget::~OpenGLWidget()
{
//    if(mVideoShowMode==VIDEO_SHOW_GPU_MODE)
//    {
        makeCurrent();
        vbo.destroy();
        if(textureY){
            textureY->destroy();
        }

        if(textureU){
            textureU->destroy();
        }

        if(textureV){
            textureV->destroy();
        }
        doneCurrent();

//    }

}

void OpenGLWidget::initializeGL()
{
//  if(mVideoShowMode==VIDEO_SHOW_GPU_MODE)
//  {

    //调用内容初始化函数
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    static const GLfloat vertices[]{
        //顶点坐标
        -1.0f,-1.0f,
        -1.0f,+1.0f,
        +1.0f,+1.0f,
        +1.0f,-1.0f,
        //纹理坐标
        0.0f,1.0f,
        0.0f,0.0f,
        1.0f,0.0f,
        1.0f,1.0f,
    };


    vbo.create();
    vbo.bind();
    vbo.allocate(vertices,sizeof(vertices));

    QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex,this);

    if(vshader==NULL)
    {
        return;
    }


    const char *vsrc =
    "attribute vec4 vertexIn; \
    attribute vec2 textureIn; \
    varying vec2 textureOut;  \
    void main(void)           \
    {                         \
        gl_Position = vertexIn; \
        textureOut = textureIn; \
    }";
    bool result=vshader->compileSourceCode(vsrc);

    if(!result)
    {
        printf("can not vshader compileSourceCode！\r\n");
    }


    QOpenGLShader *fshader= new QOpenGLShader(QOpenGLShader::Fragment,this);

    if(fshader==NULL)
    {
        return;
    }

    const char *fsrc = "varying vec2 textureOut; \
    uniform sampler2D tex_y; \
    uniform sampler2D tex_u; \
    uniform sampler2D tex_v; \
    void main(void) \
    { \
        vec3 yuv; \
        vec3 rgb; \
        yuv.x = texture2D(tex_y, textureOut).r; \
        yuv.y = texture2D(tex_u, textureOut).r - 0.5; \
        yuv.z = texture2D(tex_v, textureOut).r - 0.5; \
        rgb = mat3( 1,       1,         1, \
                    0,       -0.39465,  2.03211, \
                    1.13983, -0.58060,  0) * yuv; \
        gl_FragColor = vec4(rgb, 1); \
    }";
    result=fshader->compileSourceCode(fsrc);
    if(!result)
    {
        printf("can not fshader compileSourceCode！\r\n");
    }

    program = new QOpenGLShaderProgram(this);
    mIsSupportGPUShow&=program->addShader(vshader);
    mIsSupportGPUShow&=program->addShader(fshader);
    program->bindAttributeLocation("vertexIn",VERTEXIN);
    program->bindAttributeLocation("textureIn",TEXTUREIN);
    mIsSupportGPUShow&=program->link();
    mIsSupportGPUShow&=program->bind();
    program->enableAttributeArray(VERTEXIN);
    program->enableAttributeArray(TEXTUREIN);
    program->setAttributeBuffer(VERTEXIN,GL_FLOAT,0,2,2*sizeof(GLfloat));
    program->setAttributeBuffer(TEXTUREIN,GL_FLOAT,8*sizeof(GLfloat),2,2*sizeof(GLfloat));

    textureUniformY = program->uniformLocation("tex_y");
    textureUniformU = program->uniformLocation("tex_u");
    textureUniformV = program->uniformLocation("tex_v");
    textureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    mIsSupportGPUShow&=textureY->create();
    mIsSupportGPUShow&=textureU->create();
    mIsSupportGPUShow&=textureV->create();
    idY = textureY->textureId();
    idU = textureU->textureId();
    idV = textureV->textureId();
    glClearColor(0.0,0.0,0.0,0.0);

//  }
}

void OpenGLWidget::resizeGL(int w, int h)
{
    //当窗口大小改变时，调整界面坐标显示高度和宽度
//   if(mVideoShowMode==VIDEO_SHOW_GPU_MODE)
//   {
        if(h=0){
            h=1;
        }
        glViewport(0, 0, w, h);
//   }
}

void OpenGLWidget::paintEvent(QPaintEvent *e)
{
//    if(mVideoShowMode==VIDEO_SHOW_CPU_MODE)
//    {
        mDataMuxtex.lock();
        QPainter painter(this);
//        int themeIndex =  ConfigHelper::getConfigHelper()->getConfigFileModel().mTheme;
//        if (themeIndex == 0) {
            QBrush bg(QColor(36,36,36,255)); //视频区域背景颜色
            painter.setBrush(bg);
//        } else if (themeIndex == 1) {
//            QBrush bg(QColor(255,255,255,255)); //视频区域背景颜色
//             painter.setBrush(bg);
//        }
        painter.drawRect(0, 0, width(), height()); //先画成黑色

        if(bShowVideo && (mImage.size().width()>0))
        {

            //根据窗口计算应该显示的图片的大小
            int nOffsetX=0;
            int nOffsetY=0;

            mXOffset=nOffsetX;
            mYOffset=nOffsetY;
            mWidth=this->width();
            mHeight= this->height();

           //铺满窗口
           sigUpdateVideoPos(nOffsetX,nOffsetY,this->width(),this->height());

           //显示图片
           if (mImage.size().width() > 0){
               painter.drawImage(mXOffset,mYOffset,mImage.scaled(mWidth,mHeight));
           }
        }
        mDataMuxtex.unlock();
//    }
//    else
//    {
//          QOpenGLWidget::paintEvent(e);
//    }
}

void OpenGLWidget::paintGL()
{

//    if(mVideoShowMode==VIDEO_SHOW_GPU_MODE)
//    {
        //清除之前图形并将背景设置为黑色（设置为黑色纯粹个人爱好！
//        int themeIndex =  ConfigHelper::getConfigHelper()->getConfigFileModel().mTheme;
//        if (themeIndex == 0) {
            glClearColor(0.14f,0.14f,0.14f,0.0f); //视频区域背景颜色
//        } else if (themeIndex == 1) {
//            glClearColor(255,255,255,1);
//        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mDataMuxtex.lock();
        if(bShowVideo && yuvPtr){

            int  nWidth=videoW;
            int  nHeight=videoH;

            //qDebug()<<"nWidth:"<<nWidth;
            //qDebug()<<"nHeight:"<<nHeight;
            //如何等比例显示视频在窗口中
            //根据窗口计算应该显示的图片的大小
            int nOffsetX=0;
            int nOffsetY=0;
    #if 0
            //等比例显示
            double srcRatio = (double)nWidth*1.0 / (double)nHeight;
            double dstRatio = (double)this->width()*1.0 / (double)this->height();

            double wRatio=(double)nWidth*1.0/(double)this->width();
            double hRatio=(double)nHeight*1.0/(double)this->height();


           // qDebug()<<"srcRatio:"<<srcRatio;
            //qDebug()<<"dstRatio:"<<dstRatio;


            if(fabs(srcRatio - dstRatio) <= 1e-6)
            {
                //比例相同不做处理
                sigUpdateVideoPos(nOffsetX,nOffsetY,this->width(),this->height());

            }
            else if(srcRatio<dstRatio)
            {
                //将视频帧居中显示，左右补黑
                 int width = (int)(this->height()* srcRatio);
                //修正宽值: 因为OpenGL渲染要求宽度是4的整数倍。这里按8的整数倍来计算。+7是避免取8整数倍时得到的是左侧值（如 8 16 24，接近24的话，不能取16）
                 width = (width + 7) >> 3 << 3;

                 int height = this->height();

                 if(this->width()>width)
                 {
                    nOffsetX=(this->width()-width)/2;
                 }
                 else
                 {
                     width=this->width();
                 }

                 mXOffset=nOffsetX;
                 mYOffset=nOffsetY;
                 mWidth=width;
                 mHeight=height;

                 //qDebug()<<"width:"<<mWidth;
                 //qDebug()<<"height:"<<mHeight;

                 glViewport(nOffsetX, nOffsetY, width, height);
                 sigUpdateVideoPos(nOffsetX,nOffsetY,width,height);

            }
            else
            {
                //上下补黑，实现思路与左右补黑相同
                int height = (int)(this->width() / srcRatio);
                int width = (nWidth + 7) >> 3 << 3;
                if(this->width()>width)
                {
                     nOffsetX=floor((this->width()-width)*1.0)/2;
                }
                else
                {
                    width=this->width();
                }
                if(this->height()>height)
                {
                     nOffsetY= floor((this->height() - height)*1.0) / 2;
                }
                else
                {
                    height=this->height();
                }

                mXOffset=nOffsetX;
                mYOffset=nOffsetY;
                mWidth=width;
                mHeight=height;

                glViewport(nOffsetX, nOffsetY, width, height);
                sigUpdateVideoPos(nOffsetX,nOffsetY,width,height);
            }
    #else
            mXOffset=nOffsetX;
            mYOffset=nOffsetY;
            mWidth=this->width();
            mHeight= this->height();

             //铺满窗口
            glViewport(nOffsetX, nOffsetY, this->width(), this->height());
            sigUpdateVideoPos(nOffsetX,nOffsetY,this->width(),this->height());

    #endif
            glActiveTexture(GL_TEXTURE0);  //激活纹理单元GL_TEXTURE0,系统里面的
            glBindTexture(GL_TEXTURE_2D,idY); //绑定y分量纹理对象id到激活的纹理单元

            //使用内存中的数据创建真正的y分量纹理数据
            glTexImage2D(GL_TEXTURE_2D,0,GL_RED,videoW,videoH,0,GL_RED,GL_UNSIGNED_BYTE,yuvPtr);

            //https://blog.csdn.net/xipiaoyouzi/article/details/53584798 纹理参数解析
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glActiveTexture(GL_TEXTURE1); //激活纹理单元GL_TEXTURE1
            glBindTexture(GL_TEXTURE1,idU);

            //使用内存中的数据创建真正的u分量纹理数据
            glTexImage2D(GL_TEXTURE_2D,0,GL_RED,videoW >> 1, videoH >> 1,0,GL_RED,GL_UNSIGNED_BYTE,yuvPtr + videoW * videoH);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glActiveTexture(GL_TEXTURE2); //激活纹理单元GL_TEXTURE2
            glBindTexture(GL_TEXTURE_2D,idV);

            //使用内存中的数据创建真正的v分量纹理数据
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW >> 1, videoH >> 1, 0, GL_RED, GL_UNSIGNED_BYTE, yuvPtr+videoW*videoH*5/4);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            //指定y纹理要使用新值
            glUniform1i(textureUniformY, 0);
            //指定u纹理要使用新值
            glUniform1i(textureUniformU, 1);
            //指定v纹理要使用新值
            glUniform1i(textureUniformV, 2);
            //使用顶点数组方式绘制图形
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        }
        mDataMuxtex.unlock();

//    }
}

void OpenGLWidget::slotShowYuv(uchar *ptr, uint width, uint height)
{
    if(ptr!=NULL){

        yuvPtr = ptr;
        videoW = width;
        videoH = height;

        update();
    }
}

void OpenGLWidget::slotClear()
{
    //该函数为何会导致程序崩溃
//    int themeIndex =  ConfigHelper::getConfigHelper()->getConfigFileModel().mTheme;
//    if (themeIndex == 0) {
        glClearColor(0.14f,0.14f,0.14f,0.0f); //视频区域背景颜色
//    } else if (themeIndex == 1) {
//        glClearColor(255,255,255,1);
//    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    update();

}

