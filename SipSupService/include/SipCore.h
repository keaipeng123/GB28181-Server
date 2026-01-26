#ifndef _SIPCORE_H
#define _SIPCORE_H
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip/sip_auth.h>
#include <pjlib.h>

class SipCore
{
    public:
    SipCore();
    ~SipCore();

    bool InitSip(int sipPort);

    pj_status_t init_transport_layer(int sipPort);

    pjsip_endpoint* GetEndPoint(){return m_endpt;}

    private:
    pjsip_endpoint* m_endpt;
    pj_caching_pool m_cachingPool;
};
#endif