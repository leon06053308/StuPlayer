//
// Created by StuUngar on 2021/10/25.
//

#ifndef STUPLAYER_SAFE_QUEUE_H
#define STUPLAYER_SAFE_QUEUE_H

#include <queue>
#include <pthread.h>

using namespace std;

template<typename T>
class SafeQueue{
private:
    typedef void (*ReleaseCallback)(T *);
    typedef void (*SyncCallback)(queue<T> &);

private:
    queue<T> queue;
    pthread_mutex_t mutex;
    pthread_cond_t cond; //等待和唤醒
    int work;//标记队列是否工作
    ReleaseCallback releaseCallback;
    SyncCallback  syncCallback;

public:
    SafeQueue(){
        pthread_mutex_init(&mutex, 0);
        pthread_cond_init(&cond, 0);
    }

    ~SafeQueue(){
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    void insertToQueue(T value){
        pthread_mutex_lock(&mutex);
        if (work){
            queue.push(value);
            pthread_cond_signal(&cond);
        } else{
            //非工作状态释放value,类型不明确，回调给外部进行释放操作
            if (releaseCallback){
                releaseCallback(&value);
            }
        }

        pthread_mutex_unlock(&mutex);
    }

    int getQueueAndDel(T &value){
        int ret = 0;

        pthread_mutex_lock(&mutex);

        while (work && queue.empty()){
            pthread_cond_wait(&cond, &mutex);
        }

        if (!queue.empty()){
            value = queue.front();
            queue.pop();
            ret = 1;
        }

        pthread_mutex_unlock(&mutex);

        return ret;
    }

    void setWork(int work){
        pthread_mutex_lock(&mutex);

        this->work = work;

        //每次设置状态后，就去唤醒下，有没有阻塞睡觉的地方
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    int empty(){
        return queue.empty();
    }

    int size(){
        return queue.size();
    }

    void clear(){
        pthread_mutex_lock(&mutex);

        unsigned int size = queue.size();

        for (int i = 0; i < size; ++i) {
            T value = queue.front();
            if (releaseCallback){
                releaseCallback(&value);
            }
            queue.pop();
        }

        pthread_mutex_unlock(&mutex);
    }

    void setReleaseCallback(ReleaseCallback releaseCallback){
        this->releaseCallback = releaseCallback;
    }

    void setSyncCallback(SyncCallback syncCallback){
        this->syncCallback = syncCallback;
    }

    void sync(){
        pthread_mutex_lock(&mutex);
        syncCallback(queue);
        pthread_mutex_unlock(&mutex);
    }
};

#endif //STUPLAYER_SAFE_QUEUE_H
