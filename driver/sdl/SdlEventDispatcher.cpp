#include "SdlEventDispatcher.h"
#include <QDebug>

extern "C"{
    #include "SDL.h"
    #include "SDL_events.h"
}

SdlEventDispatcher::SdlEventDispatcher(QObject *parent) : QThread(parent)
{

}

SdlEventDispatcher::~SdlEventDispatcher()
{
    mIsRunning = false;
}

void SdlEventDispatcher::run()
{
    mIsRunning = true;
    while (mIsRunning) {
        iteration();
        SDL_Delay(100);
    }
}

//SDL事件
void SdlEventDispatcher::iteration()
{
    int done = 0;
    SDL_Event e;
    SDL_AudioDeviceID dev;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            done = 1;
        } else if (e.type == SDL_KEYUP) {
            if (e.key.keysym.sym == SDLK_ESCAPE)
                done = 1;
        } else if (e.type == SDL_AUDIODEVICEADDED) {
            int index = e.adevice.which;
            int iscapture = e.adevice.iscapture;
            const char *name = SDL_GetAudioDeviceName(index, iscapture);
            if (name != NULL)
                SDL_Log("New %s audio device at index %u: %s\n", devtypestr(iscapture), (unsigned int) index, name);
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Got new %s device at index %u, but failed to get the name: %s\n",
                    devtypestr(iscapture), (unsigned int) index, SDL_GetError());
                continue;
            }
            if (!iscapture) {
                eventAudioDeviceAdded(index, iscapture);
            }
        } else if (e.type == SDL_AUDIODEVICEREMOVED) {
            dev = (SDL_AudioDeviceID) e.adevice.which;
            SDL_Log("%s device %u removed.\n", devtypestr(e.adevice.iscapture), (unsigned int) dev);
            eventAudioDeviceRemoved(dev);
        }
    }
}

void SdlEventDispatcher::eventAudioDeviceAdded(int index, int iscapture)
{
    emit audioDeviceAdded(index, iscapture);
}

void SdlEventDispatcher::eventAudioDeviceRemoved(int devid)
{
    emit audioDeviceRemoved(devid);
}

const char* SdlEventDispatcher::devtypestr(int iscapture)
{
    return iscapture ? "capture" : "output";
}
