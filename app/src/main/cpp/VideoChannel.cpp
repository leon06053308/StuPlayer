//
// Created by StuUngar on 2021/10/24.
//

#include "VideoChannel.h"

//丢包操作， 原始包，不需要考虑关键帧
void dropAVFrame(queue<AVFrame *> &q){
    if (!q.empty()){
        AVFrame *frame = q.front();
        BaseChannel::releaseAVFrame(&frame);
        q.pop();
    }
}

//压缩包，要考虑关键帧
void dropAVPacket(queue<AVPacket *> &q){
    while (!q.empty()){
        AVPacket *pkt = q.front();

        if (pkt->flags != AV_PKT_FLAG_KEY){
            BaseChannel::releaseAVPacket(&pkt);
            q.pop();
        } else{
            break;
        }
    }
}


VideoChannel::VideoChannel(int stream_index, AVCodecContext *codecContext, AVRational time_base, int fps)
        :BaseChannel(stream_index, codecContext, time_base),
        fps(fps){
    frames.setSyncCallback(dropAVFrame);
    packets.setSyncCallback(dropAVPacket);
}

VideoChannel::~VideoChannel() {
    // TODO 播放器收尾 3
    DELETE(audio_channel);
}

void VideoChannel::stop() {
    // TODO 播放器收尾 3

    pthread_join(pid_video_decode, nullptr);
    pthread_join(pid_video_play, nullptr);

    isPlaying = false;
    packets.setWork(0);
    frames.setWork(0);

    packets.clear();
    frames.clear();
}

void *task_video_decode(void *args){
    LOGD("task_video_decode...");
    auto *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_decode();
    return nullptr;
}

void *task_video_play(void *args){
    LOGD("VideoChannel, task_video_play...");
    auto *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_play();
    return nullptr;
}

void VideoChannel::start() {
    LOGD("VideoChannel start...");
    isPlaying = true;

    //队列开始工作
    packets.setWork(1);
    frames.setWork(1);

    //第一个线程,取出队列的压缩包进行解码，解码后的原始包push到队列中
    pthread_create(&pid_video_decode, nullptr, task_video_decode, this);

    //第二个线程, 从队列取出原始包，播放
    pthread_create(&pid_video_play, nullptr, task_video_play, this);
}

void VideoChannel::video_decode() {
    LOGD("VideoChannel,  video_decode...");
    AVPacket *pkt = nullptr;
    while (isPlaying){

        if (isPlaying && frames.size() > 100){
            av_usleep(10 * 1000);
            continue;
        }

        LOGD("VideoChannel,  before getQueueAndDel...");
        int ret = packets.getQueueAndDel(pkt); //可能阻塞
        LOGD("VideoChannel,  after getQueueAndDel...");

        if (!isPlaying){
            break;
        }

        if (!ret){
            continue; //生产过慢
        }

        //1.发送pkt到缓冲区
        ret = avcodec_send_packet(codecContext, pkt);

        //FFMPeg内部会缓存一份
        //releaseAVPacket(&pkt);


        if(ret){
            break;
        }

        //2.从缓冲区拿出frame包
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)){
            //可能是B帧, 需要参考前后P帧，而此时前或后P帧并未解析出来
            continue;
        } else if (ret !=0){
            LOGE("xx----avcodec_receive_frame error: %d", ret);
            if(frame){
                releaseAVFrame(&frame);
            }
            break;
        }
        //拿到frame原始包，加入队列
        frames.insertToQueue(frame);

        //释放pkt本身空间和pkt成员指向空间释放
        av_packet_unref(pkt); //内部成员引用减一
        releaseAVPacket(&pkt);
    }//end while

    av_packet_unref(pkt); //内部成员引用减一
    releaseAVPacket(&pkt);
}

void VideoChannel::video_play() {
    LOGD("VideoChannel , video_play...");

    AVFrame *frame = 0;
    uint8_t *dst_data[4]; //R, G, B, A
    int dst_linesize[4]; //R, G, B, A

    //dst_data申请空间
    av_image_alloc(dst_data, dst_linesize,
                   codecContext->width, codecContext->height, AV_PIX_FMT_RGBA, 1);

    //图像格式转换。原始包是YUV格式，Android屏幕是RGBA【libswscale】
    SwsContext *sws_ctx = sws_getContext(
            //输入
            codecContext->width,
            codecContext->height,
            codecContext->pix_fmt, //自动获取mp4格式，也可以使用AV_PIX_FMT_YUV420P

            //输出
            codecContext->width,
            codecContext->height,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR, NULL, NULL, NULL);

    while (isPlaying){
        int ret = frames.getQueueAndDel(frame);

        if (!isPlaying){
            LOGW("VideoChannel, isPlaying false break...");
            break;
        }

        if (!ret){
            LOGW("VideoChannel, frames.getQueueAndDel fail, continue...");
            continue;
        }


        sws_scale(sws_ctx,
                  //输入
                  frame->data, frame->linesize,
                  0,codecContext->height,
                  //输出，RGBA
                  dst_data,
                  dst_linesize
                  );

        //TODO 音视频同步 加入FPS间隔时间
        //extra_delay = repeat_pict / (2*fps) : 0.040000左右
        double extra_delay = frame->repeat_pict / (2 * fps); //可能获取不到
        double fps_delay = 1.0 /fps;
        double real_delay = extra_delay + fps_delay;//当前帧延迟时间

        //不能使用如下方式，以为和音频没有任何关联
        //av_usleep(real_delay * 1000000);

        //正确音视频同步方式如下:
        double video_time = frame->best_effort_timestamp * av_q2d(time_base);
        double  audio_time = audio_channel->audio_time;

        //判断时间差值
        double time_diff = video_time - audio_time;
        if (time_diff > 0){
            //视频时间 > 音频时间， 控制视频播放速度慢点等音频
            if (time_diff > 1){
                //TODO 差距过大 比如拖动进度条
                av_usleep(real_delay * 2 * 1000000);
            } else{
                av_usleep((real_delay + time_diff) * 1000000);
            }
        }
        if (time_diff < 0){
            //视频时间 < 音频时间
            //丢帧: 不能随意丢，不能丢I帧

            //经验值0.05
            if (fabs(time_diff) <= 0.05){
                frames.sync();
                continue;
            }
        }

        //TODO 渲染 ANativeWindows
        LOGD("VideoChannel, play loop....");
        renderCallback(dst_data[0], codecContext->width, codecContext->height, dst_linesize[0]);

        av_frame_unref(frame);
        releaseAVFrame(&frame);
    }

    av_frame_unref(frame);
    releaseAVFrame(&frame);
    isPlaying = 0;
    av_free(&dst_data[0]);
    sws_freeContext(sws_ctx);
}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

void VideoChannel::setAudioChannel(AudioChannel *audio_channel) {
    this->audio_channel = audio_channel;
}


