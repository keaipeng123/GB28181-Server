 #ifndef _SIPREGISTER_H
 #define _SIPREGISTER_H
 #include "GlobalCtl.h"
 #include"TaskTimer.h"
 

class SipRegister
{
    public:
    SipRegister();
    ~SipRegister();

    int gbRegister(GlobalCtl::SupDomainInfo& node);
    void registerServiceStart();
    static void RegisterProc(void* param);

    private:
    TaskTimer* m_regTimer;

};

 #endif