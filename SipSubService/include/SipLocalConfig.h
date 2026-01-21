#ifndef _SIPLOCALCONFIG_H
#define _SIPLOCALCONFIG_H
#include"ConfReader.h"
#include "Common.h"
#include <list>

class SipLocalConfig
{
    public:
    SipLocalConfig();
    ~SipLocalConfig();

    int ReadConf();

    inline string localIp(){return m_localIp;}
    inline int localPort(){return m_localPort;}
    inline string sipId(){return m_sipId;}
    inline string sipIp(){return m_sipIp;}
    inline int sipPort(){return m_sipPort;}

     struct SupNodeInfo
    {
        string id;
        string ip;
        int port;
        int poto;
        int expires;
        string usr;
        string pwd;
        int auth; 
        string realm;
    };
    list<SupNodeInfo> upNodeInfoList;

    private:
    ConfReader m_conf;
    string m_localIp;
    int m_localPort;
    string m_sipId;
    string m_sipIp;
    int m_sipPort;
    string m_supNodeId;
    string m_supNodeIp;
    int m_supNodePort;
    int m_supNodePoto;
    int m_supNodeAuth;

};

#endif