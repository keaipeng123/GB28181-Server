# day2 学习笔记

## 1. 今日学习内容

- sip知识点整理
    - 语法库扩展： [sipregister](./pjsip/GB28181注册_REGISTER_SipRegister.md)
    - 语法库扩展： [sipcore](./pjsip/pjsip初始化_SipCore.md)
    - 语法库整理： [siptaskbase](./pjsip/SipTaskBase_业务任务基类.md)

- C++知识点整理
    - 智能锁： [AutoMutexLock](./C++和C语法整理/智能锁_RAII.md)
    - static: [static](./C++和C语法整理/static.md)

## 2. 功能实现

- 智能锁实现及下级注册列表加锁

- pj_caching_pool释放及pj_shutdown调用

- onRxRequest 上级接收下级注册请求并且解析

- 上级实现对非鉴权注册请求的具体业务处理和相应