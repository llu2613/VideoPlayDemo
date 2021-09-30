#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdlib.h>
#include <stdio.h>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //开始
    connect(ui->pushButton, &QPushButton::clicked, [this](){
        VideoWidget *video = new VideoWidget(this);
        connect(video, &QDialog::finished, [this,video](){
            qDebug()<<"remove one video "<<(videoList.removeOne(video)?"true":"false");
        });
        video->setId(videoList.size());
        videoList.append(video);
        video->show();

//        decoder->startDecoding();
//        videoWidget->exec();
//        decoder->stopDecoding();

    });
    //停止
    connect(ui->pushButton_2, &QPushButton::clicked, [this](){
//        decoder->stopDecoding();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
