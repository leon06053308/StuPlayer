//
// Created by StuUngar on 2021/10/24.
//

#ifndef STUPLAYER_AUDIOCHANNEL_H
#define STUPLAYER_AUDIOCHANNEL_H


#include "BaseChannel.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "androidlog.h"
#include "JNICallbackHelper.h"

extern "C"{
    #include <libswresample/swresample.h> //对音频数据进行转换，重采样
};

class AudioChannel : public BaseChannel{
private:
    pthread_t pid_audio_decode;
    pthread_t pid_audio_play;

public:
    int out_channels;
    int out_sample_size;
    int out_sample_rate;
    int out_buffers_size;
    uint8_t *out_buffers = 0;
    SwrContext *swr_ctx = 0;

public:
    SLObjectItf engineObject = 0; //引擎对象
    SLEngineItf engineInterface = 0; //引擎接口
    SLObjectItf outputMixObject = 0; //混音器
    SLObjectItf bqPlayerObject = 0; //播放器
    SLPlayItf  bqPlayerPlay = 0; //播放器接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = 0;//播放器队列接口

    double audio_time; //音视频同步

public:
    AudioChannel(int stream_index, AVCodecContext *codecContext, AVRational time_base);

    virtual ~AudioChannel();

    void stop();

    void start();

    void audio_decode();

    void audio_play();

    int getPCM();

};


#endif //STUPLAYER_AUDIOCHANNEL_H
