#ifndef FFMERGERCALLBACK_H
#define FFMERGERCALLBACK_H

class FFMergerCallback
{
public:
    virtual void onMergerProgress(long current, long total) =0;
    virtual void onMergerError(int code, const char* msg) =0;
};

#endif // FFMERGERCALLBACK_H
