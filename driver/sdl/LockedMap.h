#ifndef LOCKEDMAP_H
#define LOCKEDMAP_H

#include <map>
#include <list>
#include <mutex>

typedef std::lock_guard<std::recursive_mutex> LockedMapLocker;

template <class Key, class T>
class LockedMap : public std::map<Key,T>
{
public:
    std::recursive_mutex& mutex() {
        return mMutex;
    }

    void lock() {
        mMutex.lock();
    }

    void unlock() {
        mMutex.unlock();
    }

    bool contains(Key k) {
        LockedMapLocker lk(mMutex);
        return find(k)!=end();
    }

    void insert(Key k, T v) {
        LockedMapLocker lk(mMutex);
        map<Key, T>::iterator iter = find(k);
        if(iter!=end()) {
            iter->second = v;
        } else {
            std::map<Key, T>::insert(std::pair<Key, T>(k, v));
        }
    }

    void remove(Key k) {
        LockedMapLocker lk(mMutex);
        erase(k);
    }

    std::list<Key> keys() {
        LockedMapLocker lk(mMutex);
        std::list<Key> ks;
        for(std::map<Key, T>::iterator it=begin();
                 it!=end(); ++it) {
            ks.push_back(it->first);
        }
        return ks;
    }

    inline T &operator[](const Key &akey) {
        T empty = T();
        LockedMapLocker lk(mMutex);
        std::map<Key, T>::iterator it = find(akey);
        if(it!=end())
            return (it->second);
        return empty;
    }

private:
    std::recursive_mutex mMutex;
};

#endif // LOCKEDMAP_H
