#include "timestampdialog.h"
#include "ui_timestampdialog.h"



TimestampDialog::TimestampDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TimestampDialog)
{
    ui->setupUi(this);

    mTimestampReader = NULL;
}

TimestampDialog::~TimestampDialog()
{
    delete ui;
}

void TimestampDialog::onStreamTimestamp(int index, long pts)
{
    const int max = sizeof(mIndexsPts)/sizeof(mIndexsPts[0]);
    if(index>=0 && index<max) {
        mIndexsPts[index] = pts;
    }
    QString text ="";
    for (int i=0;i<max;i++) {
        text.append(QString::number(mIndexsPts[i])+"\n");
    }
    ui->label->setText(text);
}

void TimestampDialog::on_pushButton_quit_clicked()
{

}

void TimestampDialog::on_pushButton_close_clicked()
{
    if(mTimestampReader) {
        mTimestampReader->requestInterruption();
        mTimestampReader->wait();
        mTimestampReader->deleteLater();
        mTimestampReader = NULL;
    }
}

void TimestampDialog::on_pushButton_open_clicked()
{
//    QString url = "rtsp://admin:abc123456@192.168.1.108/Streaming/Channels/101";
    QString url = "rtsp://127.0.0.1:554/10ef40fb3887fa80f3cb931fa6cec475/mux35_ef0e03d0_0";

    if(!mTimestampReader) {
        memset(mIndexsPts, 0, sizeof(mIndexsPts));
        mTimestampReader = new TimestampReader(url, this);
        connect(mTimestampReader, &TimestampReader::sigStreamTimestamp, this, &TimestampDialog::onStreamTimestamp);
        mTimestampReader->start();
    }
}
