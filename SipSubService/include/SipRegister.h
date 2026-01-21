 #ifndef _SIPREGISTER_H
 #define _SIPREGISTER_H
 #include "GlobalCtl.h"

class SipRegister
{
    public:
    SipRegister();
    ~SipRegister();

    int gbRegister(GlobalCtl::SupDomainInfo& node);

};

 #endif