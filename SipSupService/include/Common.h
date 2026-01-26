#ifndef _COMMON_H
#define _COMMON_H
#include <glog/logging.h>
#include"tinyxml2.h"
#include"json/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>

using namespace std;

#define LOG_DIR "/home/GB28181-Server/log"
#define LOG_FILE_NAME "SipSupService.log" 

class AutoMutexLock
{
    public:
    AutoMutexLock(pthread_mutex_t* l):lock(l)
    {
        LOG(INFO)<<"getLock";
        getLock();
    };
    ~AutoMutexLock()
    {
        LOG(INFO)<<"freeLock";
        freeLock();
    };
    private:
    //将默认构造，拷贝构造，运算符重载私有化，禁止外部调用
    AutoMutexLock();
    AutoMutexLock(const AutoMutexLock&);
    AutoMutexLock& operator=(const AutoMutexLock&);
    void getLock(){pthread_mutex_lock(lock);}
    void freeLock(){pthread_mutex_unlock(lock);}
    pthread_mutex_t* lock;
};

#endif