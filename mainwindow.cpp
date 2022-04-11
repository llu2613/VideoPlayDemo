#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdlib.h>
#include <stdio.h>
#include <QDebug>
#include "webstreamdialog.h"

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

    openGlGroupButton = new QButtonGroup(this);
    openGlGroupButton->addButton(ui->radioBtn_softwareOpenGL,0);
    openGlGroupButton->addButton(ui->radioBtn_desktopOpenGL,1);
    openGlGroupButton->addButton(ui->radioBtn_openGLES,2);
//    ui->radioBtn_softwareOpenGL->setChecked(true);
    connect(ui->radioBtn_softwareOpenGL, &QRadioButton::clicked,
            this, &MainWindow::onOpenGLButtonGroupClicked);
    connect(ui->radioBtn_desktopOpenGL, &QRadioButton::clicked,
            this, &MainWindow::onOpenGLButtonGroupClicked);
    connect(ui->radioBtn_openGLES, &QRadioButton::clicked,
            this, &MainWindow::onOpenGLButtonGroupClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onOpenGLButtonGroupClicked(bool clicked)
{
    switch (openGlGroupButton->checkedId()) {
    case 0: //radioBtn_softwareOpenGL
        QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
        QGuiApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        qDebug()<<"AA_UseSoftwareOpenGL";
        break;
    case 1: //radioBtn_desktopOpenGL
        QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        QGuiApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        qDebug()<<"AA_UseDesktopOpenGL";
        break;
    case 2: //radioBtn_openGLES
        QApplication::setAttribute(Qt::AA_UseOpenGLES);
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        qDebug()<<"AA_UseOpenGLES";
        break;
    default:
        break;
    }
}

void MainWindow::on_pushButton_web_stream_clicked()
{
    WebStreamDialog dialog;
    dialog.exec();

}

void MainWindow::on_pushButton_2_clicked()
{
    WebStreamDialog dialog;
    dialog.exec();
}
