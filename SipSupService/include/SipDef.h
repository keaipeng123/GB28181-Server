#ifndef _SIPDEF_H
#define _SIPDEF_H
#include<string>
#include<string.h>
#define SIP_STACK_SIZE 1024*256
#define SIP_ALLOC_POOL_1M 1024*1024*1

enum statusCode
{
    SIP_SUCCESS=200,
    SIP_BADREQUEST=400,//请求的参数或者格式不对，请求非法
    SIP_FORBIDDEN=404,//请求的用户在本域中不存在
    
};

#endif