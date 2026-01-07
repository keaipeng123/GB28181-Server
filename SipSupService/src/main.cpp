#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include <glog/logging.h>
#include<signal.h>

#include"Common.h"
using namespace std;

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
    SetGlogLevel glog(0);
    cout << "Hello SipSupService!" << endl;
    LOG(INFO)<<"info log test";
    LOG(WARNING)<<"warning log test";
    LOG(ERROR)<<"error log test";
    return 0;
}