//
// Created by StuUngar on 2021/10/30.
//

#ifndef STUPLAYER_ANDROIDLOG_H
#define STUPLAYER_ANDROIDLOG_H

#include <android/log.h>
#define LOG_TAG    "Leo"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)

#endif //STUPLAYER_ANDROIDLOG_H
