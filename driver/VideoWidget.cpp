#include "VideoWidget.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>

#define VIDEO_W 1280
#define VIDEO_H 720

#define CardId 0

VideoWidget::VideoWidget(QWidget *parent)
    : QDialog(parent), videoBuffer("VideoWidget")
{
    setFixedSize(VIDEO_W+80, VIDEO_H+150);

    decoder = new StreamMediaDecoder();
    decoder->setOutVideo2(VIDEO_W, VIDEO_H);

//    QObject::connect(decoder, &FFmpegMediaDecoder::audioData,
//                     &avSyncTimer, &AVSyncTimer::onAudioData);
//    QObject::connect(decoder, &FFmpegMediaDecoder::videoData,
//                     &avSyncTimer, &AVSyncTimer::onVideoData);
//    avSyncTimer.start();

//    QObject::connect(&avSyncTimer, &AVSyncTimer::audioData,
//                     this, &VideoWidget::onAudioData);
//    QObject::connect(&avSyncTimer, &AVSyncTimer::videoData,
//                     this, &VideoWidget::onShowVideoData);

    QObject::connect(decoder, &StreamMediaDecoder::audioData,
                     this, &VideoWidget::onAudioData);
    QObject::connect(decoder, &StreamMediaDecoder::videoData,
                     this, &VideoWidget::onVideoData);

    connect(&videoBuffer, &VideoBuffer::showFrame,
            this, &VideoWidget::onShowVideoData);
    videoBuffer.start();

    sizeList<<QSize(1920,1080)<<QSize(1280,720)<<QSize(640,480)
           <<QSize(832,480)<<QSize(416,240)
          <<QSize(352,288)<<QSize(175,144);

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

    for(int i=0; i<sizeList.size(); i++){
        sizeBox->addItem(QString("%1x%2").arg(sizeList[i].width())
                     .arg(sizeList[i].height()));
    }

    inputUrl->setText("http://220.161.87.62:8800/hls/0/index.m3u8");
//    inputUrl->setText("rtsp://admin:abc123456@192.168.1.165:554/h264/ch1/main/av_stream");
//    inputUrl->setText("rtsp://192.168.31.182/audiotracker_1");
    pHLayout->addWidget(inputUrl);
    pHLayout->addWidget(openBtn);
    pHLayout->addWidget(closeBtn);
    pHLayout->addWidget(sizeBox);
    pVLayout->addLayout(pHLayout);

    connect(openBtn, &QPushButton::clicked, [this,inputUrl](){
        QString url = inputUrl->text();
        if(url.length()) {
//            decoder->stopPlay();
            decoder->startPlay(url, true);
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

    smtAudioPlayer = SmtAudioPlayer::inst();
//    SDL_AudioSpec audioSpec;
//    audioSpec.freq = 48000;
//    audioSpec.format = AUDIO_S16SYS;
//    audioSpec.channels = 2;
//    audioSpec.silence = 0;
//    audioSpec.samples = 1024*2;
//    const char* name = SDL_GetAudioDeviceName(1, 0);
//    sdlSmtAudioPlayer->openCard(name, audioSpec);

    mSourceId = 0;
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
    smtAudioPlayer->clearData(CardId, mSourceId);

    done(true);
}

void VideoWidget::onShowVideoData(std::shared_ptr<MediaData> data)
{
#if 1
    video->repaintView(data.get());
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
        smtAudioPlayer->addData(CardId, mSourceId, mediaData.get());
        smtAudioPlayer->setSourceOpen(mSourceId, true);
    } else {
        qDebug()<<"Unsupported audio format:"<<mediaData->sample_format;
    }
}

//接收视频数据
void VideoWidget::onVideoData(std::shared_ptr<MediaData> mediaData)
{
    if(mediaData->pixel_format==AV_PIX_FMT_YUV420P) {
        videoBuffer.addData(mediaData.get());
    } else {
        qDebug()<<"Unsupported video format:"<<mediaData->pixel_format;
    }
}

