//
// Created by StuUngar on 2021/10/24.
//

#ifndef STUPLAYER_VIDEOCHANNEL_H
#define STUPLAYER_VIDEOCHANNEL_H

#include "BaseChannel.h"
#include "androidlog.h"
#include "AudioChannel.h"

extern "C" {
#include <libswscale//swscale.h>
#include <libavutil/imgutils.h>
};

typedef void (*RenderCallback)(uint8_t *, int , int , int );

class VideoChannel : public BaseChannel{
private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback  renderCallback;

    int fps;
    AudioChannel *audio_channel = 0;

public:
    VideoChannel(int stream_index, AVCodecContext *codecContext, AVRational time_base, int fps);

    ~VideoChannel();

    void stop();

    void start();

    void video_decode();

    void video_play();

    void setRenderCallback(RenderCallback renderCallback);

    void setAudioChannel(AudioChannel *audio_channel);
};


#endif //STUPLAYER_VIDEOCHANNEL_H
