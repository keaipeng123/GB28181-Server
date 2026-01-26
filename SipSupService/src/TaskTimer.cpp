#include"TaskTimer.h"
#include"ECThread.h"
#include"Common.h"
#include"GlobalCtl.h"
using namespace EC;
TaskTimer::TaskTimer(int TimeSecond)
{
    m_timerFun=NULL;
    m_funParam=NULL;
    m_timeSecond=TimeSecond;
    m_timerStop=false;
}
TaskTimer::~TaskTimer()
{
    stop();
}
    
void TaskTimer::start()
{
    pthread_t pid;
    int ret=EC::ECThread::createThread(TaskTimer::timer,(void*)this,pid);
    if (ret!=0)
    {
        LOG(ERROR)<<"create thread failed";
    }
}
void TaskTimer::stop()
{
    m_timerStop=true;
}

void TaskTimer::setTimerFun(timerCallBack fun,void* param)
{
    m_timerFun=fun;
    m_funParam=param;
}

//静态成员函数没有this指针，只能通过参数传递
void* TaskTimer::timer(void* context)
{
    TaskTimer* pthis=(TaskTimer*)context;
    if(NULL==pthis)
    {
        return NULL;
    }

    unsigned long curTm=0;
    unsigned long lastTm=0;
    while(!pthis->m_timerStop)
    {
        struct timeval current;
        gettimeofday(&current,NULL);
        curTm=current.tv_sec*1000+current.tv_usec/1000;
        if((curTm-lastTm)>=(pthis->m_timeSecond)*1000)
        {
            lastTm=curTm;
            if(pthis->m_timerFun!=NULL)
            {
                pj_thread_desc desc;
                pjcall_thread_register(desc);
                pthis->m_timerFun(pthis->m_funParam);
            }
        }
        else
        {
            usleep(1000*1000);
            continue;
        }
    }
    return NULL;
}