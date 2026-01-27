#include "GlobalCtl.h"
GlobalCtl::SUBDOMAININFOLIST GlobalCtl::subDomainInfoList;
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

    SubDomainInfo info;
    auto iter =gConfig->ubNodeInfoList.begin();//auto自动类型判断
    for(;iter != gConfig->ubNodeInfoList.end();++iter)
    {
        info.sipId=iter->id;
        info.addrIp=iter->ip;
        info.sipPort=iter->port;
        info.protocal=iter->poto;
        info.isAuth=iter->auth;
        // if(iter->auth)
        // {
        //     info.isAuth=(iter->auth=1)?true:false;
        //     info.usr=iter->usr;
        //     info.pwd=iter->pwd;
        //     info.realm=iter->realm;
        // }
        subDomainInfoList.push_back(info);
    }

    LOG(INFO)<<"subDomainInfoList.SIZE:"<<subDomainInfoList.size();

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

bool GlobalCtl::checkIsExist(string id)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it=std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it!=subDomainInfoList.end())
    {
        return true;
    }
    return false;
}


void GlobalCtl::setExpires(string id,int expires)
{
    AutoMutexLock lck(&globalLock);
    SUBDOMAININFOLIST::iterator it;
    it=std::find(subDomainInfoList.begin(),subDomainInfoList.end(),id);
    if(it!=subDomainInfoList.end())
    {
        it->expires=expires;
    }
}