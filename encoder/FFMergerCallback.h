#ifndef FFMERGERCALLBACK_H
#define FFMERGERCALLBACK_H

#include <string>

class FFMergerCallback
{
public:
    virtual void onMergerProgress(long current, long total) =0;
    virtual void onMergerError(int code, std::string msg) =0;
};

#endif // FFMERGERCALLBACK_H
