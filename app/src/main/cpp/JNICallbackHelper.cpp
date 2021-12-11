//
// Created by StuUngar on 2021/10/24.
//

#include "JNICallbackHelper.h"

JNICallbackHelper::JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job) {
    this->vm =vm;
    this->env =env;
    //this->job = job; //jobject不能跨越线程，必须全局引用
    this->job = env->NewGlobalRef(job); //提升为全局引用

    jclass clazz = env->GetObjectClass(job);
    jmd_prepared = env->GetMethodID(clazz, "onPrepared", "()V");
    jmd_onError = env->GetMethodID(clazz, "onError", "(I)V");
    // 播放音频的时间搓回调
    jmd_onProgress = env->GetMethodID(clazz, "onProgress", "(I)V");
}

JNICallbackHelper::~JNICallbackHelper() {
    vm = 0;
    env->DeleteGlobalRef(job);
    job = 0;
    env = 0;
}

void JNICallbackHelper::onPrepared(int thread_mode) {
    if (thread_mode == THREAD_MAIN){
        env->CallVoidMethod(job, jmd_prepared);
    } else if (thread_mode == THREAD_CHILD){
        //子线程，env不能跨线程
        JNIEnv * env_child;
        vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(job, jmd_prepared);
        vm->DetachCurrentThread();
    }


}

void JNICallbackHelper::onError(int thread_mode, int error_code) {
    if (thread_mode == THREAD_MAIN){
        env->CallVoidMethod(job, jmd_onError);
    } else {
        //子线程
        //当前子线程的 JNIEnv
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(job, jmd_onError, error_code);
        vm->DetachCurrentThread();
    }
}

void JNICallbackHelper::onProgress(int thread_mode, int audio_time) {
    if (thread_mode == THREAD_MAIN) {
        //主线程
        env->CallVoidMethod(job, jmd_onError);
    } else {
        //子线程
        //当前子线程的 JNIEnv
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(job, jmd_onProgress, audio_time);
        vm->DetachCurrentThread();
    }
}


