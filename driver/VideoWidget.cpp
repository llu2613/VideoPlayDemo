#include "VideoWidget.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QPainter>

#define VIDEO_W 1280
#define VIDEO_H 720

VideoWidget::VideoWidget(QWidget *parent)
    : QDialog(parent), videoBuffer("VideoWidget")
{
    mCardId = 0;
    mSourceId = 0;
    setFixedSize(VIDEO_W+80, VIDEO_H+150);

    decoder = new StreamMediaDecoder();
    decoder->setOutVideo(AV_PIX_FMT_YUV420P, 640, 480);
    //decoder->setOutVideo2(VIDEO_W, VIDEO_H);

//    QObject::connect(decoder, &FFmpegMediaDecoder::audioData,
//                     &avSyncTimer, &AVSyncTimer::onAudioData);
//    QObject::connect(decoder, &FFmpegMediaDecoder::videoData,
//                     &avSyncTimer, &AVSyncTimer::onVideoData);
//    avSyncTimer.start();

//    QObject::connect(&avSyncTimer, &AVSyncTimer::audioData,
//                     this, &VideoWidget::onAudioData);
//    QObject::connect(&avSyncTimer, &AVSyncTimer::videoData,
//                     this, &VideoWidget::onShowVideoData);

    connect(decoder, &StreamMediaDecoder::audioData, this, &VideoWidget::onAudioData);
    connect(decoder, &StreamMediaDecoder::videoData, this, &VideoWidget::onVideoData);
    connect(decoder, &StreamMediaDecoder::mediaDecoderEvent, this, &VideoWidget::onMediaDecoderEvent);

    connect(&videoBuffer, &VideoBuffer::showFrame,
            this, &VideoWidget::onShowVideoData);
    videoBuffer.setSync(decoder->getSynchronizer());
    videoBuffer.start();

    sizeList<<QSize(1920,1080)<<QSize(1280,720)<<QSize(640,480)
           <<QSize(832,480)<<QSize(416,240)
          <<QSize(352,288)<<QSize(175,144);
    fmtList<<AV_PIX_FMT_YUV420P<<AV_PIX_FMT_RGB24;

    // Set core profile
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    qDebug()<<"QSurfaceFormat version:"<<format.version().first<<format.version().second;
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3); // Adapt to your system
    video = new YUVGLWidget(format);
    video->setFrameSize(VIDEO_W, VIDEO_H);

//    video = new XVideoWidget;
//    video->InitDrawBuffer(1280*720+1280*720/2);

//    video = new I420PlayerWidget;
    QVBoxLayout* pVLayout = new QVBoxLayout(this);
    pVLayout->addWidget(video);
//    video->setFixedSize(1280, 720);

    QHBoxLayout* pHLayout = new QHBoxLayout(this);
    QLineEdit *inputUrl = new QLineEdit(this);
    QPushButton *openBtn = new QPushButton("Open It", this);
    QPushButton *closeBtn = new QPushButton("Stop", this);
    QComboBox *sizeBox = new QComboBox(this);
    for(int i=0; i<sizeList.size(); i++) {
        sizeBox->addItem(QString("%1x%2").arg(sizeList[i].width())
                     .arg(sizeList[i].height()));
    }
    QComboBox *fmtBox = new QComboBox(this);
    for(int i=0; i<fmtList.size(); i++) {
        if(fmtList[i]==AV_PIX_FMT_YUV420P)
            fmtBox->addItem("YUV420P");
        else if(fmtList[i]==AV_PIX_FMT_RGB24)
            fmtBox->addItem("RGB24");
    }
    QComboBox *cardBox = new QComboBox(this);
    QStringList cards = SmtAudioPlayer::inst()->devices();
    for(int i=0; i<cards.size(); i++) {
        cardBox->addItem(cards.at(i));
    }


//    inputUrl->setText("http://220.161.87.62:8800/hls/0/index.m3u8");
    inputUrl->setText("rtsp://192.168.88.17/forward");
//    inputUrl->setText("http://192.168.31.233:9000/devrecorda02c237ee64441e5b90f5e6c7ab616cf1/19700101084735036-1-3-A02C237E-E644-41E5-B90F-5E6C7AB616CF-0100811318.ts");
//    inputUrl->setText("rtsp://admin:abc123456@192.168.1.165:554/h264/ch1/main/av_stream");
//    inputUrl->setText("rtsp://192.168.1.151/audiotracker_1");
    pHLayout->addWidget(inputUrl);
    pHLayout->addWidget(openBtn);
    pHLayout->addWidget(closeBtn);
    pHLayout->addWidget(sizeBox);
    pHLayout->addWidget(fmtBox);
    pHLayout->addWidget(cardBox);
    pVLayout->addLayout(pHLayout);

    timeSlider = new QSlider(this);
    timeSlider->setOrientation(Qt::Horizontal);
    timeSlider->setRange(0,255);
    timeSlider->setValue(130);
    connect(timeSlider, &QSlider::valueChanged, this, &VideoWidget::onTimeSliderValueChanged);
    pVLayout->addWidget(timeSlider);

    connect(openBtn, &QPushButton::clicked, [this,inputUrl](){
        QString url = inputUrl->text();
        if(url.length()) {
//            decoder->stopPlay();
            decoder->startPlay(url, false);
        }
    });
    connect(closeBtn, &QPushButton::clicked, [this](){
        decoder->stopPlay();
        smtAudioPlayer->clearSourceData(mSourceId);
        videoBuffer.clear();
    });
    connect(sizeBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [this](int index){
        QSize size = sizeList[index];
        video->setFixedSize(size);
    });
    connect(fmtBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [this](int index){
        AVPixelFormat fmt = fmtList[index];
        decoder->setOutVideoFmt(fmt);
    });
    connect(cardBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [this](int index){
        mCardId = index;
    });

    smtAudioPlayer = SmtAudioPlayer::inst();
    connect(smtAudioPlayer, &SmtAudioPlayer::audioCardOpened,
            this, &VideoWidget::onAudioCardOpened);
    smtAudioPlayer->initialize();
//    SDL_AudioSpec audioSpec;
//    audioSpec.freq = 48000;
//    audioSpec.format = AUDIO_S16SYS;
//    audioSpec.channels = 2;
//    audioSpec.silence = 0;
//    audioSpec.samples = 1024*2;
//    const char* name = SDL_GetAudioDeviceName(1, 0);
//    sdlSmtAudioPlayer->openCard(name, audioSpec);

    smtAudioPlayer->setSync(mSourceId, decoder->getSynchronizer());
    mSliderTimer = new QTimer();
    connect(mSliderTimer, &QTimer::timeout, this, &VideoWidget::onSliderTimeout);
    mSliderTimer->start(1000);
}

VideoWidget::~VideoWidget()
{
    decoder->stopPlay();
    delete decoder;
}

int VideoWidget::exec()
{
    return QDialog::exec();
}

void VideoWidget::closeEvent(QCloseEvent *e)
{
    decoder->stopPlay();
    smtAudioPlayer->clearData(mCardId, mSourceId);

    done(true);
}

void VideoWidget::onShowVideoData(std::shared_ptr<MediaData> data)
{
#if 1
    if(data->pixel_format==AV_PIX_FMT_YUV420P) {
        video->repaintView(data.get());
    } else if(data->pixel_format==AV_PIX_FMT_RGB24) {
        QImage img = QImage(data->width, data->height, QImage::Format_RGB888);
        for(int y = 0; y < data->height; ++y) {
            memcpy(img.scanLine(y), data->data[0]+y*data->linesize[0],
                    data->linesize[0]);
        }
        m_rgb24_image = img;
        m_is_m_rgb24_updated = true;
        update();
    }

#else
    int y_size = mediaData->width * mediaData->height;
    int bufSize = y_size+y_size/2;
    uchar *ptr = new uchar[bufSize];
    memcpy(ptr, mediaData->data[0], y_size);
    memcpy(ptr+y_size, mediaData->data[1], y_size/4);
    memcpy(ptr+y_size+y_size/4, mediaData->data[2], y_size/4);
//        int bufSize = mediaData->datasize[0]+mediaData->datasize[1]+mediaData->datasize[2];
//        uchar *ptr = new uchar[bufSize];
//        memcpy(ptr, mediaData->data[0], mediaData->datasize[0]);
//        memcpy(ptr+mediaData->datasize[0], mediaData->data[1], mediaData->datasize[1]);
//        memcpy(ptr+mediaData->datasize[0]+mediaData->datasize[1], mediaData->data[2], mediaData->datasize[2]);

    //            fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);
    //            fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);
    //            fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);
//        video->slotShowYuv(ptr, mediaData->width, mediaData->height);

//    std::shared_ptr<const std::vector<uint8_t>> shared(new std::vector<uint8_t>(ptr, (ptr+bufSize)));
//    video->setI420Data(shared, mediaData->width, mediaData->height);

//    video->DisplayVideoFrame((unsigned char *)ptr, mediaData->width, mediaData->height);

    video->repaintView(mediaData);
//    video->slotShowYuv(ptr, mediaData->width, mediaData->height);

    mediaData->release();
    delete mediaData;
#endif
}

//接收音频数据
void VideoWidget::onAudioData(std::shared_ptr<MediaData> mediaData)
{
    if(mediaData->sample_format==AV_SAMPLE_FMT_S16) {
        smtAudioPlayer->addData(mCardId, mSourceId, mediaData.get());
        smtAudioPlayer->setSoundOpen(mSourceId, true);
    } else {
        qDebug()<<"Unsupported audio format:"<<mediaData->sample_format;
    }
}

//接收视频数据
void VideoWidget::onVideoData(std::shared_ptr<MediaData> mediaData)
{
    if(mediaData->pixel_format==AV_PIX_FMT_YUV420P) {
        videoBuffer.addData(mediaData.get());
    } else if(mediaData->pixel_format==AV_PIX_FMT_RGB24) {
        videoBuffer.addData(mediaData.get());


//      {
//          QFile file("D:\\1.jpg");
//          if(file.open(QIODevice::ReadOnly)){
//              QByteArray data = file.readAll();
//              mImage = QImage::fromData(data);
//              file.close();
//          }
//      }

//        {
//            static int aaa = 0;
//            QImage img = QImage(mediaData->width, mediaData->height, QImage::Format_RGB888);
//            for(int y = 0; y < mediaData->height; ++y) {
//                memcpy(img.scanLine(y), mediaData->data[0]+y*mediaData->linesize[0],
//                        mediaData->linesize[0]);
//            }
//            QFile file(QString("D://%1.jpg").arg(aaa++));
//            if (file.open(QFile::WriteOnly)) {
//                img.save(&file, "JPG");
//                file.close();
//            }
//        }
        //qDebug()<<"Unsupported video format:"<<mediaData->pixel_format;
    }
}

//声卡初始化完成
void VideoWidget::onAudioCardOpened(QString name, int cardId)
{
    if(cardId==mCardId) {
        smtAudioPlayer->setCardVolume(mCardId, 20);
    }
}

void VideoWidget::onMediaDecoderEvent(int event, QString msg)
{
    if(event==StreamMediaDecoder::SeekFrame) {
        smtAudioPlayer->clearSourceData(mSourceId);
        videoBuffer.clear();
    }
}

void VideoWidget::onTimeSliderValueChanged(int value)
{
    qDebug()<<"value:"<<value;
    //onHSliderValueChanged(value);//执行角点检测
    decoder->seek(value);
}

void VideoWidget::onSliderTimeout()
{
    if(timeSlider) {
//        bool pause = decoder->isPause();
        int msec = decoder->getSynchronizer()->videoPlayingMs();
        timeSlider->blockSignals(true);
        timeSlider->setValue(msec/1000);
        timeSlider->blockSignals(false);
    }
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setBrush(Qt::black);
    painter.drawRect(0, 0, this->width(), this->height());

    if (m_rgb24_image.size().width() <= 0
            || !m_is_m_rgb24_updated)
        return;
    m_is_m_rgb24_updated = false;

    //比例缩放
    QImage img = m_rgb24_image.scaled(this->size(),Qt::KeepAspectRatio);
    int x = this->width() - img.width();
    int y = this->height() - img.height();

    x /= 2;
    y /= 2;

    //QPoint(x,y)为中心绘制图像
    painter.drawImage(QPoint(x,y),img);
}
