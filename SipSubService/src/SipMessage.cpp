#include"SipMessage.h"
SipMessage::SipMessage()
{
    //把变量（或数组）contact 所占的全部内存字节都设为 0，即“彻底清零”
    memset(fromHeader,0,sizeof(fromHeader));
    memset(toHeader,0,sizeof(toHeader));
    memset(requestUrl,0,sizeof(requestUrl));
    memset(contact,0,sizeof(contact));
}
SipMessage::~SipMessage()
{

}
void SipMessage::setFrom(char* fromUsr,char* fromIp)
{
    sprintf(fromHeader,"<sip:%s@%s>",fromUsr,fromIp);
}
void SipMessage::setTo(char* toUser,char* toIp)
{
    sprintf(toHeader,"<sip:%s@%s>",toUser,toIp);
}
void SipMessage::setUrl(char* sipId,char* url_ip,int url_port,char* url_proto)
{
    sprintf(requestUrl,"sip:%s@%s:%d;transport=%s",sipId,url_ip,url_port,url_proto);
}
void SipMessage::setContact(char* sipId,char* natIp,int natPort)
{
    sprintf(contact,"sip:%s@%s:%d",sipId,natIp,natPort);
}