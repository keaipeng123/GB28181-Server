#include "GlobalCtl.h"
GlobalCtl::SUPDOMAININFOLIST GlobalCtl::supDomainInfoList;
pthread_mutex_t GlobalCtl::globalLock=PTHREAD_MUTEX_INITIALIZER;//宏，用于静态初始化一个互斥锁。它相当于给这个锁赋一个默认的初始状态（未锁定、可用）。
bool GlobalCtl::gStopPool=false;
GlobalCtl* GlobalCtl::m_pInstance=NULL;

GlobalCtl* GlobalCtl::instance()
{
    if(!m_pInstance)
    {
        m_pInstance = new GlobalCtl();
    }
    return m_pInstance;
}

bool GlobalCtl::init(void *param)
{
    gConfig=(SipLocalConfig*)param;
    if (!gConfig)
    {
        return false;
    }

    SupDomainInfo info;
    auto iter =gConfig->upNodeInfoList.begin();//auto自动类型判断
    for(;iter != gConfig->upNodeInfoList.end();++iter)
    {
        info.sipId=iter->id;
        info.addrIp=iter->ip;
        info.sipPort=iter->port;
        info.protocal=iter->poto;
        info.expires=iter->expires;
        // if(iter->auth)
        // {
        //     info.isAuth=(iter->auth=1)?true:false;
        //     info.usr=iter->usr;
        //     info.pwd=iter->pwd;
        //     info.realm=iter->realm;
        // }
        supDomainInfoList.push_back(info);
    }

    LOG(INFO)<<"supDomainInfoList.SIZE:"<<supDomainInfoList.size();


    if (!gThpool)
    {
        gThpool=new ThreadPool();
        gThpool->createThreadPool(10);
    }
    if(!gSipServer)
    {
        gSipServer=new SipCore();
    }
    gSipServer->InitSip(gConfig->sipPort());
     
    return true;

}