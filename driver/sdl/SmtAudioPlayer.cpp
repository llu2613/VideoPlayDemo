#include "SmtAudioPlayer.h"
#include <QDebug>

//单例
SmtAudioPlayer* SmtAudioPlayer::m_instance = nullptr;

SmtAudioPlayer* SmtAudioPlayer::inst()
{
    if(!m_instance) {
        m_instance = new SmtAudioPlayer();
    }
    return m_instance;
}

SmtAudioPlayer::SmtAudioPlayer(QObject *parent)
    : QObject(parent)
{
    mTextCodec = QTextCodec::codecForName("UTF-8");

    SDL_zero(mAudioSpec);
    mAudioSpec.freq = 48000;
    mAudioSpec.format = AUDIO_S16SYS;
    mAudioSpec.channels = 2;
    mAudioSpec.silence = 0;
    mAudioSpec.samples = 1024*2;

    connect(&mSdlEventDispatcher, &SdlEventDispatcher::audioDeviceAdded,
            this, &SmtAudioPlayer::eventAudioDeviceAdded);
    connect(&mSdlEventDispatcher, &SdlEventDispatcher::audioDeviceRemoved,
            this, &SmtAudioPlayer::eventAudioDeviceRemoved);
    mSdlEventDispatcher.start();
}

SmtAudioPlayer::~SmtAudioPlayer()
{

}

void SmtAudioPlayer::setSync(int sourceId, AVSynchronizer *sync) {
    if(!sync)
        return;

    mSyncMap.lock();
    if(mSyncMap.contains(sourceId))
        qDebug()<<"AVSynchronizer had been set. NOT update!";
    else
        mSyncMap.insert(sourceId, sync);
    mSyncMap.unlock();
}

void SmtAudioPlayer::removeSync(int sourceId)
{
    mSyncMap.lock();
    if(mSyncMap.contains(sourceId))
        mSyncMap.remove(sourceId);
    mSyncMap.unlock();
}

void SmtAudioPlayer::addData(int cardId, int sourceId, MediaData *data)
{
    bool isSoundOpen = false;
    mSourceInfoMap.lock();
    if(mSourceInfoMap.contains(sourceId)) {
        isSoundOpen = mSourceInfoMap[sourceId].isOpenSound;
    }
    mSourceInfoMap.unlock();

    unsigned char *buf = data->data[0];
    int len = data->datasize[0];
    double audio_pts_second = data->pts*data->time_base_d;
    if(isSoundOpen) {
        SdlAudioPlayer::addData(cardId, sourceId, (Uint8*)buf, len, audio_pts_second);
    }

    //计算波形图
    std::shared_ptr<SampleBuffer> b(new SampleBuffer);
    b->copy(buf, len);
    b->channels = audioChannels();
    mWaveformProcessor.addSample(sourceId, b);
}

void SmtAudioPlayer::setSourceOpen(int sourceId, bool isOpen)
{
    LockedMapLocker lk(mSourceInfoMap.mutex());

    if(mSourceInfoMap.contains(sourceId)) {
        AudioSource &source = mSourceInfoMap[sourceId];
        source.isOpenSound = isOpen;
    } else {
        AudioSource source;
        source.isOpenSound = isOpen;
        mSourceInfoMap.insert(sourceId, source);
    }
}

QList<int> SmtAudioPlayer::getWaveformData(int sourceId)
{
    return mWaveformProcessor.getShowData(sourceId);
}

void SmtAudioPlayer::clearWaveformData(int sourceId)
{
    mWaveformProcessor.clear(sourceId);
}

int SmtAudioPlayer::audioSamples()
{
    return mAudioSpec.freq;
}

int SmtAudioPlayer::audioChannels()
{
    return mAudioSpec.channels;
}

QStringList SmtAudioPlayer::devices()
{
    std::list<std::string> devs = SdlAudioPlayer::devices();
    QStringList list;
    for(std::list<std::string>::iterator it=devs.begin();
        it!=devs.end(); it++) {
        list.append(QString::fromUtf8(it->c_str()));
    }
    return list;
}

void SmtAudioPlayer::clearSourceData(int sourceId)
{
    SdlAudioPlayer::clearSourceData(sourceId);
}

void SmtAudioPlayer::clearData(int cardId, int sourceId)
{
    SdlAudioPlayer::clearData(cardId, sourceId);
}

void SmtAudioPlayer::eventAudioDeviceAdded(QString name)
{
    qDebug()<<"eventAddCard, name:"<<name;

    int cardId = openCard(name.toStdString(), mAudioSpec);
    if(cardId!=-1) {
        emit audioCardOpened(name, cardId);
        qDebug()<<"eventAddCard, opened:"<<name<<cardId;
    }
}

void SmtAudioPlayer::eventAudioDeviceRemoved(uint32_t devid)
{
    qDebug()<<"eventMoveCard, ID:"<<devid;

    int cardId = getCardIdByDevid(devid);
    QString name = QString::fromUtf8(getCardName(cardId).c_str());
    if(cardId!=-1) {
        closeCard(cardId);
        emit audioCardClosed(name, cardId);
    }
}

//复写函数
void SmtAudioPlayer::onCardDataPrepared(int cardId, std::map<int, std::shared_ptr<SampleBuffer>> &dataMap)
{
    //同步
    mSyncMap.lock();
    for(std::map<int, std::shared_ptr<SampleBuffer>>::iterator it=dataMap.begin();
             it!=dataMap.end(); ++it) {
        int idx = it->first;
        if(mSyncMap.contains(idx)&&mSyncMap.at(idx)) {
            mSyncMap.at(idx)->setPlayingSample(dataMap.at(idx));
        }
    }
    mSyncMap.unlock();
}
