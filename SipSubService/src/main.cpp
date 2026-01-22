#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <signal.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip/sip_auth.h>
#include"rtpsession.h"
#include"rtpsourcedata.h"
#include"rtptcptransmitter.h"
#include"rtptcpaddress.h"
#include"rtpudpv4transmitter.h"
#include"rtpipv4address.h"
#include"rtpsessionparams.h"
#include"rtperrors.h"
#include"rtplibraryversion.h"
#include"rtcpsrpacket.h"

#include"event2/event.h"
#include"event2/listener.h"
#include"event2/bufferevent.h"
#include"event2/buffer.h"
#include"event2/thread.h"

#include"Common.h"
#include"SipLocalConfig.h"
#include"GlobalCtl.h"

#include"ECThread.h"
#include"SipRegister.h"
using namespace EC;

class SetGlogLevel
{
	public:
	SetGlogLevel(const int type)
	{
		//将日志重定向到指定文件中
		google::InitGoogleLogging(LOG_FILE_NAME);
		//设置输出控制台的Log等级
		FLAGS_stderrthreshold=type;
		//设置颜色区分
		FLAGS_colorlogtostderr=true;
		//设置日志缓冲区的刷新时间,0不使用缓冲区
		FLAGS_logbufsecs=0;
		//日志文件目录
		FLAGS_log_dir=LOG_DIR;
		//设置最大日志文件为4M
		FLAGS_max_log_size=4;
		//将warning和error写到文件中
		google::SetLogDestination(google::GLOG_WARNING,"");
		google::SetLogDestination(google::GLOG_ERROR,"");
		//防止日志写入失败导致进程崩溃，保证日志系统稳定性
		signal(SIGPIPE,SIG_IGN);
	}
	~SetGlogLevel()
	{
		google::ShutdownGoogleLogging();
	}
};

void* func(void *argc)
{
	pthread_t id = pthread_self();
	LOG(INFO)<<"current thread id:"<<id;
	return NULL;
}

int main()
{
	//signal(SIGINT,SIG_IGN);//忽略终止信号
    SetGlogLevel glog(0);
	SipLocalConfig* config=new SipLocalConfig(); 
	int ret=config->ReadConf();
	if (ret==-1)
	{
		LOG(ERROR)<<"read config error";
		return ret;
	}
	bool re=GlobalCtl::instance()->init(config);
	if(re==false)
	{
		LOG(ERROR)<<"init error";
		return -1;
	}
	LOG(INFO)<<"sipIp-port:"<<GBOJ(gConfig)->sipIp()<<":"<<GBOJ(gConfig)->sipPort();
	LOG(INFO)<<"localIp-port:"<<GBOJ(gConfig)->localIp()<<":"<<GBOJ(gConfig)->localPort();
	pthread_t pid;
	ret=EC::ECThread::createThread(func,NULL,pid);//pid传入后，创新新线程的id将赋值给pid
	if (ret!=0)
	{
		ret=-1;
		LOG(ERROR)<<"create thread error";
		return ret;
	}
	LOG(INFO)<<"create thread pid:"<<pid;
	LOG(INFO)<<"main thread pid:"<<pthread_self();

	SipRegister* sipRegister=new SipRegister();
	sipRegister->registerServiceStart();

	while(true)
	{
		sleep(30);
	}
    return 0;
}