# PJSIP 学习笔记（围绕 `SipCore`）

> 对应工程代码：
>
> - `SipSubService/include/SipCore.h`
> - `SipSubService/src/SipCore.cpp`
> - `SipSupService/include/SipCore.h`
> - `SipSupService/src/SipCore.cpp`
> - `SipSubService/include/SipDef.h`
>
> 目标：
>
> 1) 搞清 PJSIP/PJLIB 的模块划分与“消息从哪来/到哪去”。
> 2) 能读懂 `SipCore::InitSip()` 的每一步初始化在做什么。
> 3) 知道如何注册自己的模块处理 SIP 消息，以及常见坑。

这份笔记以工程当前实现为准（尤其是 `pollingEvent()`、`recv_mod`、`init_transport_layer()` 的写法），用来回答三个实用问题：

1) 这个服务是如何把 PJSIP 栈“拉起来并持续跑起来”的？
2) 收到 SIP 报文后，回调会走到哪里？我在 `onRxRequest()` 里能拿到哪些信息？
3) 现有实现有哪些“能跑但不够工程化”的点（退出、线程、资源释放）？

---

## 0. `SipCore` 一眼看懂（先抓住工程结构）

`SipCore` 目前就是一个“最小 SIP 栈启动器”，核心成员只有两个：

- `pjsip_endpoint* m_endpt`：SIP 栈的核心句柄（模块挂载、transport、事件循环都围绕它）
- `pj_caching_pool m_cachingPool`：PJSIP 全栈长期使用的缓存池（必须与 `SipCore` 同寿命）

对外接口也很简单：

- `bool InitSip(int sipPort)`：完成 PJLIB/PJSIP 初始化 + 开 transport + 注册模块 + 启事件线程
- `pjsip_endpoint* GetEndPoint()`：给上层拿 endpoint（例如后续发消息/建事务可能会用）

## 1. PJSIP 家族模块速览（你需要先有一张“地图”）

### 1.1 PJLIB（`pjlib.h`）：地基

- 作用：线程/锁、计时器、内存池、socket 抽象、日志、异常处理等基础能力
- 典型入口：`pj_init()`

你可以把它理解为：PJSIP 的“运行时环境”。PJLIB 没初始化成功，后面所有组件都没法用。

### 1.2 PJLIB-UTIL（`pjlib-util.h`）：工具箱

- 作用：MD5/SHA1、DNS、XML、STUN/DNS 等工具能力
- 典型入口：`pjlib_util_init()`

### 1.3 PJSIP 核心（`pjsip.h` / `sip_endpoint.h` / `sip_transport.h` 等）

你在描述中提到的“核心库”重点主要包含：

- **SIP Endpoint（端点，核心句柄）**：`pjsip_endpoint`
  - 它是 SIP 栈的“总控中心”，模块挂载、事件循环、定时器、收发调度都围绕它
  - 创建：`pjsip_endpt_create(...)`

- **模块系统（pjsip_module）**：
  - 你在工程中定义了一个接收模块 `recv_mod`
  - 每个模块都有回调，例如 `on_rx_request` / `on_rx_response`
  - 模块有 **priority**（优先级）决定处理顺序

- **Transport 层（UDP/TCP/TLS）**：
  - 负责“网络收发”
  - 例如 `pjsip_udp_transport_start()` / `pjsip_tcp_transport_start()`

### 1.4 事务层（Transaction Layer，`sip_transaction.h`）

- 作用：把“收/发的 SIP 报文”组织成一个个事务（Transaction）
  - 创建/销毁事务
  - 状态机（Trying/Proceeding/Completed/Terminated 等）
  - 超时、重传、匹配响应到请求、ACK/CANCEL 相关处理
- 工程初始化：`pjsip_tsx_layer_init_module(m_endpt);`

> 你可以把事务层理解成：SIP 的“可靠性与状态机”。

### 1.5 UA / Dialog（会话层：`sip_dialog.h`、`pjsip_ua.h`）

- **UA 层（User Agent）**：在事务层之上，面向“呼叫/会话”的更高层逻辑
- **Dialog（会话）**：维护会话状态（Call-ID、From/To tag、Route set、CSeq 等）
  - 管理会话生命周期

工程初始化：`pjsip_ua_init_module(m_endpt, NULL);`

> 你的描述里把 `sip_dialog.h` 称为“会话模块”，这个理解是对的：它是 SIP 会话状态的核心载体。

### 1.6 pjnath（NAT 穿越）

- 作用：STUN/TURN/ICE 等 NAT 穿越能力
- 常见用途：公网/内网互通，获取映射地址，打洞，媒体通道建立

### 1.7 pjmedia（媒体处理）

- 作用：RTP/RTCP、音视频设备、编解码、抖动缓冲、回声消除等
- 在 GB28181 场景里：媒体协商/SDP、RTP 推流/拉流等通常会用到

---

## 2. 模块处理顺序：发送 vs 接收（结合你的描述）

你给出的总结非常关键（建议背下来）：

- **发送（TX）**：从应用层到传输层
  - 模块处理顺序：按 module priority “从大到小”处理（你的描述）

- **接收（RX）**：从传输层到应用层
  - 模块处理顺序：按 module priority “从小到大”处理（你的描述）

工程里 `recv_mod` 设置了：

- `PJSIP_MOD_PRIORITY_APPLICATION`（应用层优先级）

实际学习时你可以记一个更直观的模型：

- RX：网络进来 → transport → transaction → UA/dialog → 你的应用模块
- TX：你的应用模块 → UA/dialog → transaction → transport → 网络出去

补充一条更“贴近工程现状”的观察：

- 你在 `recv_mod` 里只实现了 `on_rx_request`，并且当前返回值等价于“不拦截”。因此收到请求后，通常会继续向后交给其它模块/默认处理路径。

---

## 3. 围绕 `SipCore`：初始化流程逐行拆解

### 3.1 `SipCore` 类的定位

`SipCore` 目前干了三件事：

1) 初始化 PJLIB/PJSIP
2) 创建 endpoint + 初始化事务层/UA 层
3) 启动 UDP/TCP 监听，并启动事件轮询线程（`pollingEvent`）

### 3.2 `SipCore::InitSip(int sipPort)` 主流程

初始化顺序（与你代码一致）：

1) `pj_log_set_level(6)`：设置 PJLIB 日志等级（0 关，越大越详细）
2) `pj_init()`：初始化 PJLIB
3) `pjlib_util_init()`：初始化 pjlib-util
4) `pj_caching_pool_init(...)`：初始化 caching pool
   - `SipDef.h` 里：`SIP_STACK_SIZE = 1024*256`
5) `pjsip_endpt_create(...)`：创建 endpoint（得到 `m_endpt`）
6) `pjsip_tsx_layer_init_module(m_endpt)`：初始化事务层模块
7) `pjsip_ua_init_module(m_endpt, NULL)`：初始化 UA 层
8) `init_transport_layer(sipPort)`：启动 UDP/TCP transport 监听端口
9) `pjsip_endpt_register_module(m_endpt, &recv_mod)`：注册你自定义的接收模块
10) `pjsip_endpt_create_pool(...)`：给后续线程/对象创建长期 pool
    - `SipDef.h` 里：`SIP_ALLOC_POOL_1M = 1MB`
11) `pj_thread_create(..., pollingEvent, m_endpt, ..., &eventThread)`：启动事件轮询线程

两个“代码细节”建议在脑子里记牢：

- 这段初始化是用 `do { ... } while(0)` 包起来的：一旦某步失败 `break`，最后用 `status` 决定返回值。
- 事件线程使用的是 `pjsip_endpt_create_pool()` 创建的 pool：只要 endpoint 活着，这个 pool 也应该一直活着。


---

## 4. Transport 层：`init_transport_layer()` 做了什么？

关键点：

- `pj_sockaddr_in addr` 配好监听地址
  - `addr.sin_addr.s_addr = 0` 表示 0.0.0.0（监听所有网卡）
  - `addr.sin_port = pj_htons(sipPort)` 做端口字节序转换

- 启动 UDP：`pjsip_udp_transport_start(m_endpt, &addr, NULL, 1, NULL);`
- 启动 TCP：`pjsip_tcp_transport_start(m_endpt, &addr, 1, NULL);`

这样 endpoint 就能在该端口收发 SIP 报文。

结合参数再解释一次：

- `addr` 绑定 `0.0.0.0:sipPort`：表示监听本机所有网卡
- UDP 的 `async_cnt=1`：给 UDP transport 准备一个异步接收 worker（最小配置）
- TCP 的 `async_cnt=1`：类似含义（最小配置），具体实现由 PJSIP 版本决定

---

## 5. 事件循环线程：为什么必须要 `pjsip_endpt_handle_events()`？

你在 `pollingEvent()` 里做的是：

```cpp
while (true) {
    pj_time_val timeout = {0, 500};
    pjsip_endpt_handle_events(ept, &timeout);
}
```

这一步非常关键：

- SIP 栈的 IO、定时器、事务重传、模块回调调度，很多都依赖这个事件循环推进
- 没有这个循环：
  - 可能收不到消息
  - 定时器不跑
  - 事务超时/重传不工作

可以把它理解为：PJSIP 的“心跳/调度器”。

对照你的实现再补充两点工程化理解：

1) `timeout={0,500}` 是 500ms（`pj_time_val` 的第二个字段是毫秒），这会影响“收到消息到回调触发”的最坏延迟，以及 CPU 占用。
2) 当前代码把任何 `PJ_SUCCESS != status` 都当成错误并退出线程；但有些 API 在超时/无事件时也可能返回特定状态码。更稳妥的写法通常是：识别“可忽略”的返回码（例如超时），不要把它当致命错误。

---

## 6. 自定义模块：`recv_mod` 怎么工作？

### 6.1 `pjsip_module` 的核心要点

你定义了：

- 模块名：`{"mod-recv", 8}`
- 优先级：`PJSIP_MOD_PRIORITY_APPLICATION`
- 回调：只实现了 `onRxRequest`

把 `recv_mod` 结构体按字段“对照代码”写清楚（方便以后你加回调时不迷路）：

- `NULL, NULL`：模块链表的 prev/next（由 PJSIP 管）
- `{"mod-recv", 8}`：模块名（注意：长度要匹配字符串长度）
- `-1`：模块 id（注册时由 PJSIP 分配）
- `PJSIP_MOD_PRIORITY_APPLICATION`：模块优先级
- 后面四个 `NULL`：load/start/stop/unload（当前不需要生命周期钩子）
- `onRxRequest`：接收请求（REGISTER/INVITE/MESSAGE/...）
- 其余 `NULL`：接收响应、发送前回调、事务状态变化等（当前不实现）

`onRxRequest(pjsip_rx_data* rdata)` 的意义：

- 当栈收到 SIP 请求报文（REGISTER/INVITE/MESSAGE/BYE/...）时
- 你能在这里拿到 `rdata`，解析报文，决定如何处理

### 6.2 `on_rx_request` 的返回值语义（学习重点）

它的返回类型是 `pj_bool_t`，通常表示：

- 返回 “真/假” 来告诉 PJSIP：这个请求是否被该模块处理并截断/继续传递

你当前代码里写了：

```cpp
pj_bool_t onRxRequest(pjsip_rx_data *rdata)
{
    return PJ_SUCCESS;
}
```

从学习角度建议你记住：

- `PJ_SUCCESS` 是 `pj_status_t` 的成功码概念
- 而 `pj_bool_t` 语义一般用 `PJ_TRUE`/`PJ_FALSE`（或 1/0）表达

（这点在“编译可过但语义不清晰”时特别容易迷糊。）

结合你这里的实际返回值再说一句“工程事实”：

- `PJ_SUCCESS` 的数值通常是 0，因此它在 `pj_bool_t` 上等价于 `PJ_FALSE`。
- 也就是说，你当前实现实际上表示：**我不处理/不截断这个请求，继续让其它模块处理**。

更建议你写成更清晰的形式（语义不变）：

```cpp
pj_bool_t onRxRequest(pjsip_rx_data *rdata)
{
  PJ_UNUSED_ARG(rdata);
  return PJ_FALSE;
}
```

如果你在这个回调里已经“发送了最终响应”并希望阻止后续模块重复处理，则通常返回 `PJ_TRUE`。

### 6.3 在 `onRxRequest()` 里你能拿到什么？（最小信息提取）

`pjsip_rx_data* rdata` 里最常用的是 `rdata->msg_info`：

- 方法（REGISTER/INVITE/...）：`rdata->msg_info.msg->line.req.method`
- Request-URI：`rdata->msg_info.msg->line.req.uri`
- Call-ID：`rdata->msg_info.cid`
- CSeq：`rdata->msg_info.cseq`
- From/To/Via：`rdata->msg_info.from` / `to` / `via`

写“按方法分发”时，典型代码形状是：

```cpp
pjsip_msg* msg = rdata->msg_info.msg;
if (msg && msg->type == PJSIP_REQUEST_MSG) {
  const pjsip_method& m = msg->line.req.method;
  // m.id / m.name 可用于判断方法
}
```

### 6.4 最小 stateless 回复（学习用）

你后面做 GB28181 时很快会需要“收到请求，直接回个 200/401/403”。
在 PJSIP 里这通常走 stateless response：从 `rdata` 创建 `tdata`，然后 send。

（提示：具体 API 名在不同版本/封装里会略有差别，但核心思路是：create_response → send_response。）

---

### 6.5 上下级差异：SUB vs SUP 的 `onRxRequest()` 与“任务分发”

虽然上下级两侧都注册了同名的 `recv_mod`（模块名 `mod-recv`），但 **接收回调实现差异很大**：

#### 6.5.1 下级（SipSubService）侧：当前是“空实现/不处理”

对应 [SipSubService/src/SipCore.cpp](../../SipSubService/src/SipCore.cpp)：

- `onRxRequest()` 直接 `return PJ_SUCCESS;`
- 由于 `PJ_SUCCESS` 通常是 0，它在 `pj_bool_t` 上等价于 `PJ_FALSE`
- 实际含义：**该模块不处理请求，让请求继续向后传递给其它模块**

这通常意味着：SUB 侧更多是“主动发起请求（例如 REGISTER client）”，而不是在 `onRxRequest()` 里做业务。

#### 6.5.2 上级（SipSupService）侧：按方法创建任务类 + 开线程处理

对应 [SipSupService/src/SipCore.cpp](../../SipSupService/src/SipCore.cpp)：

SUP 侧 `onRxRequest()` 做了一个最小的“路由器/分发器”：

1) 判空：`rdata` / `rdata->msg_info.msg`
2) 克隆消息：`pjsip_rx_data_clone(rdata, 0, &param->data)`
  - 目的：把 `rdata` 的内容复制一份，交给业务线程异步处理
3) 按方法选择业务：
  - `if (msg->line.req.method.id == PJSIP_REGISTER_METHOD) param->base = new SipRegister();`
4) 启线程处理：`ECThread::createThread(SipCore::dealTaskThread, param, pid)`
  - 每个请求一个线程（学习阶段简单直接，但高并发下会有开销，后续可替换成线程池/队列）

这里还有一个非常关键的工程点：

- 既然你已经“接管”了该请求，并会在业务线程里回复响应，一般更合理的返回值是 `PJ_TRUE`（表示本模块已处理/已接管，避免其它模块重复处理同一请求）。
- 目前代码返回的是 `PJ_SUCCESS`（通常为 0），从语义上更像 `PJ_FALSE`。

是否一定要改成 `PJ_TRUE` 取决于你整体模块链路是否还有其它模块会处理同一个 REGISTER；但**写笔记时建议把这个语义点记牢**。

---

### 6.6 `pjsip_rx_data_clone()` / `pjsip_rx_data_free_cloned()`：异步处理的“必要动作”

SUP 侧之所以要 clone，是因为：

- `onRxRequest()` 运行在 PJSIP 的接收线程/事件线程上下文里
- `rdata` 的生命周期通常由栈内部管理，回调返回后不保证继续有效

所以正确做法是：

- 回调里 clone 一份 `pjsip_rx_data`
- 业务线程处理完后释放 clone

在 SUP 侧 [SipSupService/include/SipCore.h](../../SipSupService/include/SipCore.h) 里用 `threadParam` 的析构函数统一做了收尾：

- `delete base;`（释放业务任务对象）
- `pjsip_rx_data_free_cloned(data);`（释放 clone 的 rdata）

这也是为什么业务线程末尾只需要 `delete param;`。

### 6.6.1 `_threadParam`/`threadParam` 的作用：跨线程的“任务上下文 + 统一回收”

SUP 侧 [SipSupService/include/SipCore.h](../../SipSupService/include/SipCore.h) 定义了：

- `_threadParam`：结构体本体
- `threadParam`：`typedef` 的别名

它的定位可以理解为：**把一次收到的 SIP 请求，打包成一个“可在线程间传递”的任务对象**。

这个任务对象里装了两类关键资源：

1) `SipTaskBase* base`：
  - 指向“具体业务处理类”（例如 REGISTER 就是 `new SipRegister()`）
  - 通过多态 `base->run(...)` 执行业务

2) `pjsip_rx_data* data`：
  - `pjsip_rx_data_clone()` 得到的克隆报文
  - 用于业务线程里安全地解析/回复（避免使用回调返回后可能失效的原始 `rdata`）

为什么要建立这个结构体？主要解决 3 个工程问题：

- **跨线程传参**：`onRxRequest()` 里创建任务、clone 报文，然后把这一包参数丢给 `ECThread::createThread()`；线程入口只接收一个 `void*`，因此需要一个“打包结构”。
- **明确资源所有权**：谁负责 `delete base`、谁负责 `pjsip_rx_data_free_cloned`？都放进 `threadParam` 的析构函数里统一处理，规则清晰。
- **异常/失败路径不泄漏**：例如线程创建失败时，代码会 `delete param;`，会自动触发析构把已创建的 `base` 与已 clone 的 `data` 一并释放。

一句话总结：`threadParam` 就是这套“收到请求 → 分发到业务线程”的承载体，它把 **业务对象 + 报文副本** 绑在一起，并用 RAII（析构回收）保证任何路径下都不泄漏。

---

### 6.7 外部线程使用 PJSIP：必须做 PJLIB 线程注册

SUP 侧的业务线程是用 `ECThread::createThread()` 创建的（不是 PJLIB/PJSIP 创建的线程）。

PJLIB 要求：**外部线程在调用任何 PJLIB/PJSIP API 之前必须注册**，否则可能触发断言：

`Assertion failed: !"Calling PJLIB from unknown/external thread"`

工程里通过 `pjcall_thread_register()` 封装了这一步：

- 定义在 [SipSupService/include/GlobalCtl.h](../../SipSupService/include/GlobalCtl.h)
- 内部调用 `pj_thread_is_registered()` / `pj_thread_register()`

因此在 [SipSupService/src/SipCore.cpp](../../SipSupService/src/SipCore.cpp) 的 `dealTaskThread()` 开头会先：

- `pj_thread_desc desc;`
- `pjcall_thread_register(desc);`

另外你还能在 [SipSupService/src/TaskTimer.cpp](../../SipSupService/src/TaskTimer.cpp) 里看到同样的注册动作，说明：**只要线程不是 PJLIB 创建的，但又要调用 PJLIB/PJSIP，就应该先注册**。

---

## 7. 结合工程：`GlobalCtl` 如何拉起 `SipCore`

在 `GlobalCtl::init()` 中：

- `gSipServer = new SipCore();`
- `gSipServer->InitSip(gConfig->sipPort());`

这意味着 `SipCore` 是整个服务启动 SIP 栈的入口。

---

## 8. 学习过程中必须注意的几个“工程级坑”（强烈建议记在脑子里）

### 8.1 `pj_caching_pool` 生命周期问题（你代码里已经写了提示）

`SipCore::InitSip()` 里：

- `pj_caching_pool cachingPool;` 是**栈对象**
- 函数返回后它会销毁

但 `pjsip_endpt_create(&cachingPool.factory, ..., &m_endpt)` 会把这个 factory 用于后续内存分配。

所以如果 caching pool 生命周期不够长，后面就可能出严重问题。

> 你代码注释里也写了“cachingPool 需要和 sipCore 对象生命周期一致，所以放在类成员中”，这是正确方向。

### 8.2 事件线程的退出/销毁顺序

当前 `pollingEvent()` 是死循环，且线程句柄 `eventThread` 没有保存到成员里。

如果 `SipCore` 析构里 `pjsip_endpt_destroy(m_endpt);` 先发生：

- 事件线程仍在跑、还在用 `ept`
- 容易出现 use-after-free（线程访问已销毁的 endpoint）

学习时要牢记一个通用规则：

- **先让事件线程停止并退出**
- 再销毁 endpoint / pool / caching pool

并且对照你当前实现，问题点更具体是：

- `pollingEvent()` 是 `while(true)`，没有任何退出条件
- `eventThread` 没有保存成成员，也没有 join/detach 管理
- 析构函数里直接 `pjsip_endpt_destroy(m_endpt);`：极易导致线程还在跑、endpoint 已释放

### 8.3 析构与全局初始化/反初始化

你当前 `SipCore::~SipCore()` 只有：

- `pjsip_endpt_destroy(m_endpt);`

从“资源生命周期完整性”看，还缺少至少两类收尾动作（是否要做取决于你是否需要优雅退出）：

1) 停止事件线程（让 `pollingEvent` 退出，并等待线程结束）
2) 销毁 caching pool：`pj_caching_pool_destroy(&m_cachingPool)`

另外，如果你把 PJLIB/PJSIP 当“整个进程只初始化一次”的全局设施，通常在进程退出时还会有 `pj_shutdown()` 之类的收尾（具体取决于你工程的初始化策略）。

### 8.3 多线程下的 PJLIB 线程注册

PJLIB 在多线程环境中通常要求线程在使用 PJLIB API 前做线程注册（具体是否需要取决于你调用路径）。

学习阶段先记结论：

- “PJSIP 能跑”不代表“线程模型正确”，多线程要特别谨慎。

这里结合你的代码给一个更具体的提醒：

- 事件线程里会调用 PJSIP API（`pjsip_endpt_handle_events`），如果后续你在其它业务线程里也调用 PJSIP API，需要确认 PJLIB 的线程注册/锁策略是否满足要求。

---

## 9. 基本使用套路（你可以按这个顺序写自己的功能）

### 9.1 初始化（你已经做了）

- `pj_init()` → `pjlib_util_init()` → `pjsip_endpt_create()`
- 初始化事务层/UA 层
- 启动 transport
- 注册自定义 module
- 启动事件循环线程

### 9.2 接收消息（下一步要做的）

- 在 `onRxRequest(pjsip_rx_data* rdata)`：
  - 判断方法类型（REGISTER/INVITE/MESSAGE/...）
  - 解析头域（From/To/Call-ID/CSeq/Via）
  - 选择：
    - 直接 stateless 回复（例如 200 OK/401/403）
    - 进入事务/会话逻辑（更完整的呼叫流程）

### 9.3 发送请求（后续学习方向）

常见路线：

- 构造请求报文（request line + headers + body/SDP）
- 通过 endpoint/UA/transaction 发送（具体选哪条 API 取决于你是 stateless 还是 stateful）

（你当前 `SipCore` 还没涉及“构造与发送 SIP 请求”，后续可以再按实际代码补一节。）

---

## 10. 建议你下一步怎么学（围绕 GB28181 更贴近业务）

1) 先把 `onRxRequest` 做成“按方法分发”的路由：REGISTER / MESSAGE / INVITE / BYE
2) 先实现最简单的 stateless 回复（比如 REGISTER 的 200 OK 或 401 challenge）
3) 再补“事务层/会话层”完整呼叫流程（INVITE 的对话建立与状态维护）
4) 进入媒体：SDP 解析 + pjmedia RTP 发送/接收
5) 如果有公网/内网：再引入 pjnath（ICE/TURN）
