#ifndef _SIPLOCALCONFIG_H
#define _SIPLOCALCONFIG_H
#include"ConfReader.h"
#include "Common.h"

class SipLocalConfig
{
    public:
    SipLocalConfig();
    ~SipLocalConfig();

    int ReadConf();

    inline string localIp(){return m_localIp;}
    inline string sipIp(){return m_sipIp;}
    inline int sipPort(){return m_sipPort;}

    private:
    ConfReader m_conf;
    string m_localIp;
    int m_localPort;
    string m_sipIp;
    int m_sipPort;
    string m_subNodeIp;
    int m_subNodePort;
    int m_subNodePoto;
    int m_subNodeAuth;

};

#endif