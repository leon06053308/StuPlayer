#include <jni.h>
#include <string>
//#include <android/log.h>
#include "androidlog.h"
#include "StuPlayer.h"
#include "JNICallbackHelper.h"
#include <android/native_window_jni.h>

extern "C"{
     #include <libavutil//avutil.h>
}

/*#define TAG "Leo"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)*/

StuPlayer *player = nullptr;
JavaVM *vm = nullptr;
ANativeWindow *window = nullptr;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;//静态初始化锁

jint JNI_OnLoad(JavaVM * vm, void * args){
     ::vm = vm;
     return JNI_VERSION_1_6;
}

void renderFrame(uint8_t * src_data, int width, int height, int src_lineSize){
     LOGD("renderFrame...");
     pthread_mutex_lock(&mutex);

     if (!window){
          pthread_mutex_unlock(&mutex);
     }

     //设置窗口的大小，各个属性
     ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);

     ANativeWindow_Buffer   window_buffer;
     if (ANativeWindow_lock(window, &window_buffer, 0)){
          ANativeWindow_release(window);
          window = 0;
          pthread_mutex_unlock(&mutex);
          return;
     }

     //开始真正渲染。因为window没有被锁住，就可以把rgba数据字节对齐渲染
     //填充window_buff，画面就出来了
     uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
     int dst_linesize = window_buffer.stride * 4;

     for (int i = 0; i < window_buffer.height; ++i) { //图像一行一行显示
          //426*4(rgba8888) = 1704
          //memcpy(dst_data + i * 1704, src_data + i * 1704, 1704);

          //ANativeWindow_Buffer 16字节对齐， 1704无法以16字节对齐
         //memcpy(dst_data + i * 1792, src_data + i * 1704, 1792);

          memcpy(dst_data + i * dst_linesize, src_data + i * src_lineSize, dst_linesize);
     }

     //数据刷新
     ANativeWindow_unlockAndPost(window);

     pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL Java_com_planx_stuplayer_StuPlayer_prepareNative(JNIEnv *env, jobject thiz, jstring data_source) {
     const char* datasource_ = env->GetStringUTFChars(data_source, 0);

     auto *helper = new JNICallbackHelper(vm, env, thiz);

     player = new StuPlayer(datasource_, helper);
     player->setRenderCallback(renderFrame);
     player->prepare();

     env->ReleaseStringUTFChars(data_source, datasource_);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_planx_stuplayer_StuPlayer_startNative(JNIEnv *env, jobject thiz) {
     LOGD("startNative...");
     if (player){
          player->start();
     }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_planx_stuplayer_StuPlayer_stopNative(JNIEnv *env, jobject thiz) {
     if (player) {
          player->stop();
     }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_planx_stuplayer_StuPlayer_releaseNative(JNIEnv *env, jobject thiz) {
     pthread_mutex_lock(&mutex);

     // 先释放之前的显示窗口
     if (window) {
          ANativeWindow_release(window);
          window = nullptr;
     }

     pthread_mutex_unlock(&mutex);

     // 释放工作
     DELETE(player);
     DELETE(vm);
     DELETE(window);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_planx_stuplayer_StuPlayer_setSurfaceNative(JNIEnv *env, jobject thiz, jobject surface) {
     // TODO: implement setSurfaceNative()
     LOGD("setSurfaceNative...");
     pthread_mutex_lock(&mutex);

     //先释放之前的显示窗口
     if (window){
          ANativeWindow_release(window);
          window = nullptr;
     }

     //创建新的窗口用于视频显示
     window = ANativeWindow_fromSurface(env, surface);

     pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_planx_stuplayer_StuPlayer_getDurationNative(JNIEnv *env, jobject thiz) {
     // TODO: implement getDurationNative()
     if (player) {
          return player->getDuration();
     }
     return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_planx_stuplayer_StuPlayer_seekNative(JNIEnv *env, jobject thiz, jint play_value) {
     // TODO: implement seekNative()
     if (player) {
          player->seek(play_value);
     }
}