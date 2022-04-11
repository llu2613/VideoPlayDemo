#ifndef WEBSTREAMDIALOG_H
#define WEBSTREAMDIALOG_H

#include <QDialog>

namespace Ui {
class WebStreamDialog;
}

class WebStreamDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebStreamDialog(QWidget *parent = nullptr);
    ~WebStreamDialog();

private:
    Ui::WebStreamDialog *ui;

    void pullStream(QString url);
};

#endif // WEBSTREAMDIALOG_H
