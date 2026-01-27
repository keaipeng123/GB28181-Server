#include"SipRegister.h"
#include"Common.h"
#include"SipDef.h"
#include"GlobalCtl.h"
#include<sys/sysinfo.h>


// static pj_status_t auth_cred_callback(pj_pool_t *pool,
//     const pj_str_t *realm,
//     const pj_str_t *acc_name,
//     pjsip_cred_info *cred_info)
// {
//     // //string usr_str=GBOJ(gConfig)->usr();
//     // pj_str_t usr=pj_str((char*)GBOJ(gConfig)->usr().c_str());
//     // if(pj_stricmp(acc_name,&usr)!=0)
//     // {
//     //     LOG(ERROR)<<"usr name wrong";
//     //     return PJ_FALSE;
//     // }
//     // //string pwd_str=GBOJ(gConfig)->pwd();
//     // pj_str_t pwd=pj_str((char*)GBOJ(gConfig)->pwd().c_str());
//     // cred_info->realm=*realm;
//     // cred_info->username=usr;
//     // //cred_info->username=pj_str("123123");
//     // cred_info->data_type=PJSIP_CRED_DATA_PLAIN_PASSWD;
//     // cred_info->data=pwd;
//     // 	// 打印 cred_info 关键字段（假设是 pjsip_passwd 类型）
// 	// printf("first_cred_info->username: %.*s\n",
//     //     (int)cred_info->username.slen,
//     //     cred_info->username.ptr);
//     // printf("first_cred_info->data_type: %d\n", cred_info->data_type); // 例如 PJSIP_CRED_DATA_PLAIN_PASSWD
//     // //cred_info->data=pj_str("123123");
//     // return PJ_SUCCESS;
//     // 从配置获取用户名
//     const std::string& usr_str = GBOJ(gConfig)->usr();
    
//     // 创建临时pj_str_t并复制到内存池
//     pj_str_t src_usr = pj_str((char*)usr_str.c_str());
//     pj_str_t usr;
//     pj_strdup(pool, &usr, &src_usr); // 正确使用3个参数的pj_strdup

//     if (pj_stricmp(acc_name, &usr) != 0) {
//         LOG(ERROR) << "usr name wrong";
//         return PJ_FALSE;
//     }

//     // 从配置获取密码
//     const std::string& pwd_str = GBOJ(gConfig)->pwd();
    
//     // 创建临时pj_str_t并复制到内存池
//     pj_str_t src_pwd = pj_str((char*)pwd_str.c_str());
//     pj_str_t pwd;
//     pj_strdup(pool, &pwd, &src_pwd);

//     // 设置凭证信息
//     cred_info->realm = *realm;       // PJSIP会自动管理这个内存
//     cred_info->username = usr;       // 使用池分配后的用户名
//     cred_info->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
//     cred_info->data = pwd;           // 使用池分配后的密码

//     return PJ_SUCCESS;
// }
SipRegister::SipRegister()
{
    m_regTimer=new TaskTimer(10);
}

SipRegister::~SipRegister()
{
    if(m_regTimer)
    {
        delete m_regTimer;
        m_regTimer=NULL;
    }
}

// void SipRegister::registerServiceStart()
// {
//     if(m_regTimer)
//     {
//         m_regTimer->setTimerFun(RegisterCheckProc,this);
//         m_regTimer->start();
//     }
// }

// void SipRegister::RegisterCheckProc(void* param)
// {
//     time_t regTime=0;
//     struct sysinfo info;
//     memset(&info,0,sizeof(info));
//     int ret=sysinfo(&info);
//     if(ret==0)
//     {
//         regTime=info.uptime;
//     }
//     else
//     {
//         regTime=time(NULL);
//     }
//     AutoMutexLock lock(&GlobalCtl::globalLock);
//     GlobalCtl::SUBDOMAININFOLIST::iterator iter=GlobalCtl::instance()->getSubDomainInfoList().begin();
//     for(;iter!=GlobalCtl::instance()->getSubDomainInfoList().end();iter++)
//     {
//         if(iter->registered)
//         {
//             LOG(INFO)<<"regTime:"<<regTime<<",lastRegTime"<<iter->lastRegTime;
//             if(regTime-(iter->lastRegTime)>=iter->expires)
//             {
//                 iter->registered=false;
//                 LOG(INFO)<<"registet time was gone";
//             }
//         }
//     }
// }

pj_status_t SipRegister::run(pjsip_rx_data *rdata)
{
    return RegisterRequestMessage(rdata);
}

pj_status_t SipRegister::RegisterRequestMessage(pjsip_rx_data *rdata)
{
    pjsip_msg* msg=rdata->msg_info.msg;
    //if(GlobalCtl::getAuth(parseFromId(msg)))
    //{
    //    return dealWithAuthorRegister(rdata);
    //}
    //else
    //{
        return dealWithRegister(rdata);
    //}
}

// #if 1
// pj_status_t SipRegister::dealWithAuthorRegister(pjsip_rx_data* rdata)
// {
//     pjsip_msg* msg=rdata->msg_info.msg;
//     string fromId=parseFromId(msg);
//     pj_int32_t expiresValue=0;
//     pjsip_hdr hdr_list;
//     pj_list_init(&hdr_list);
//     int status_code=401;
//     pj_status_t status;
//     bool registered=false;
//     if(pjsip_msg_find_hdr(msg,PJSIP_H_AUTHORIZATION,NULL)==NULL)
//     {
//         pjsip_www_authenticate_hdr* hdr=pjsip_www_authenticate_hdr_create(rdata->tp_info.pool);
//         hdr->scheme=pj_str("digest");
//         //nonce
//         string nonce=GlobalCtl::randomNum(32);
//         LOG(INFO)<<"nonce:"<<nonce;
//         pj_str_t src_nonce = pj_str((char*)nonce.c_str());
//         pj_str_t res_nonce;
//         pj_strdup(rdata->tp_info.pool, &res_nonce, &src_nonce);
//         hdr->challenge.digest.nonce=res_nonce;
//         //realm
//         string realm=GBOJ(gConfig)->realm();
//         LOG(INFO)<<"realm:"<<realm;
//         hdr->challenge.digest.realm=pj_str((char*)realm.c_str());
//         #if 0
//         hdr->challenge.digest.realm=pj_str((char*)GBOJ(gConfig)->realm().c_str());
//         #endif
//         //opaque
//         string opaque=GlobalCtl::randomNum(32);
//         LOG(INFO)<<"opaque:"<<opaque;
//         pj_str_t src_opaque=pj_str((char*)opaque.c_str());
//         pj_str_t res_opaque;
//         pj_strdup(rdata->tp_info.pool,&res_opaque,&src_opaque);
//         hdr->challenge.digest.opaque=res_opaque;
//         //加密方式
//         hdr->challenge.digest.algorithm=pj_str("MD5");

//         pj_list_push_back(&hdr_list,hdr);
//     }
//     else
//     {
//         pjsip_auth_srv auth_srv;
//         string realm_str=GBOJ(gConfig)->realm();
//         pj_str_t realm=pj_str((char*)realm_str.c_str());
//         status=pjsip_auth_srv_init(rdata->tp_info.pool,&auth_srv,&realm,&auth_cred_callback,0);
//         if(PJ_SUCCESS!=status)
//         {
//             LOG(ERROR)<<"pjsip_auth_srv_init failed";
//             status_code=401;
//         }
//         pjsip_auth_srv_verify(&auth_srv,rdata,&status_code);
//         // status=pjsip_auth_srv_verify(&auth_srv,rdata,&status_code);
//         // if(PJ_SUCCESS!=status)
//         // {
//         //     LOG(ERROR)<<"pjsip_auth_srv_verify failed,pj_status:"<<status;
//         //     status_code=401;
//         // }
//         LOG(INFO)<<"status_code_t:"<<status_code;
//         if(SIP_SUCCESS==status_code)
//         {
//             pjsip_expires_hdr* expires=(pjsip_expires_hdr*)pjsip_msg_find_hdr(msg,PJSIP_H_EXPIRES,NULL);
//             expiresValue=expires->ivalue;
//             GlobalCtl::setExpires(fromId,expiresValue);

//             //data字段hdr部分组织
//             time_t t;
//             t=time(0);
//             char bufT[32]={0};
//             strftime(bufT,sizeof(bufT),"%y-%m-%d%H:%M:%S",localtime(&t));
//             pj_str_t value_time =pj_str(bufT);
//             pj_str_t key=pj_str("Date");
//             pjsip_date_hdr* date_hrd= (pjsip_date_hdr*)pjsip_date_hdr_create(rdata->tp_info.pool,&key,&value_time);
//             pj_list_push_back(&hdr_list,date_hrd);
//             registered=true;
//         }

//     }
//     status=pjsip_endpt_respond(GBOJ(gSipServer)->GetEndPoint(),NULL,rdata,status_code,NULL,&hdr_list,NULL,NULL);
//     if(PJ_SUCCESS!=status)
//     {
//         LOG(ERROR)<<"pjsip_endpt_respond failed";
//         return status;
//     }

//     if(registered)
//     {
//         if(expiresValue>0)
//         {
//             time_t regTime=0;
//             struct sysinfo info;
//             memset(&info,0,sizeof(info));
//             int ret=sysinfo(&info);
//             if(ret==0)
//             {
//                 regTime=info.uptime;
//             }
//             else
//             {
//                 regTime=time(NULL);
//             }
//             GlobalCtl::setRegister(fromId,true);
//             GlobalCtl::setLastRegTime(fromId,regTime);
//         }
//         else if(expiresValue==0)
//         {
//             GlobalCtl::setRegister(fromId,false);
//             GlobalCtl::setLastRegTime(fromId,0);
//         }
//     }
// }
// #endif

pj_status_t SipRegister::dealWithRegister(pjsip_rx_data *rdata)
{
    //string random=GlobalCtl::randomNum(32);
    //LOG(INFO)<<"random"<<random;
    pjsip_msg* msg=rdata->msg_info.msg;
    int status_code=200;
    pj_int32_t expiresValue=0;
    string fromId=parseFromId(msg);
    LOG(INFO)<<"fromId:"<<fromId;
    if(!GlobalCtl::checkIsExist(fromId))
    {
        status_code=SIP_FORBIDDEN;
    }
    else
    {
        pjsip_expires_hdr* expires=(pjsip_expires_hdr*)pjsip_msg_find_hdr(msg,PJSIP_H_EXPIRES,NULL);
        expiresValue=expires->ivalue;
        GlobalCtl::setExpires(fromId,expiresValue);
    }
    pjsip_tx_data* txdata;
    //创建txdata数据结构
    pj_status_t status=pjsip_endpt_create_response(GBOJ(gSipServer)->GetEndPoint(),rdata,status_code,NULL,&txdata);
    if(PJ_SUCCESS!=status)
    {
        LOG(ERROR)<<"create response failed";
        return status;
    }
    time_t t;
    t=time(0);
    char bufT[32]={0};
    strftime(bufT,sizeof(bufT),"%y-%m-%d%H:%M:%S",localtime(&t));
    pj_str_t value_time =pj_str(bufT);
    pj_str_t key=pj_str("Date");
    pjsip_date_hdr* date_hrd= (pjsip_date_hdr*)pjsip_date_hdr_create(rdata->tp_info.pool,&key,&value_time);
    pjsip_msg_add_hdr(txdata->msg,(pjsip_hdr*)date_hrd);

    pjsip_response_addr res_addr;
    //获取响应的地址
    status=pjsip_get_response_addr(txdata->pool,rdata,&res_addr);
    if(PJ_SUCCESS!=status)
    {
        pjsip_tx_data_dec_ref(txdata);
        LOG(ERROR)<<"get response addr failed";
        return status;
    }
    //发送响应
    status=pjsip_endpt_send_response(GBOJ(gSipServer)->GetEndPoint(),&res_addr,txdata,NULL,NULL);
    if(PJ_SUCCESS!=status)
    {
        pjsip_tx_data_dec_ref(txdata);
        LOG(ERROR)<<"send response msg failed";
        return status;
    }
    // if(status_code==200)
    // {
    //     if(expiresValue>0)
    //     {
    //         time_t regTime=0;
    //         struct sysinfo info;
    //         memset(&info,0,sizeof(info));
    //         int ret=sysinfo(&info);
    //         if(ret==0)
    //         {
    //             regTime=info.uptime;
    //         }
    //         else
    //         {
    //             regTime=time(NULL);
    //         }
    //         GlobalCtl::setRegister(fromId,true);
    //         GlobalCtl::setLastRegTime(fromId,regTime);
    //     }
    //     else if(expiresValue==0)
    //     {
    //         GlobalCtl::setRegister(fromId,false);
    //         GlobalCtl::setLastRegTime(fromId,0);
    //     }
    // }

    return PJ_SUCCESS;
    
}