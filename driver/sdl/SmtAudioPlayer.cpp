#include "SmtAudioPlayer.h"
#include <QDebug>

#define MAX_BUF_MEM_SIZE (10*1024*1024)

//单例
SmtAudioPlayer* SmtAudioPlayer::m_instance = nullptr;

SmtAudioPlayer* SmtAudioPlayer::inst()
{
    if(!m_instance) {
        m_instance = new SmtAudioPlayer();
    }
    return m_instance;
}

static void SdlLogOutputFunction(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    const static char prio[][32] = {"NONE", "VERBOSE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"};
    const static char cate[][32] = {"APPLICATION", "ERROR", "ASSERT", "SYSTEM", "AUDIO", "VIDEO", "RENDER", "INPUT", "TEST"};

    SmtAudioPlayer *pThis = userdata? (SmtAudioPlayer*)userdata: NULL;
    if(pThis) {
        char cate_s[32], prio_s[32];
        if(category>=0 && category<(sizeof(cate)/32))
            strcpy(cate_s, cate[category]);
        else
            sprintf(cate_s, "%d", category);

        if(priority>=0 && priority<(sizeof(prio)/32))
            strcpy(prio_s, prio[priority]);
        else
            sprintf(prio_s, "%d", priority);

        qDebug()<<QString("SdlLog: %1 %2 %3").arg(cate_s).arg(prio_s).arg(message);
    }
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
}

SmtAudioPlayer::~SmtAudioPlayer()
{
    for(int i=0; i<MAX_CARD_NUM; i++) {
        closeCard(i);
    }
}

void SmtAudioPlayer::initialize()
{
    SDL_LogSetOutputFunction(SdlLogOutputFunction, this);
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

    if(!mSdlEventDispatcher.isRunning()) {
        qDebug()<<"AudioSpec:"<<"freq"<<mAudioSpec.freq
                   <<"channels"<<mAudioSpec.channels<<"silence"<<mAudioSpec.silence
                  <<"samples"<<mAudioSpec.samples<<"format"<<mAudioSpec.format;
        mSdlEventDispatcher.start();
    }
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
    unsigned long audio_pts = data->pts;
    if(isSoundOpen) {
        SdlAudioPlayer::addData(cardId, sourceId, (Uint8*)buf, len, audio_pts);
    }

    //计算波形图
    std::shared_ptr<SampleBuffer> b(new SampleBuffer);
    b->copy(buf, len);
    b->channels = audioChannels();
    mWaveformProcessor.addSample(sourceId, b);
}

void SmtAudioPlayer::setSoundOpen(int sourceId, bool isOpen)
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

int SmtAudioPlayer::cardVolume(int cardId)
{
    int val = SdlAudioPlayer::volume(cardId);
    return (int)(val*100.0f/MAX_CARD_VOLUME);
}

void SmtAudioPlayer::setCardVolume(int cardId, int volume)
{
    int val = (int)(volume*1.0f*MAX_CARD_VOLUME/100);
    SdlAudioPlayer::setVolume(cardId, val);
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

void SmtAudioPlayer::eventAudioDeviceAdded(int index, int iscapture)
{
    qDebug()<<"eventAddCard, index:"<<index<<"iscapture:"<<iscapture;

    const char *name = SDL_GetAudioDeviceName(index, iscapture);
    if(name==NULL) {
        qDebug()<<"eventAddCard, device name is null!";
    }

    int oldCardId = getCardId(name);
    if(oldCardId!=-1) {
        print_card_info(oldCardId);
        closeCard(oldCardId);
    }

    int cardId = openCard(name, mAudioSpec, MAX_BUF_MEM_SIZE);
    if(cardId!=-1) {
        emit audioCardOpened(name, cardId);
        qDebug()<<"eventAddCard, opened:"<<name<<cardId;
    }
}

void SmtAudioPlayer::eventAudioDeviceRemoved(int devid)
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
            std::shared_ptr<SampleBuffer> b = dataMap.at(idx);
            mSyncMap.at(idx)->setAudioPlayingTs(b->timestamp);
        }
    }
    mSyncMap.unlock();
}
