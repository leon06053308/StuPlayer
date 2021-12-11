//
// Created by StuUngar on 2021/10/24.
//

#ifndef STUPLAYER_JNICALLBACKHELPER_H
#define STUPLAYER_JNICALLBACKHELPER_H


//#include "../../../../../../Android/Sdk/ndk/21.4.7075529/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/include/jni.h"
#include <jni.h>
#include "util.h"

class JNICallbackHelper {
    JavaVM *vm = 0;
    JNIEnv *env = 0;
    jobject job;
    jmethodID jmd_prepared;
    jmethodID jmd_onError;
    jmethodID jmd_onProgress; // 播放音频的时间搓回调


public:
    JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job);

    virtual ~JNICallbackHelper();

    void onPrepared(int thread_mode);

    void onError(int thread_mode, int error_code);

    void onProgress(int thread_mode, int audio_time);
};


#endif //STUPLAYER_JNICALLBACKHELPER_H
