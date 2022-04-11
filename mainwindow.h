#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QButtonGroup>
#include "driver/VideoWidget.h"
#include "decoder/FFmpegMediaDecoder.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QButtonGroup *openGlGroupButton;
    QList<VideoWidget*> videoList;

private slots:
    void onOpenGLButtonGroupClicked(bool clicked);

    void on_pushButton_web_stream_clicked();
    void on_pushButton_2_clicked();
};

#endif // MAINWINDOW_H
