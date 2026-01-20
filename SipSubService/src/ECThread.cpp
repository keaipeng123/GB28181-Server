#include "ECThread.h"
using namespace EC;

int ECThread::createThread(ECThreadFunc startRoutine,void* args,pthread_t& id)
{
    int ret=0;
    pthread_attr_t threadAttr;//定义线程属性
    pthread_attr_init(&threadAttr);//初始化
    do
    {
        ret=pthread_attr_setdetachstate(&threadAttr,PTHREAD_CREATE_DETACHED);//设置线程属性，创建时设置为可分离
        if(ret!=0)
            break;
        ret=pthread_create(&id,&threadAttr,startRoutine,args);//创建线程
        if(ret!=0)
            break;
    } while (0);//允许通过 break 提前退出流程，避免深层嵌套的 if 判断
    //销毁属性对象
    pthread_attr_destroy(&threadAttr);
    if (ret!=0)
    {
        ret=-1;
    }
    return ret;
    

}
int ECThread::detachSelf()
{
    int ret=pthread_detach(pthread_self());
    if (ret!=0)
    {
        return -1;
    }
    return 0;
}
void ECThread::exitSelf(void* rval)
{
    pthread_exit(rval);

}
int ECThread::waitThread(const pthread_t& id,void** rval)
{
    int ret =pthread_join(id,rval);
    if(ret!=0)
    {
        return -1;
    }
    return 0;
}
int ECThread::terminateThread(const pthread_t& id)
{
    int ret=pthread_cancel(id);
    if (ret!=0)
        return -1;
    return 0;    
}