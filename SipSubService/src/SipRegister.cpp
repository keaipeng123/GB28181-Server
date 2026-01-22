#include "SipRegister.h"
#include "Common.h"
#include "SipMessage.h"

//回调函数不能有this指针，全局用static
static void  client_cb(struct pjsip_regc_cbparam *param)
{
    LOG(INFO)<<"code:"<<param->code;
    if(param->code==200)
    {
        GlobalCtl::SupDomainInfo* subinfo=(GlobalCtl::SupDomainInfo*)param->token;
        subinfo->registered=true;
    }
    return;
}

SipRegister::SipRegister()
{
    m_regTimer=new TaskTimer(3);
//    GlobalCtl::SUPDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSupDomainInfoList().begin();
//     for(;iter!=GlobalCtl::instance()->getSupDomainInfoList().end();iter++)
//     {
//         if(!(iter->registered))
//         {
//             if (gbRegister(*iter) < 0)
//             {
//                 LOG(ERROR)<<"register error for:"<<iter->sipId;
//             }
//         } 
//     }
   
}
SipRegister::~SipRegister()
{
    if(m_regTimer)
    {
        delete m_regTimer;
        m_regTimer=NULL; 
    }
}

void SipRegister::registerServiceStart()
{
    if(m_regTimer)
    {
        m_regTimer->setTimerFun(SipRegister::RegisterProc,(void*)this);
        m_regTimer->start();
    }
}

void SipRegister::RegisterProc(void* param)
{
    SipRegister* pthis=(SipRegister*)param;
    GlobalCtl::SUPDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter!=GlobalCtl::instance()->getSupDomainInfoList().end();iter++)
    {
        if(!(iter->registered))
        {
            if((pthis->gbRegister(*iter))<0)
            {
                LOG(ERROR)<<"register error for:"<<iter->sipId;
            }
        }
    }
    //GlobalCtl::free_global_mutex();
}



int SipRegister::gbRegister(GlobalCtl::SupDomainInfo& node)
{
    SipMessage msg;
    //From: <sip:11000000002000000001@127.0.1>;tag=2mdIf8lexOTBIMg2pPgKOBdDB3SowCcf
    msg.setFrom((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());//From字段 本级信息
    //To: <sip:11000000002000000001@127.0.1>
    msg.setTo((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str());//To字段 本级信息 下级向上级注册的时候，to字段是告诉上级要回复的地址（gb注册的时候是本级，其他都是对端）

    //Request-Line字段
    if (node.protocal==1)
    {
    //Request-Line: REGISTER sip:10000000002000000001@127.0.1:5061;transport=tcp SIP/2.0
    msg.setUrl((char*)node.sipId.c_str(),(char*)node.addrIp.c_str(),node.sipPort,(char*)"tcp");
    }
    else
    {
    msg.setUrl((char*)node.sipId.c_str(),(char*)node.addrIp.c_str(),node.sipPort);
    }
    //Contact: <sip:11000000002000000001@127.0.1:7101>
    msg.setContact((char*)GBOJ(gConfig)->sipId().c_str(),(char*)GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());

    pj_str_t from =pj_str(msg.FromHeader());
    pj_str_t to=pj_str(msg.ToHeader());
    pj_str_t line=pj_str(msg.RequestUrl());
    pj_str_t contact=pj_str(msg.Contact());

    pj_status_t status=PJ_SUCCESS;

    do
    {
        pjsip_regc* regc;
        status=pjsip_regc_create(GBOJ(gSipServer)->GetEndPoint(),&node,&client_cb,&regc);//client_cb注册后接收注册结果的回调函数
        if(PJ_SUCCESS!=status)
        {   
            LOG(ERROR)<<"pjsip_regc_create error";
            break;
        }
        status=pjsip_regc_init(regc,&line,&from,&to,1,&contact,node.expires);
        if(PJ_SUCCESS!=status)
        {
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_init error";
            break;
        }

        pjsip_tx_data* tdata=NULL;
        status=pjsip_regc_register(regc,PJ_TRUE,&tdata);
        if(PJ_SUCCESS!=status)
        {
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_register error";
            break;
        }
        status=pjsip_regc_send(regc,tdata);
        if(PJ_SUCCESS!=status)
        {
            pjsip_regc_destroy(regc);
            LOG(ERROR)<<"pjsip_regc_send error";
            break;
        }

    } while (0);

    int ret=0;
    if(PJ_SUCCESS!=status)
    {
        ret=-1;
    }
    return ret;

}