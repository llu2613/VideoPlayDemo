#ifndef I420PLAYERWIDGET_H
#define I420PLAYERWIDGET_H

#include <QOpenGLWidget>
#include <QMatrix4x4>
//#include <qopenglfunctions_4_5_core.h>
#include <QOpenGLFunctions>
#include <memory>

class I420PlayerWidget : public QOpenGLWidget, public QOpenGLFunctions//QOpenGLFunctions_4_5_Core
{
    Q_OBJECT
public:
    explicit I420PlayerWidget(QWidget *parent = nullptr);

    /// I420 就是 YUV420P，这个函数设置裸数据，和图片宽高
    void setI420Data(std::shared_ptr<const std::vector<uint8_t>> d, int width, int height);


    // 以下是绘制YUV420P的方法，继承自QOPenGLWidget
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
private:
    void initShaders();
    GLuint program{0};
    GLuint tex_y{0};
    GLuint tex_u{0};
    GLuint tex_v{0};
    GLuint sampler_y{0};
    GLuint sampler_u{0};
    GLuint sampler_v{0};
    GLuint matWorld{0};
    GLuint  matView{0};
    GLuint  matProj{0};

    QMatrix4x4 mProj;
    QMatrix4x4 mView;
    QMatrix4x4 mWorld;

// 图片宽高，和I420裸数据
    int m_width{-1};
    int m_height{1};
    std::shared_ptr<const std::vector<uint8_t>> m_cur_data{nullptr};// 需要绘制的I420数据

};

#endif // I420PLAYERWIDGET_H
