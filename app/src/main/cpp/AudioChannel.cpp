//
// Created by StuUngar on 2021/10/24.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int stream_index, AVCodecContext *codecContext, AVRational time_base)
:BaseChannel(stream_index, codecContext, time_base)
{
    //初始化缓冲区

    //音频三要素
    /**
     * 1.采样率 44100
     * 2.位深/采样格式大小  16bit
     * 3.声道数 2
    */
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);//STEREO: 双声道
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);//每个sample是16bit,2字节
    out_sample_rate = 44100;

    out_buffers_size = out_sample_rate * out_sample_size * out_channels;
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));

    //FFmpeg 音频重采样
    swr_ctx = swr_alloc_set_opts(0,

                                 //下面是输出
                                 AV_CH_LAYOUT_STEREO, //声道布局类型,双声道
                                 AV_SAMPLE_FMT_S16, //采样大小16bit
                                 out_sample_rate, //采样率

                                 //下面是输入
                                 codecContext->channel_layout, //声道布局类型
                                 codecContext->sample_fmt, //采样大小
                                 codecContext->sample_rate, //采样率
                                 0, 0); //采样率 44100

    swr_init(swr_ctx);
}

AudioChannel::~AudioChannel() {
    if (swr_ctx){
        swr_free(&swr_ctx);
    }

    DELETE(out_buffers);
}

void AudioChannel::stop() {

    // 等  解码线程  播放线程  全部停止，你再安心的做释放工作
    pthread_join(pid_audio_decode, nullptr);
    pthread_join(pid_audio_play, nullptr);

    // 保证两个线程执行完毕，我再释放  稳稳的释放

    isPlaying = false;
    packets.setWork(0);
    frames.setWork(0);

    // TODO 7、OpenSLES释放工作
    // 7.1 设置停止状态
    if (bqPlayerPlay) {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
        bqPlayerPlay = nullptr;
    }

    // 7.2 销毁播放器
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = nullptr;
        bqPlayerBufferQueue = nullptr;
    }

    // 7.3 销毁混音器
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = nullptr;
    }

    // 7.4 销毁引擎
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = nullptr;
        engineInterface = nullptr;
    }

    // 队列清空
    packets.clear();
    frames.clear();

}

void *task_audio_decode(void * args){
    auto *audio_channel = static_cast<AudioChannel *>(args);
    audio_channel->audio_decode();

    return 0;
}

void *task_audio_play(void * args){
    auto *audio_channel = static_cast<AudioChannel *>(args);
    audio_channel->audio_play();

    return 0;
}

void AudioChannel::start() {
    isPlaying = 1;

    packets.setWork(1);
    frames.setWork(1);

    //第一个线程,取出队列的压缩包进行解码，解码后的原始包push到队列中(pcm数据)
    pthread_create(&pid_audio_decode, 0, task_audio_decode, this);

    //第二个线程, 从队列取出原始包，播放
    pthread_create(&pid_audio_play, 0, task_audio_play, this);
}

void AudioChannel::audio_decode() {
    AVPacket *pkt = 0;
    while (isPlaying) {
        // 3.1 内存泄漏点
        if (isPlaying && frames.size() > 100) {
            av_usleep(10 * 1000); // 单位 ：microseconds 微妙 10毫秒
            continue;
        }

        int ret = packets.getQueueAndDel(pkt); // 阻塞式函数
        if (!isPlaying) {
            break; // 如果关闭了播放，跳出循环，releaseAVPacket(&pkt);
        }

        if (!ret){
            continue;
        }

        ret = avcodec_send_packet(codecContext, pkt);

        // FFmpeg源码缓存一份pkt，大胆释放即可
        // releaseAVPacket(&pkt);

        if (ret){
            break;
        }

        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);

        //音频也有帧的概念,所以获取原始包的时候，需要判断
        if (ret == AVERROR(EAGAIN)){
            continue;
        } else if (ret != 0){
            if(frame){
                releaseAVFrame(&frame);
            }
            break;
        }

        frames.insertToQueue(frame);

        av_packet_unref(pkt); //内部成员引用减一
        releaseAVPacket(&pkt);
    }

    av_packet_unref(pkt); //内部成员引用减一
    releaseAVPacket(&pkt);
}


/**
 * 4.3 TODO 回调函数
 * @param bq  队列
 * @param args  this // 给回调函数的参数
 */
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void * args) {

    auto *audio_channel = static_cast<AudioChannel *>(args);

    int pcm_size = audio_channel->getPCM();

    // 添加数据到缓冲区里面去
    (*bq)->Enqueue(
            bq, // 传递自己，为什么（因为没有this，为什么没有this，因为不是C++对象，所以需要传递自己） JNI讲过了
            audio_channel->out_buffers, // PCM数据
            pcm_size); // PCM数据对应的大小，缓冲区大小怎么定义？（复杂）
}

int AudioChannel::getPCM() {
    int pcm_data_size = 0;

    AVFrame *frame = 0;
    while (isPlaying) {
        int ret = frames.getQueueAndDel(frame);
        if (!isPlaying){
            break;
        }
        if (!ret){
            continue;
        }

        // 开始重采样

        // 来源：10个48000   ---->  目标:44100  11个44100
        // 获取单通道的样本数 (计算目标样本数： ？ 10个48000 --->  48000/44100因为除不尽  11个44100)
        int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples, // 获取下一个输入样本相对于下一个输出样本将经历的延迟
                                            out_sample_rate, // 输出采样率
                                            frame->sample_rate, // 输入采样率
                                            AV_ROUND_UP); // 先上取 取去11个才能容纳的上

        //返回的结果,每个通道输出的样本数(是转换后的)
        int samples_per_channel = swr_convert(swr_ctx,
                                              // 下面是输出区域
                                              &out_buffers,  // 【成果的buff】  重采样后的
                                              dst_nb_samples, // 【成果的 单通道的样本数 无法与out_buffers对应，所以有下面的pcm_data_size计算】

                                              //下面是输入区域
                                              (const uint8_t **) frame->data,
                                              frame->nb_samples);

        //由于out_buffers和dst_nb_samples无法对应, 需要重新计算
        pcm_data_size = samples_per_channel * out_sample_size * out_channels;

        //单通道样本数: 1024 * 2(通道数) * 2(16bit) = 4096

        //TODO 音视频同步
        //TimeBase时间基理解(fps25 , 25分之1就是TimeBase)
        /*typedef struct AVRational{
            int num; ///< Numerator //分子
            int den; ///< Denominator //分母
        } */
        audio_time = frame->best_effort_timestamp * av_q2d(time_base);

        if (this->jniCallbackHelper){
            jniCallbackHelper->onProgress(THREAD_CHILD, audio_time);
        }

        break;
    }

    av_frame_unref(frame);
    releaseAVFrame(&frame);
    return pcm_data_size;
}

//队列中取出pcm数据，用OpenSLES播放
void AudioChannel::audio_play() {
    SLresult  result;

    /**
     * 1.创建引擎并获取引擎接口
    */

    // 1.1 创建引擎对象: SLObjectItf
    result = slCreateEngine(&engineObject,0, 0, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result){
        LOGE("创建引擎slCreateEngine error");
        return;
    }

    //1.2 初始化引擎
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE); //SL_BOOLEAN_FALSE 延时等待创建成功
    if (SL_RESULT_SUCCESS != result){
        LOGE("创建引擎Realize error");
        return;
    }

    //1.3 获取引擎接口
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    if (SL_RESULT_SUCCESS != result){
        LOGE("创建引擎接口,GetInterface error");
        return;
    }

    if (engineInterface){
        LOGD("1.创建引擎接口成功，SUCCESS");
    } else{
        LOGE("创建引擎接口失败，FAIL");
        return;
    }

    /**
     * 2.设置混音器
    */

    //2.1 创建混音器
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);

    if (SL_RESULT_SUCCESS != result){
        LOGE("创建混音器,CreateOutputMix error");
        return;
    }

    //2.2 初始化混音器
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result){
        LOGE("初始化混音器,Realize error");
        return;
    }

    LOGD("2.设置混音器 SUCCESS");

    /**
     * 3.创建播放器
    */
    //3.1 创建buffer缓冲队列
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 10};

    //pcm不能直接播放，需要设置相关参数
    //SL_DATAFORMAT_PCM: 数据格式为pcm
    //SL_SAMPLINGRATE_44_1: 采样率为44100
    //SL_PCMSAMPLEFORMAT_FIXED_16: 采样格式为16bit
    //SL_PCMSAMPLEFORMAT_FIXED_16: 数据大小为16bit
    //SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT: 左右声道
    //SL_BYTEORDER_LITTLEENDIAN: 小端模式
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, //pcm数据
                                  2,                  //声道数
                                  SL_SAMPLINGRATE_44_1,  //采样率
                                  SL_PCMSAMPLEFORMAT_FIXED_16,
                                  SL_PCMSAMPLEFORMAT_FIXED_16,
                                  SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                  SL_BYTEORDER_LITTLEENDIAN};

    //将上面配置信息放到数据源中
    //audioSrc最终配置音频信息的成果，给后面使用
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    //3.2 配置音轨
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink  audioSink = {&loc_outmix, NULL};

    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean  req[1] = {SL_BOOLEAN_TRUE};

    //3.3 创建播放器SLObjectItf bqPlayerObject
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, //引擎接口
                                                   &bqPlayerObject, //播放器
                                                   &audioSrc, //音频配置信息
                                                   &audioSink, //混音器

                                                   //下面是打开队列的工作
                                                   1, //开放的队列个数
                                                   ids, //代表我们需要Buff
                                                   req //代表我们上面的Buff,需要开放出去
                                                   );

    if (SL_RESULT_SUCCESS != result){
        LOGE("创建播放器,CreateAudioPlayer fail!");
        return;
    }

    //3.4 初始化播放器
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result){
        LOGE("初始化播放器,Realize fail!");
        return;
    }

    LOGD("创建播放器 CreateAudioPlayer SUCCESS！");

    //3.5 获取播放器接口
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    if (SL_RESULT_SUCCESS != result){
        LOGE("获取播放器接口,GetInterface fail!");
        return;
    }

    LOGD("3. 创建播放器 SUCCESS");

    /**
     *4. 设置回调函数
     *
    */
    //4.1 获取播放器队列接口: SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue //播放需要的队列
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
    if (result != SL_RESULT_SUCCESS){
        LOGE("获取播放器队列接口,GetInterface SL_IID_BUFFERQUEUE fail!");
        return;
    }

    //4.2 设置回调 void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, //传入刚刚设置好的队列
                                             bqPlayerCallback, //回调函数
                                             this);    //给回调函数的参数
    LOGD("5. 设置回调函数 SUCCESS!");

    /**
     * 5. 设置播放器状态为播放状态
    */
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);

    LOGD("5. 设置播放器状态位播放状态!");

    /**
     * 6.手动激活回调函数
    */
    bqPlayerCallback(bqPlayerBufferQueue, this);

    LOGD("6. 手动激活回调函数 SUCCESS!");

}
