#include "SipCore.h"
#include "Common.h"
#include "SipDef.h"
#include"GlobalCtl.h"
#include "SipTaskBase.h"
#include "SipRegister.h"
#include "ECThread.h"   

using namespace EC;

static int pollingEvent(void* arg)
{
    LOG(INFO)<<"polling event thread start success";
    pjsip_endpoint* ept = (pjsip_endpoint*)arg;
    while(!(GlobalCtl::gStopPool))
    {
        pj_time_val timeout = {0,500};
        pj_status_t status = pjsip_endpt_handle_events(ept,&timeout);
        if(PJ_SUCCESS != status)
        {
            LOG(ERROR)<<"polling events failed,code:"<<status;
            return -1;
        }
    }
    return 0;
}

//rx 接收
pj_bool_t onRxRequest(pjsip_rx_data *rdata)
{
    LOG(INFO)<<"onRxRequest come in";
    if(NULL==rdata||NULL==rdata->msg_info.msg)
    {
        return PJ_FALSE;
    }
    threadParam* param=new threadParam();
    pjsip_msg* msg=rdata->msg_info.msg;
    pjsip_rx_data_clone(rdata,0,&param->data);
    if(msg->line.req.method.id==PJSIP_REGISTER_METHOD)
    {
        param->base=new SipRegister();
    }
    pthread_t pid;
    int ret=EC::ECThread::createThread(SipCore::dealTaskThread,param,pid);
    if(ret!=0)
    {
        LOG(ERROR)<<"create task thread error";
        if(param)
        {
            delete param;
            param=NULL;
        }
        return PJ_FALSE;
    }
    return PJ_SUCCESS;
}

static pjsip_module recv_mod=
{
    NULL,NULL,//链表操作，指定模块儿的上下层
    {"mod-recv",8},//name 字符串长度
    -1,//id
    PJSIP_MOD_PRIORITY_APPLICATION,//优先级设置为应用层
    NULL,
    NULL,
    NULL,
    NULL,//四个回调，此处均置为空，暂不需要(load,start,stop,unload)
    onRxRequest,//接收请求消息的回调
    NULL,//接收响应消息的回调，这个回调会返回响应码，如果这里指定，还需要知道响应码对应的请求事件，后面会在每次事务请求时立马获取到对应的响应码，这里不指定
    NULL,//tx 发送请求消息之前的回调
    NULL,//发送响应消息之前的回调，检查之用
    NULL,//事务状态变更回调
};

void* SipCore::dealTaskThread(void* arg)
{
    threadParam* param = (threadParam*)arg;
    if(!param || param->base == NULL)
    {
        return NULL;
    }
    /*
    | 线程来源                                 | 是否已注册 | 举例                                                       |
| ------------------------------------ | ----- | -------------------------------------------------------- |
| PJSIP 内部创建                           | ✅ 已注册 | `pj_thread_create()`、`pjsip_endpt_handle_events()` 所在的线程 |
| 外部框架 / 自己写的 `ECThread::createThread` | ❌ 未注册 | `SipCore::dealTaskThread`                                |
 是你自己的跨平台线程封装（或第三方库），PJSIP 根本不知道它的存在，因此 PJLIB TLS 尚未初始化，任何 PJSIP API 都会触发：
Assertion failed: !"Calling PJLIB from unknown/external thread"

    */
    pj_thread_desc desc;
    pjcall_thread_register(desc);
    param->base->run(param->data);
    delete param;
    param=NULL;
}


SipCore::SipCore()
:m_endpt(NULL)
{

}
SipCore::~SipCore()
{
    pjsip_endpt_destroy(m_endpt);
    pj_caching_pool_destroy(&m_cachingPool);
    pj_shutdown();
    GlobalCtl::gStopPool=true;
}

bool SipCore::InitSip(int sipPort)
{
    pj_status_t status;
    //0-关闭 6-详细
    pj_log_set_level(6);
    do
    {
        //【目的】初始化 pjlib 核心
        //【详解】pjlib 是所有 PJSIP 组件的地基；内部会初始化线程、锁、内存、高精度时钟、异常处理等。失败则整个栈都不可用。
        status=pj_init();
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init pjlib faild,code:"<<status;
            break;
        }
        //【目的】初始化 pjlib-util 附加库
        //【详解】提供 MD5、SHA1、STUN、DNS、XML 等实用函数，供后续 SIP/SDP 解析、认证、ICE 使用。
        status=pjlib_util_init();
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init pjlib util faild,code:"<<status;
            break;
        }
        //缓存池,pjsip中的内存均是这个缓存池分配的，所以需要将该缓存池储存起来，不能销毁,
        //此处定义了一个栈对象，当函数结束这个对象也就释放了；cachingPool需要和sipCore对象的生命周期一致，所以将其放在类的成员中
        //pj_caching_pool cachingPool;
        pj_caching_pool_init(&m_cachingPool,NULL,SIP_STACK_SIZE);

        //【目的】创建并返回 SIP 端点对象
        //【详解】端点是整个 SIP 栈的核心句柄，后续所有模块（事务层、UA 层、传输层、定时器、消息调度）都要挂到它上面。
        status=pjsip_endpt_create(&m_cachingPool.factory,NULL,&m_endpt);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"create endpt faild,code:"<<status;
            break;
        }

        //会话事务模块
        //目的】注册 事务层 模块
        //【详解】把 INVITE Client/Server Transaction、Non-INVITE Transaction 全部挂到 SIP 端点，提供状态机、定时器重传、ACK/CANCEL 处理。
        status=pjsip_tsx_layer_init_module(m_endpt);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init tsx layer faild,code:"<<status;
            break;
        }
        //会话模块
        //【目的】注册 UA (User Agent) 层 模块
        //【详解】UA 层在事务层之上，负责 dialog、call-leg、自动 100 Trying、自动 BYE on Cancel 等高层逻辑。
         status = pjsip_ua_init_module(m_endpt,NULL);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init UA layer faild,code:"<<status;
            break;
        }

        //sip 消息的发送和接收（tcp udp tls）
        //【目的】（自定义函数）为 SIP 端点绑定 传输层（UDP/TCP/TLS 端口）
        status=init_transport_layer(sipPort);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"init transport layer faild,code:"<<status;
            break;
        }

        //【目的】注册用户自定义的 接收/分发模块
        //【详解】recv_mod 里实现了回调 on_rx_request() / on_rx_response()，用来把收到的消息转给上层业务逻辑。
        status=pjsip_endpt_register_module(m_endpt,&recv_mod);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"register recv module faild,code:"<<status;
            break;
        }

        /*
        【目的】为 SIP 任务再建一个 持久内存池
        【详解】大小参数：初始块 1 MB，扩容块 1 MB；线程轮询、定时器、应用层对象都从这里分配，生命周期与 SIP 端点相同。
        【与m_caching_poll的区别】：在 SIP 栈（m_endpt）上先搭“大仓库”(pj_caching_pool)，再从仓库里拿一块“临时工地”(pj_pool)，最后用这块工地起一条 后台线程 不断调用 pjsip_endpt_handle_events()，让 PJSIP 能 7×24 小时收包、发包、跑定时器
        */
        pj_pool_t* pool=pjsip_endpt_create_pool(m_endpt,NULL,SIP_ALLOC_POOL_1M,SIP_ALLOC_POOL_1M);//分配内存池
        if(NULL==pool)
        {
            LOG(ERROR)<<"create pool faild";
            break;
        }

        pj_thread_t* eventThread = NULL;
        //【目的】创建 事件分发线程
        //【详解】线程入口 pollingEvent 里通常死循环调用 pjsip_endpt_handle_events() 或 pj_ioqueue_poll()，让 SIP 栈持续处理网络 IO、定时器、重传等后台任务。
        status=pj_thread_create(pool,NULL,&pollingEvent,m_endpt,0,0,&eventThread);//启动线程轮询处理endpt事务
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"create pjsip thread faild,code:"<<status;
            break;
        }


    }while(0);

    bool bret=true;
    if(PJ_SUCCESS!=status)
    {
        bret=false;      
    }
    return bret;
}

pj_status_t SipCore::init_transport_layer(int sipPort)
{
    pj_sockaddr_in addr;
    pj_bzero(&addr,sizeof(addr));//将 addr 结构体的内存区域全部置零，确保没有未初始化的残留数据
    addr.sin_family=pj_AF_INET();//指定地址族为 IPv4（AF_INET）
    addr.sin_addr.s_addr=0;//将 IP 地址设置为 0.0.0.0，表示监听所有本地网络接口（即允许来自任意网卡的连接）
    addr.sin_port=pj_htons((pj_uint16_t)sipPort);//pj_htons 将主机字节序转为网络字节序（大端），确保跨平台兼容性

    pj_status_t status;
    do
    {
        status=pjsip_udp_transport_start(m_endpt,&addr,NULL,1,NULL);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"start udp server faild,code:"<<status;
            break;
        }
        status=pjsip_tcp_transport_start(m_endpt,&addr,1,NULL);
        if(PJ_SUCCESS!=status)
        {
            LOG(ERROR)<<"start tcp server faild,code:"<<status;
            break;
        }

        LOG(INFO)<<"sip tcp:"<<sipPort<<" udp:"<<sipPort<<" running";
    } while (0);

    return status;
    
}