#ifndef _SIPTASKBASE_H
#define _SIPTASKBASE_H
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip/sip_auth.h>
#include <pjlib.h>
#include "Common.h"
#include <sys/sysinfo.h>
class SipTaskBase
{
    public:
    SipTaskBase(){}

    virtual ~SipTaskBase()//虚析构作用：确保多态删除时派生类的析构函数被调用，避免派生类资源泄漏
    {
        LOG(INFO)<<"~SipTaskBase";
    }

    virtual pj_status_t run(pjsip_rx_data *rdata)=0;
    //从 SIP 消息的 body 中提取一段 XML，获取根节点类型，以及某个指定标签的文本值
    static tinyxml2::XMLElement* parseXmlData(pjsip_msg* msg,string& rootType,const string xmlkey,string& xmlvalue);
    protected://只能由派生类去调用
    string parseFromId(pjsip_msg* msg);
};
#endif