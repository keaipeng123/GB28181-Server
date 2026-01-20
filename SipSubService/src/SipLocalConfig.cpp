#include "SipLocalConfig.h"

#define CONFIGFILE_PATH "/home/GB28181-Server/SipSubService/conf/SipSubService.conf"
#define LOCAL_SECTION "localserver"
#define SIP_SECTION "sipserver"

//被所有的类对象共享且不可修改
static const string keyLocalIp="local_ip";
static const string keyLocalPort="local_port";

static const string keySipIp="sip_ip";
static const string keySipPort="sip_port";

static const string keySupNodeNum="supnode_num";
static const string keySupNodeIp="sip_supnode_ip";
static const string keySupNodePort="sip_supnode_port";
static const string keySupNodePoto="sip_supnode_poto";
static const string keySupNodeAuth="sip_supnode_auth";

SipLocalConfig::SipLocalConfig()
:m_conf(CONFIGFILE_PATH)
{
    m_localIp="";
    m_localPort=0;
    m_sipIp="";
    m_sipPort=0;
    m_supNodeIp="";
    m_supNodePort=0;
    m_supNodePoto=0;
    m_supNodeAuth=0;

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

    int num=m_conf.readInt(keySupNodeNum);
    for(int i=1;i<num+1;++i)
    {
        string ip=keySupNodeIp+to_string(i);
        string port=keySupNodePort+to_string(i);
        string proto=keySupNodePoto+to_string(i);
        string auth=keySupNodeAuth+to_string(i);
        
        m_supNodeIp=m_conf.readStr(ip);
        m_supNodePort=m_conf.readInt(port);
        m_supNodePoto=m_conf.readInt(proto);
        m_supNodeAuth=m_conf.readInt(auth);

        LOG(INFO)<<"m_supNodeIp:"<<m_supNodeIp<<",m_supNodePort:"<<m_supNodePort<<",m_supNodePoto:"<<m_supNodePoto\
        <<",m_supNodeAuth:"<<m_supNodeAuth;
    }

    return ret;
}
