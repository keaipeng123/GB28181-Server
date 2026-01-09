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
	while(true)
	{
		sleep(30);
	}
    return 0;
}