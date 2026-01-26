#ifndef _SIPMESSAGE_H
#define _SIPMESSAGE_H
#include"Common.h"
#include"GlobalCtl.h"
class SipMessage
{
    public:
    SipMessage();
    ~SipMessage();
    void setFrom(char* fromUsr,char* fromIp);
    void setTo(char* toUser,char* toIp);
    void setUrl(char* sipId,char* url_ip,int url_port,char* url_proto=(char*)"udp");
    void setContact(char* sipId,char* natIp,int natPort);

    inline char* FromHeader(){return fromHeader;}
    inline char* ToHeader(){return toHeader;}
    inline char* RequestUrl(){return requestUrl;}
    inline char* Contact(){return contact;}

    private:
    char fromHeader[128];
    char toHeader[128];
    char requestUrl[128];
    char contact[128];

};
#endif