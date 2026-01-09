#include "SipLocalConfig.h"

#define CONFIGFILE_PATH "/home/GB28181-Server/SipSupService/conf/SipSupService.conf"
#define LOCAL_SECTION "localserver"
#define SIP_SECTION "sipserver"

//被所有的类对象共享且不可修改
static const string keyLocalIp="local_ip";
static const string keyLocalPort="local_port";

static const string keySipIp="sip_ip";
static const string keySipPort="sip_port";

static const string keySubNodeNum="subnode_num";
static const string keySubNodeIp="sip_subnode_ip";
static const string keySubNodePort="sip_subnode_port";
static const string keySubNodePoto="sip_subnode_poto";
static const string keySubNodeAuth="sip_subnode_auth";

SipLocalConfig::SipLocalConfig()
:m_conf(CONFIGFILE_PATH)
{
    m_localIp="";
    m_localPort=0;
    m_sipIp="";
    m_sipPort=0;
    m_subNodeIp="";
    m_subNodePort=0;
    m_subNodePoto=0;
    m_subNodeAuth=0;

}

SipLocalConfig::~SipLocalConfig()
{

}

int SipLocalConfig::ReadConf()
{
    int ret=0;
    m_conf.setSection(LOCAL_SECTION);
    m_localIp=m_conf.readStr(keyLocalIp);
    if (m_localIp.empty())
    {
        ret=-1;
        LOG(ERROR)<<"localIp is woring";
        return ret;
    }
    m_localPort=m_conf.readInt(keyLocalPort);
    if (m_localPort<=0)
    {
        ret=-1;
        LOG(ERROR)<<"localPort is woring";
        return ret;
    }
    m_conf.setSection(SIP_SECTION);
    m_sipIp=m_conf.readStr(keySipIp);
    if (m_sipIp.empty())
    {
        ret=-1;
        LOG(ERROR)<<"sipIp is woring";
        return ret;
    }
    m_sipPort=m_conf.readInt(keySipPort);
    if (m_sipPort<=0)
    {
        ret=-1;
        LOG(ERROR)<<"sipPort is woring";
        return ret;
    }
   
    LOG(INFO)<<"localip:"<<m_localIp<<",localPort:"<<m_localPort<<",sipIp:"<<m_sipIp\
    <<",sipPort:"<<m_sipPort;

    int num=m_conf.readInt(keySubNodeNum);
    for(int i=1;i<num+1;++i)
    {
        string ip=keySubNodeIp+to_string(i);
        string port=keySubNodePort+to_string(i);
        string proto=keySubNodePoto+to_string(i);
        string auth=keySubNodeAuth+to_string(i);
        
        m_subNodeIp=m_conf.readStr(ip);
        m_subNodePort=m_conf.readInt(port);
        m_subNodePoto=m_conf.readInt(proto);
        m_subNodeAuth=m_conf.readInt(auth);

        LOG(INFO)<<"m_subNodeIp:"<<m_subNodeIp<<",m_subNodePort:"<<m_subNodePort<<",m_subNodePoto:"<<m_subNodePoto\
        <<",m_subNodeAuth:"<<m_subNodeAuth;
    }

    return ret;
}

   