//
// Created by StuUngar on 2021/10/24.
//

#include "StuPlayer.h"
#include "androidlog.h"

StuPlayer::StuPlayer(const char *data_path, JNICallbackHelper *pHelper) {
    //深拷贝，+1的原因是C++字符串是以'\0'结尾
    this->data_source = new char[strlen(data_path) + 1];
    strcpy(this->data_source, data_path);

    this->helper = pHelper;

    pthread_mutex_init(&seek_mutex, nullptr);
}

StuPlayer::~StuPlayer() {
    if (data_source){
        delete data_source;
        data_source = nullptr;
    }

    if (helper){
        delete helper;
        helper = nullptr;
    }

    pthread_mutex_destroy(&seek_mutex);
}

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> prepare

void* task_prepare(void * args) { //此函数和StuPlayer对象没有关系，你无法那该对象的私有成员
    auto *player = static_cast<StuPlayer *>(args);

    player->prepare_();
    //注意，必须返回
    return 0;
}

void StuPlayer::prepare_() { //在子线程中执行

    //为什么FFMPEG源码，大量使用上下文：因为FFMPEG是纯C的，它不像C++，java，上下文的出现是为了贯彻环境，就相当于java的this能操作成员

    /**
     * 第一步，打开媒体文件
     */
    formatContext = avformat_alloc_context();
    AVDictionary *options = nullptr;
    av_dict_set(&options, "timeout", "5000000", 0);

    /**
     * 1.AVFormatContext *
     * 2.路径
     * 3.AVInputFormat *fmt MAC,Windows摄像头，麦克风才有用
     * 4.AVDictionary **options，一些设置选项，例如Http连接超时,打开rtmp的超时
     */
    int r = avformat_open_input(&formatContext, data_source, nullptr, &options);

    av_dict_free(&options);
    if (r){
        //TODO: 错误信息回调到java层
        if (helper){
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);

            //char * errorInfo = av_err2str(r);//根据返回码得到错误详情
        }
        avformat_close_input(&formatContext);
        return;
    }

    /**
     * 第二步，查找媒体中的音视频流信息
     */
    r = avformat_find_stream_info(formatContext, nullptr);
    if (r < 0){
        //TODO
        if (helper){
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAM);
        }

        avformat_close_input(&formatContext);
        return;
    }

    this->duration = formatContext->duration / AV_TIME_BASE;

    AVCodecContext *codecContext = nullptr;
    /**
     * 第三步: 根据流信息，循环流个数
     */
    for (int stream_index = 0; stream_index < formatContext->nb_streams; ++stream_index) {
        /**
         * 第四步: 获取媒体流(视频或音频)
         */
        AVStream *stream = formatContext->streams[stream_index];

        /**
         * 第五步: 从上面流中获取编解码的参数
         * 后面的编解码器都需要此处的相关参数
         */
        AVCodecParameters *parameters = stream->codecpar;

        /**
         * 第六步: 根据上面的参数获取解码器
         */
        AVCodec *codec = avcodec_find_decoder(parameters->codec_id);

        if (!codec){
            if (helper){
                helper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            }

            avformat_close_input(&formatContext);
        }

        /**
         * 第七步: 获取编解码器的上下文(真正干活的), 目前是一张白纸
         */
        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            //TODO
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }

            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        /**
         * 第八步: 需要将parameters拷贝到codecContext
         */
        r = avcodec_parameters_to_context(codecContext, parameters);

        if (r < 0) {
            //TODO
            if (helper){
                helper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }

            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        /**
         * 第九步: 打开解码器
         */
        r = avcodec_open2(codecContext, codec, nullptr);
        if (r) {
            //TODO
            if (helper){
                helper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }

            avcodec_free_context(&codecContext);
            avformat_close_input(&formatContext);
            return;
        }

        //音视频同步
        AVRational  time_base = stream->time_base;

        /**
         * 第十步: 从编码器参数中获取流的类型 codec_type
         */
         if (parameters->codec_type ==  AVMediaType::AVMEDIA_TYPE_AUDIO){
            audio_channel = new AudioChannel(stream_index, codecContext, time_base);

            if (this->duration != 0){
                audio_channel->setJNICallbackHelper(helper);
            }

         } else if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO){
             //视频流，但是只有一帧封面
             if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC){
                 continue;
             }
             //音视频同步(视频独有的fps)
             AVRational fps_rational = stream->avg_frame_rate;
             int fps = av_q2d(fps_rational);

            video_channel = new VideoChannel(stream_index, codecContext, time_base, fps);
            video_channel->setRenderCallback(renderCallback);

             if (this->duration != 0){
                 video_channel->setJNICallbackHelper(helper);
             }
         }
    }//for end

    /**
     * 第十一步: 如果流中没有音频也没有视频
     */
    if (!audio_channel && !video_channel) {
        //TODO
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        }

        if (codecContext){
            avcodec_free_context(&codecContext);
        }
        avformat_close_input(&formatContext);
        return;
    }

    /**
      * 第十二步: 媒体文件准备完毕，通知给java层
      */
    if (helper){
        helper->onPrepared(THREAD_CHILD);
    }

}

void StuPlayer::prepare() {
    //解封装FFmpeg来解析。耗时操作，需要再子线程处理
    pthread_create(&pid_prepare, 0, task_prepare, this);
}

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> start

void* task_start(void * args) { //此函数和StuPlayer对象没有关系，你无法拿该对象的私有成员
    LOGD("StuPlayer task_start...");
    auto *player = static_cast<StuPlayer *>(args);

    player->start_();
    //注意，必须返回
    return nullptr;
}

void StuPlayer::start_() { //子线程
    LOGD("StuPlayer start_ begin: %d", pid_start);
    while (isPlaying){
        LOGD("Stuplayer, start_ loop...");
        //等待队列中的数据被消费
        if (video_channel && video_channel->packets.size() > 100){
            av_usleep(10*1000);//microseconds
            continue;
        }

        //等待队列中的数据被消费
        if (audio_channel && audio_channel->packets.size() > 100){
            av_usleep(10*1000);//microseconds
            continue;
        }

        //AVPacket--音视频压缩包
        AVPacket * packet = av_packet_alloc();
        int  ret = av_read_frame(formatContext, packet);
        if (!ret){//0--成功
            //把AVPacket*加入队列，音频和视频
            if (video_channel && video_channel->stream_index == packet->stream_index){
                LOGD("StuPlayer start-, insert packet to queue...");
                video_channel->packets.insertToQueue(packet);
            } else if (audio_channel && audio_channel->stream_index == packet->stream_index){
                audio_channel->packets.insertToQueue(packet);
            }
        } else if (ret == AVERROR_EOF){ //end of file 读到文件末尾
            LOGE("AVERROR_EOF...");
            //TODO 读完了，并不代表播放完成，要考虑是否播放完成。后面处理
            if (video_channel->packets.empty() && audio_channel->packets.empty()){
                break; //队列数据全部被播放完毕,退出
            }
        } else{
            LOGE("av_read_frame 出现了错误,结束当前循环...");
            break; //av_read_frame 出现了错误,结束当前循环
        }

    }// end while

    isPlaying = false;
    video_channel->stop();
    audio_channel->stop();
}

void StuPlayer::start() {
    LOGD("StuPlayer start...");
    isPlaying = 1;

    //1.解码 2.播放
    if (video_channel){
        //音视频同步
        video_channel->setAudioChannel(audio_channel);
        video_channel->start();
    }

    if (audio_channel){
        audio_channel->start();
    }

    //把音频和视频压缩包加入队列
    pthread_create(&pid_start, 0, task_start, this);
}

void StuPlayer::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

int StuPlayer::getDuration() {
    return duration;
}

void StuPlayer::seek(int progress) {
    // 健壮性判断
    if (progress < 0 || progress > duration) {
        // TODO 同学们自己去完成，给Java的回调
        return;
    }
    if (!audio_channel && !video_channel) {
        // TODO 同学们自己去完成，给Java的回调
        return;
    }
    if (!formatContext) {
        // TODO 同学们自己去完成，给Java的回调
        return;
    }

    // formatContext 多线程， av_seek_frame内部会对我们的 formatContext上下文的成员做处理，安全的问题
    // 互斥锁 保证多线程情况下安全

    pthread_mutex_lock(&seek_mutex);

    // FFmpeg 大部分单位 == 时间基AV_TIME_BASE
    /**
     * 1.formatContext 安全问题
     * 2.-1 代表默认情况，FFmpeg自动选择 音频 还是 视频 做 seek，  模糊：0视频  1音频
     * 3. AVSEEK_FLAG_ANY（老实） 直接精准到 拖动的位置，问题：如果不是关键帧，B帧 可能会造成 花屏情况
     *    AVSEEK_FLAG_BACKWARD（则优  8的位置 B帧 ， 找附件的关键帧 6，如果找不到他也会花屏）
     *    AVSEEK_FLAG_FRAME 找关键帧（非常不准确，可能会跳的太多），一般不会直接用，但是会配合用
     */
    int r = av_seek_frame(formatContext, -1, progress * AV_TIME_BASE, AVSEEK_FLAG_FRAME);
    if (r < 0) {
        // TODO 同学们自己去完成，给Java的回调
        return;
    }

    // TODO 如果你的视频，假设出了花屏，AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME， 缺点：慢一些
    // 有一点点冲突，后面再看 （则优  | 配合找关键帧）
    // av_seek_frame(formatContext, -1, progress * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);

    // 音视频正在播放，用户去 seek，我是不是应该停掉播放的数据  音频1frames 1packets，  视频1frames 1packets 队列

    // 这四个队列，还在工作中，让他们停下来， seek完成后，重新播放
    if (audio_channel) {
        audio_channel->packets.setWork(0);  // 队列不工作
        audio_channel->frames.setWork(0);  // 队列不工作
        audio_channel->packets.clear();
        audio_channel->frames.clear();
        audio_channel->packets.setWork(1); // 队列继续工作
        audio_channel->frames.setWork(1);  // 队列继续工作
    }

    if (video_channel) {
        video_channel->packets.setWork(0);  // 队列不工作
        video_channel->frames.setWork(0);  // 队列不工作
        video_channel->packets.clear();
        video_channel->frames.clear();
        video_channel->packets.setWork(1); // 队列继续工作
        video_channel->frames.setWork(1);  // 队列继续工作
    }

    pthread_mutex_unlock(&seek_mutex);
}

void *task_stop(void *args) {
    auto *player = static_cast<StuPlayer *>(args);
    player->stop_(player);
    return nullptr; // 必须返回，坑，错误很难找
}

void StuPlayer::stop_(StuPlayer * player) {
    isPlaying = false;
    pthread_join(pid_prepare, nullptr);
    pthread_join(pid_start, nullptr);

    // pid_prepare pid_start 就全部停止下来了  稳稳的停下来
    if (formatContext) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
    DELETE(audio_channel);
    DELETE(video_channel);
    DELETE(player);
}

void StuPlayer::stop() {
    // 只要用户关闭了，就不准你回调给Java成 start播放
    helper = nullptr;
    if (audio_channel) {
        audio_channel->jniCallbackHelper = nullptr;
    }
    if (video_channel) {
        video_channel->jniCallbackHelper = nullptr;
    }


    // 如果是直接释放 我们的 prepare_ start_ 线程，不能暴力释放 ，否则会有bug

    // 让他 稳稳的停下来

    // 我们要等这两个线程 稳稳的停下来后，我再释放DerryPlayer的所以工作
    // 由于我们要等 所以会ANR异常

    // 所以我们我们在开启一个 stop_线程 来等你 稳稳的停下来
    // 创建子线程
    pthread_create(&pid_stop, nullptr, task_stop, this);
}

