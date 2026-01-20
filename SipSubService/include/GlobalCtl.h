#ifndef _GLOBALCTL_H
#define _GLOBALCTL_H
#include "Common.h"
#include "SipLocalConfig.h"
#include "ThreadPool.h"
#include "SipCore.h"

class GlobalCtl;
#define GBOJ(obj) GlobalCtl::instance()->obj //宏定义简化单例成员访问
class GlobalCtl
{
    public:
    static GlobalCtl* instance();
    bool init(void *param);//void* 指向任意类型的指针

    SipLocalConfig* gConfig;
    ThreadPool* gThpool =NULL;
    SipCore* gSipServer=NULL;
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
    
};
#endif