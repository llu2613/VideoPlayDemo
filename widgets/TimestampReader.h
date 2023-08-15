#ifndef TIMESTAMPREADER_H
#define TIMESTAMPREADER_H

#include <QThread>

class TimestampReader : public QThread
{
    Q_OBJECT
public:
    explicit TimestampReader(QString url, QObject *parent = nullptr);

private:
    void run() override;
    int readtimestamp();
    bool mIsRunning;
    QString mUrl;

signals:
    void sigStreamTimestamp(int index, long pts);
};

#endif // TIMESTAMPREADER_H
