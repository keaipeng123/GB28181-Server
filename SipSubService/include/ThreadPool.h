#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include "Common.h"
#include "ECThread.h"
#include <queue>
#include <unistd.h>
#include <semaphore.h>
using namespace EC;

//虚基类，只提供一个传虚接口
//使用不同的类继承这个基类，并根据不同的业务实现虚接口
class ThreadTask
{
    public:
    ThreadTask(){}
    virtual ~ThreadTask(){}
    virtual void run()=0;
};

class ThreadPool
{
    public:
    ThreadPool();
    ~ThreadPool();

    int createThreadPool(int threadCount);
    //阻塞当前线程
    int waitTask();
    int postTask(ThreadTask* task);

    static void* mainThread(void* argc);//线程入口函数
    //静态成员变量在头文件中需要声明，在cpp中需要定义
    static queue<ThreadTask*> m_taskQueue;//队列容器存储任务
    static pthread_mutex_t m_queueLock; 
    private:
    sem_t m_signalSem;//信号量
};
#endif