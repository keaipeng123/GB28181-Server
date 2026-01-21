#include "SipRegister.h"
#include "Common.h"

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
   GlobalCtl::SUPDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSupDomainInfoList().begin();
    for(;iter!=GlobalCtl::instance()->getSupDomainInfoList().end();iter++)
    {
        if(!(iter->registered))
        {
            if (gbRegister(*iter) < 0)
            {
                LOG(ERROR)<<"register error for:"<<iter->sipId;
            }
        } 
    }
   
}
SipRegister::~SipRegister()
{
   
}



int SipRegister::gbRegister(GlobalCtl::SupDomainInfo& node)
{
   char fromHeader[128] = {0};
   sprintf(fromHeader, "<sip:%s@%s>", GBOJ(gConfig)->sipId().c_str(), GBOJ(gConfig)->sipIp().c_str());

   char toHeader[128] = {0};
   sprintf(toHeader, "<sip:%s@%s>", GBOJ(gConfig)->sipId().c_str(), GBOJ(gConfig)->sipIp().c_str());

   char requestUrl[128] = {0};
   sprintf(requestUrl, "sip:%s@%s:%d;transport=%s",node.sipId.c_str(), node.addrIp.c_str(), node.sipPort, node.protocal==1?"tcp":"udp");

   char contactUrl[128] = {0};
   sprintf(contactUrl, "<sip:%s@%s:%d>",GBOJ(gConfig)->sipId().c_str(), GBOJ(gConfig)->sipIp().c_str(),GBOJ(gConfig)->sipPort());

   pj_str_t from = pj_str(fromHeader);
   pj_str_t to = pj_str(toHeader);
   pj_str_t line = pj_str(requestUrl);
   pj_str_t contact = pj_str(contactUrl);

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




}