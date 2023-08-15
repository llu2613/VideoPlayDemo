#ifndef TIMESTAMPDIALOG_H
#define TIMESTAMPDIALOG_H

#include <QDialog>
#include "TimestampReader.h"

namespace Ui {
class TimestampDialog;
}

class TimestampDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimestampDialog(QWidget *parent = nullptr);
    ~TimestampDialog();

private slots:
    void on_pushButton_quit_clicked();

    void on_pushButton_close_clicked();

    void on_pushButton_open_clicked();

private:


private:
    Ui::TimestampDialog *ui;
    TimestampReader *mTimestampReader;
    long mIndexsPts[32];

private slots:
    void onStreamTimestamp(int index, long pts);

};

#endif // TIMESTAMPDIALOG_H
