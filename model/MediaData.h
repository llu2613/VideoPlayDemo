#ifndef MEDIADATA_H
#define MEDIADATA_H

#include <QObject>
#include <QMetaType>

#define MAX_DATA_ARRAY 8

class MediaData : public QObject
{
    Q_OBJECT
public:
    enum Format{
        Unknown,
        AudioU16,
        AudioS16,
        AudioS32,
        AudioF32,
        VideoYUV,
        VideoRGB
    };

    explicit MediaData(QObject *parent = nullptr);
    ~MediaData();

    void release();

    void copy(const MediaData &src);

    enum Format format;
    unsigned char *data[MAX_DATA_ARRAY];
    int datasize[MAX_DATA_ARRAY];
    int linesize[MAX_DATA_ARRAY];

    int64_t pts;
    int64_t duration;
    int64_t timestamp;
    double time_base_d;
    //audio
    int channels;
    int samples;
    int sample_rate;
    //video
    int width;
    int height;
    int repeat_pict;
    int frame_rate;

};

//Q_DECLARE_METATYPE(MediaData)

#endif // MEDIADATA_H
