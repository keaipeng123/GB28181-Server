#ifndef _GLOBALCTL_H
#define _GLOBALCTL_H
#include "Common.h"
#include "SipLocalConfig.h"
#include "ThreadPool.h"
#include "SipCore.h"

class GlobalCtl;
#define GBOJ(obj) GlobalCtl::instance()->obj //宏定义简化单例成员访问

//每个线程在使用PJSIP功能前必须注册
static pj_status_t pjcall_thread_register(pj_thread_desc desc)
{
    pj_thread_t* thread=0; 
    if(!pj_thread_is_registered())
    {
        return pj_thread_register(NULL,desc,&thread);
    }
    return PJ_SUCCESS;
}

class GlobalCtl
{
    public:
    static GlobalCtl* instance();
    bool init(void *param);//void* 指向任意类型的指针

    SipLocalConfig* gConfig;
    ThreadPool* gThpool =NULL;
    SipCore* gSipServer=NULL;

    typedef struct _SupDomainInfo{
        _SupDomainInfo()
        {
            sipId="";
            addrIp="";
            sipPort=0;
            protocal=0;
            registered=false;
            expires=0;
            isAuth=false;
            usr="";
            pwd="";
            realm="";
        }
        string sipId;
        string addrIp;
        int sipPort;
        int protocal;
        bool registered;
        int expires;
        bool isAuth;
        string usr;
        string pwd;
        string realm;
    }SupDomainInfo;
    typedef list<SupDomainInfo> SUPDOMAININFOLIST;

    SUPDOMAININFOLIST& getSupDomainInfoList()
    {
        return supDomainInfoList;
    }

    static void get_global_mutex()
    {
        pthread_mutex_lock(&globalLock);
    }

    static void free_global_mutex()
    {
        pthread_mutex_unlock(&globalLock);
    }

    static pthread_mutex_t globalLock;

    static bool gStopPool;

    private:
    //私有构造函数：防止外部通过 new GlobalCtl() 创建实例
    GlobalCtl(void)
    { 

    }
    ~GlobalCtl(void);
    //禁用拷贝和赋值：避免通过复制生成新实例
    GlobalCtl(const GlobalCtl& global);
    const GlobalCtl& operator=(const GlobalCtl& global);

    static GlobalCtl* m_pInstance;
    static SUPDOMAININFOLIST supDomainInfoList;
    
};
#endif