//
// Created by StuUngar on 2021/10/24.
//

#ifndef STUPLAYER_STUPLAYER_H
#define STUPLAYER_STUPLAYER_H

#include <cstring>
#include <pthread.h>
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "JNICallbackHelper.h"
#include "util.h"

extern "C"{//ffmpeg是纯C的库，必须采用C的编译方式，所以需要加extern "C"
#include <libavformat/avformat.h>
#include <libavutil//time.h>
};

class StuPlayer {
private:
    char * data_source;
    pthread_t pid_prepare;
    pthread_t pid_start;
    AVFormatContext * formatContext = 0;
    AudioChannel *audio_channel = 0;
    VideoChannel *video_channel = 0;
    JNICallbackHelper *helper = 0;
    bool isPlaying; //是否播放
    RenderCallback  renderCallback;
    int  duration;

    pthread_mutex_t  seek_mutex;
    pthread_t pid_stop;

public:
    StuPlayer(const char *data_path, JNICallbackHelper *pHelper);

    virtual ~StuPlayer();

    void prepare();

    void prepare_();

    void start();

    void start_();

    void setRenderCallback(RenderCallback renderCallback);

    int getDuration();

    void seek(int play_value);

    void stop();

    void stop_(StuPlayer *derryPlayer);
};


#endif //STUPLAYER_STUPLAYER_H
