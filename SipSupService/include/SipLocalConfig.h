#ifndef _SIPLOCALCONFIG_H
#define _SIPLOCALCONFIG_H
#include"ConfReader.h"
#include "Common.h"
#include <list>
#include<algorithm>
class SipLocalConfig
{
    public:
    SipLocalConfig();
    ~SipLocalConfig();

    int ReadConf();

    inline string localIp(){return m_localIp;}
    inline int localPort(){return m_localPort;}
    inline string sipIp(){return m_sipIp;}
    inline int sipPort(){return m_sipPort;}

    struct SubNodeInfo
    {
        string id;
        string ip;
        int port;
        int poto;
        int auth; 
    };
    list<SubNodeInfo> ubNodeInfoList;

    private:
    ConfReader m_conf;
    string m_localIp;
    int m_localPort;
    string m_sipId;
    string m_sipIp;
    int m_sipPort;
    string m_subNodeIp;
    int m_subNodePort;
    int m_subNodePoto;
    int m_subNodeAuth;

};

#endif