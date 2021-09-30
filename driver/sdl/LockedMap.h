#ifndef LOCKEDMAP_H
#define LOCKEDMAP_H

#include <QMap>
#include <QMutex>

template <class Key, class T>
class LockedMap : public QMap<Key,T>
{
public:
    QMutex& mutex() {
        return mMutex;
    }

    void lock() {
        mMutex.lock();
    }

    void unlock() {
        mMutex.unlock();
    }

private:
    QMutex mMutex;
};

#endif // LOCKEDMAP_H
