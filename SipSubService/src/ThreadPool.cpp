#include"ThreadPool.h"
//pthread_mutex_t ThreadPool::m_queueLock=PTHREAD_MUTEX_INITIALIZER;//锁有两种初始化方法，可在定义时初始化（静态初始化），也可调用接口的方式初始化（动态初始化）
#include "GlobalCtl.h"
pthread_mutex_t ThreadPool::m_queueLock;//动态初始化需要手动释放资源
queue<ThreadTask*> ThreadPool::m_taskQueue;

ThreadPool::ThreadPool()
{
    pthread_mutex_init(&m_queueLock,NULL);
    sem_init(&m_signalSem,0,0);
}
ThreadPool::~ThreadPool()
{
    pthread_mutex_destroy(&m_queueLock);
    sem_destroy(&m_signalSem);
}
int ThreadPool::createThreadPool(int threadCount)
{
    if (threadCount<=0)
    {
        LOG(ERROR)<<"thread count error";
        return -1;
    }

    for(int i=0;i<threadCount;++i)
    {
        pthread_t pid;
        if(EC::ECThread::createThread(ThreadPool::mainThread,(void*)this,pid)<0)
        {
            LOG(ERROR)<<"create thread error";
        }

        LOG(INFO)<<"thread:"<<pid<<" was created";
    }
}

void* ThreadPool::mainThread(void* argc)
{
    ThreadPool* pthis=(ThreadPool*)argc;
    do
    {
        int ret=pthis->waitTask();
        if (ret==0)
        {
            ThreadTask* task=NULL;
            //m_taskQueue是线程间的共享资源，需要使用互斥锁保护并发访问的影响
            pthread_mutex_lock(&m_queueLock);
            if (m_taskQueue.size()>0)
            {
                task=m_taskQueue.front();
                m_taskQueue.pop();
            }
            pthread_mutex_unlock(&m_queueLock);
            if(task)
            {
                task->run();
                delete task;
            }
        }
    }while(true);
}

int ThreadPool::waitTask()
{
    int ret=0;
    ret=sem_wait(&m_signalSem);
    if(ret!=0)
    {
        LOG(ERROR)<<"the api exec error";
    }
    return ret;
}

int ThreadPool::postTask(ThreadTask* task)
{
    if(task)
    {
        pthread_mutex_lock(&m_queueLock);
        m_taskQueue.push(task);
        pthread_mutex_unlock(&m_queueLock);
        sem_post(&m_signalSem);
        
    }
}