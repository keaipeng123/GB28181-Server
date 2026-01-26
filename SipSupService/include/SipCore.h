#ifndef _SIPCORE_H
#define _SIPCORE_H
#include"SipTaskBase.h"

typedef struct _threadParam
{
    _threadParam()
    {
        base=NULL;
        data=NULL;
    }
    ~_threadParam()
    {
        if(base)
        {
            delete base;
            base=NULL;
        }
        if(data)
        {
            pjsip_rx_data_free_cloned(data);
            data=NULL;
        }
    }
    SipTaskBase* base;
    pjsip_rx_data* data;
}threadParam;

class SipCore
{
    public:
    SipCore();
    ~SipCore();

    bool InitSip(int sipPort);

    pj_status_t init_transport_layer(int sipPort);

    pjsip_endpoint* GetEndPoint(){return m_endpt;}

    public:
    static void* dealTaskThread(void* arg);

    private:
    pjsip_endpoint* m_endpt;
    pj_caching_pool m_cachingPool;
};
#endif