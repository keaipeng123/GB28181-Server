# PJSIP 学习笔记（围绕 `SipCore`）

> 对应工程代码：
>
> - `SipSupService/include/SipCore.h`
> - `SipSupService/src/SipCore.cpp`
> - `SipSupService/include/SipDef.h`
>
> 目标：
>
> 1) 搞清 PJSIP/PJLIB 的模块划分与“消息从哪来/到哪去”。
> 2) 能读懂 `SipCore::InitSip()` 的每一步初始化在做什么。
> 3) 知道如何注册自己的模块处理 SIP 消息，以及常见坑。

---

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


---

## 4. Transport 层：`init_transport_layer()` 做了什么？

关键点：

- `pj_sockaddr_in addr` 配好监听地址
  - `addr.sin_addr.s_addr = 0` 表示 0.0.0.0（监听所有网卡）
  - `addr.sin_port = pj_htons(sipPort)` 做端口字节序转换

- 启动 UDP：`pjsip_udp_transport_start(m_endpt, &addr, NULL, 1, NULL);`
- 启动 TCP：`pjsip_tcp_transport_start(m_endpt, &addr, 1, NULL);`

这样 endpoint 就能在该端口收发 SIP 报文。

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

---

## 6. 自定义模块：`recv_mod` 怎么工作？

### 6.1 `pjsip_module` 的核心要点

你定义了：

- 模块名：`{"mod-recv", 8}`
- 优先级：`PJSIP_MOD_PRIORITY_APPLICATION`
- 回调：只实现了 `onRxRequest`

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

### 8.3 多线程下的 PJLIB 线程注册

PJLIB 在多线程环境中通常要求线程在使用 PJLIB API 前做线程注册（具体是否需要取决于你调用路径）。

学习阶段先记结论：

- “PJSIP 能跑”不代表“线程模型正确”，多线程要特别谨慎。

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
