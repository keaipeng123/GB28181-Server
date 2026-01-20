#ifndef _ECTHREAD_H
#define _ECTHREAD_H
#include <pthread.h>
#include<sys/prctl.h>
#include<string>
namespace EC
{
    typedef void* (*ECThreadFunc)(void*);//函数指针
    /*| 片段                             | 含义                                       |
| ------------------------------ | ---------------------------------------- |
| `(*ECThreadFunc)`              | `ECThreadFunc` 是一个指针（`*`）                |
| `(*ECThreadFunc)(void*)`       | 这个指针指向 **一个函数**，该函数只有一个形参，类型为 `void*`    |
| `void* (*ECThreadFunc)(void*)` | 该函数返回 `void*`                            |
| `typedef`                      | 把上述整个“函数指针”类型重新命名为一个新的类型名 `ECThreadFunc` |
*/
    //该类无需对外提供实例，方法均为static对外提供操作
    class ECThread
    {
        public:
        static int createThread(ECThreadFunc startRoutine,void* args,pthread_t& id);//创建线程:在一个线程内创建一个子线程
        static int detachSelf();//分离线程：在一个线程调用后表示将自身分离，当前线程任务结束后可自动释放资源，无需其他线程调用pthread销毁来回收
        static void exitSelf(void* rval);//退出线程：在一个线程内调用退出当前线程,返回退出码
        static int waitThread(const pthread_t& id,void** rval);//阻塞等待指定线程退出，不允许修改id所以用const,并拿到其“退出码”rval
        static int terminateThread(const pthread_t& id);//向指定线程发出退出信号
        private:
        ECThread(){}
        ~ECThread(){}
        //由于不对外提供一个实例，所以无需对拷贝构造以及复制运算符重载私有化
    };
}
#endif