#include "webstreamdialog.h"
#include "ui_webstreamdialog.h"

WebStreamDialog::WebStreamDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WebStreamDialog)
{
    ui->setupUi(this);
    ui->lineEdit_input_url->setText(
                "rtsp://admin:abc123456@192.168.1.104:554/Streaming/Channels/101?transportmode=unicast");

    QString url = ui->lineEdit_input_url->text();
    if(url.size()) {
        pullStream(url);
    }
}

WebStreamDialog::~WebStreamDialog()
{
    delete ui;
}

void WebStreamDialog::pullStream(QString url)
{


}
