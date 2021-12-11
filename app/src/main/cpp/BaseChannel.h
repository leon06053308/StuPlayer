//
// Created by StuUngar on 2021/10/28.
//

#ifndef STUPLAYER_BASECHANNEL_H
#define STUPLAYER_BASECHANNEL_H

extern "C" {
    #include <libavcodec/avcodec.h>
#include <libavutil//time.h>
};

#include "safe_queue.h"
#include "JNICallbackHelper.h"

class BaseChannel {
public:
    int stream_index; //音频或者视频下标
    SafeQueue<AVPacket *> packets; //压缩数据包
    SafeQueue<AVFrame *> frames; //原始数据包
    bool isPlaying;
    AVCodecContext *codecContext = 0;

    AVRational  time_base; //音视频同步

    JNICallbackHelper *jniCallbackHelper = 0;

    void setJNICallbackHelper(JNICallbackHelper *helper){
        this->jniCallbackHelper = helper;
    }


    BaseChannel(int stream_index, AVCodecContext *codecContext, AVRational time_base)
    :stream_index(stream_index),
    codecContext(codecContext),
    time_base(time_base)
    {
        packets.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }

    ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    /**
     * 释放队列中的AVPacket *
     * @param p
     */
    static void releaseAVPacket(AVPacket **p){
        if (p){
            av_packet_free(p);
            *p = 0;
        }
    }

    /**
     * 释放队列中的所有AVFrame*
     * @param f
     */
    static void releaseAVFrame(AVFrame **f){
        if(f){
            av_frame_free(f);
            *f = 0;
        }
    }
};

#endif //STUPLAYER_BASECHANNEL_H
