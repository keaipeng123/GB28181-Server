# GB28181 注册（REGISTER）学习笔记（围绕 `SipRegister`）

> 对应工程代码：
>
> - [SipSubService/include/SipRegister.h](../../SipSubService/include/SipRegister.h)
> - [SipSubService/src/SipRegister.cpp](../../SipSubService/src/SipRegister.cpp)
> - [SipSupService/include/SipRegister.h](../../SipSupService/include/SipRegister.h)
> - [SipSupService/src/SipRegister.cpp](../../SipSupService/src/SipRegister.cpp)
> - [SipSubService/include/SipMessage.h](../../SipSubService/include/SipMessage.h)
> - [SipSubService/src/SipMessage.cpp](../../SipSubService/src/SipMessage.cpp)
>
> 前置建议先读：
>
> - [notes/pjsip/pjsip初始化_SipCore.md](pjsip初始化_SipCore.md)
>
> 目标：
>
> 1) 看懂下级（SUB）如何用 PJSIP 发起 REGISTER（客户端）。
> 2) 看懂上级（SUP）如何处理收到的 REGISTER 并回复响应（服务端）。
> 3) 明确 `From/To/Request-URI/Contact/Expires` 在 GB28181 注册里的含义。
> 4) 知道回调/响应链路、线程与并发风险，以及常见坑。

---

## 1. `SipRegister` 在工程里的定位

`SipRegister` 负责“向上级平台（SUP）注册”。它的行为很直接：

本工程里 REGISTER 相关代码分两侧：

- **SipSubService（下级/客户端）**：主动向上级发 REGISTER，并在回调里根据响应码更新 `registered`。
- **SipSupService（上级/服务端）**：接收来自下级的 REGISTER 请求，校验、记录 `Expires`，并回 200/403 等响应。

两边的名字都叫 `SipRegister`，但职责不同：

- SUB 侧：更像“注册发起器（client regc）”。
- SUP 侧：是一个业务处理任务类，继承自 [SipTaskBase_业务任务基类.md](SipTaskBase_业务任务基类.md)（通过 `run()` 处理收到的 `pjsip_rx_data*`）。

### 1.1 定时轮询式注册（当前代码的真实行为）

当前实现并不是“构造函数里立刻遍历并注册”。在 [SipSubService/src/SipRegister.cpp](../../SipSubService/src/SipRegister.cpp) 里，构造函数中那段遍历注册逻辑已经被注释掉。

现在的实际流程是：

1) `SipRegister` 构造函数里创建一个 `TaskTimer(3)`（每 3 秒触发一次）。
2) 调用 `registerServiceStart()` 后：
   - `m_regTimer->setTimerFun(SipRegister::RegisterProc, this)`
   - `m_regTimer->start()` 启动定时线程
3) 定时线程每次触发 `RegisterProc()`：
   - 遍历 `GlobalCtl::instance()->getSupDomainInfoList()`
   - 对所有 `registered == false` 的节点调用 `gbRegister(node)`
4) 当收到注册响应时（异步）：在 `client_cb()` 里根据响应码更新 `node.registered=true`。

> 这种模式的好处是简单粗暴：没注册上就一直重试。
> 但要注意“并发与重复发送”的工程风险，下面会单独讲。

> 这里的 `GlobalCtl::SupDomainInfo` 可以理解为“一个上级平台的注册目标”，里面通常会有：对端 `addrIp/sipPort`、SIP ID、transport、expires、以及 `registered` 状态。

### 1.2 上下级对照：REGISTER 的“请求-响应”闭环

把 SUB 与 SUP 侧放在一起看，REGISTER 的闭环就是：

1) SUB：定时触发 → 遍历上级列表 → 对未注册的 `SupDomainInfo` 发送 REGISTER
2) 网络：REGISTER 报文到达 SUP
3) SUP：解析 `From`/`Expires` → 判断是否允许注册 → 回 SIP 响应（200/403）
4) 网络：响应到达 SUB
5) SUB：`client_cb()` 回调拿到响应码 → 200 则置 `registered=true`

只要事件循环线程（`pjsip_endpt_handle_events()`）在跑，上述回调/响应就能推进。

---

## 2. 注册关键字段：From / To / Request-URI / Contact

在 [SipSubService/src/SipRegister.cpp](../../SipSubService/src/SipRegister.cpp) 里，`gbRegister()` 会借助 `SipMessage` 拼 4 个字符串（见 [SipSubService/include/SipMessage.h](../../SipSubService/include/SipMessage.h)、[SipSubService/src/SipMessage.cpp](../../SipSubService/src/SipMessage.cpp)）：

1) `fromHeader`：`<sip:本端ID@本端域>`
2) `toHeader`：`<sip:本端ID@本端域>`（REGISTER 场景里通常与 From 一致）
3) `requestUrl`：`sip:对端ID@对端IP:对端端口;transport=udp|tcp`
4) `contactUrl`：`sip:本端ID@本端IP:本端端口`

对应到 SIP REGISTER 的语义：

- `From/To`：声明“谁在注册”（AOR，Address-Of-Record），GB28181里通常形如 `sip:设备ID@域ID`。
  - 你当前代码用的是 `sipId@sipIp`。
- `Request-URI`：发给谁（Registrar 的地址）。你这里发给 `node.addrIp:node.sipPort`，并加了 `transport=` 参数。
- `Contact`：注册成功后，对端以后“如何联系到你”（你实际可接收请求的地址/端口）。
- `Expires`：有效期（秒），来自 `node.expires`。

### 2.1 `SipMessage` 是做什么的？（只管拼字符串）

`SipMessage` 目前只负责拼 4 个字段字符串，并不直接“发 SIP”——发送仍由 PJSIP 的 `pjsip_regc` 完成。

它的实现特点：

- 内部用 4 个 `char[128]` 缓冲区保存结果：`fromHeader/toHeader/requestUrl/contact`
- 构造函数里用 `memset` 全部清零
- 用 `sprintf` 拼接（学习阶段够用，但工程上建议改成 `snprintf` 防止越界）

### 2.2 TCP/UDP 选择逻辑

`gbRegister()` 里：

- 当 `node.protocal == 1`：`Request-URI` 会加 `;transport=tcp`
- 否则：默认 `udp`

这对应 [SipSubService/src/SipMessage.cpp](../../SipSubService/src/SipMessage.cpp) 里 `setUrl(..., url_proto)` 的默认参数是 `"udp"`。

### 2.3 `pj_str_t` 是什么？（以及你这里的生命周期是否安全）

在 `gbRegister()` 里，你把 `SipMessage` 生成的 C 字符串包成了 `pj_str_t`：

- `pj_str_t from = pj_str(msg.FromHeader());`
- `pj_str_t to = pj_str(msg.ToHeader());`
- `pj_str_t line = pj_str(msg.RequestUrl());`
- `pj_str_t contact = pj_str(msg.Contact());`

`pj_str_t` 可以理解为：`{char* ptr, int len}` 的轻量视图，不一定要求以 `\0` 结尾；你的字符串本身是 `sprintf` 生成的，因此天然有 `\0`。

生命周期要点：

- 这些 `pj_str_t` 指向 `msg` 的成员缓冲区，而 `msg` 是栈对象。
- 如果某个 PJSIP API 只是“保存指针延迟使用”，就会悬空；如果它在调用时“解析/拷贝到自己的 pool”，就没问题。

经验上：`pjsip_regc_init()` 通常会立刻解析并存储所需字段，所以多数情况下可用。但当你后续遇到“偶现解析失败/野指针”时，要第一时间想到这里。

### 2.4 REGISTER 里 `To` vs `Contact`：别混

你代码里 `From/To` 都设置为“本端 ID@本端地址”。在 REGISTER 语义里：

- `From/To` 更像是声明“我要注册的 AOR 是谁”（注册身份）
- `Contact` 才是“注册成功后对端如何联系到我”（可达地址）

所以如果你想表达“上级平台应该把请求发到哪里”，核心字段通常是 `Contact`（以及 NAT 场景下的可达性策略），而不是 `To`。

> 小建议：学习时可以抓包对照字段（Wireshark 过滤 `sip.Method == "REGISTER"`），把这 4 个字段逐项对应。

---

## 3. PJSIP 注册客户端 `pjsip_regc` 的调用流程

你这份实现走的是 PJSIP 提供的“注册客户端”接口：

1) `pjsip_regc_create(endpt, token, cb, &regc)`
   - `endpt`：由 `SipCore` 初始化出来的 endpoint（工程里通过 `gSipServer->GetEndPoint()` 获取）
   - `token`：你传了 `&node`，用于在回调里识别“这是哪个上级平台的注册结果”
   - `cb`：注册结果回调（你的 `client_cb`）

2) `pjsip_regc_init(regc, request_uri, from, to, contact_cnt, contact, expires)`
   - 这一步把 REGISTER 需要的基本信息绑定到 regc 上

3) `pjsip_regc_register(regc, PJ_TRUE, &tdata)`
   - 构造一个 REGISTER 请求（`tdata`）

4) `pjsip_regc_send(regc, tdata)`
   - 交给 PJSIP 栈发送；响应会异步回来触发回调

**重要前提**：事件循环必须在跑。

- 如果没有 `pjsip_endpt_handle_events()` 的轮询线程（见 [notes/pjsip/pjsip初始化_SipCore.md](pjsip初始化_SipCore.md) 的事件循环部分），你可能“发得出去，但收不到响应/回调不触发”。

---

## 4. 注册结果回调 `client_cb`：如何拿到结果

你的回调是一个 `static` 函数（这是对的：PJSIP 的 C 回调签名不带 `this`）：

- `param->code`：SIP 响应码（你只判断了 `200`）
- `param->token`：`pjsip_regc_create()` 时传入的 `token`

工程里用法：

- 将 `param->token` 强转回 `GlobalCtl::SupDomainInfo*`
- `code == 200` 则 `registered = true`

补充两个非常关键的“工程约束”：

1) `token` 指向的对象必须在回调触发时仍然有效。
   - 你这里传的是容器元素的地址（`&node`）。如果容器是 `std::list` 并且元素不被删除，通常是稳定的；但如果容器是 `vector` 或存在元素 erase/realloc，就可能悬空。
2) 回调线程与定时线程并发：
   - 回调通常在 PJSIP 的事件线程触发
   - `RegisterProc()` 在定时线程触发
   - 它们可能同时读写 `node.registered`，如果没有锁/原子，就存在数据竞争风险（学习阶段先认识这个风险就够了）。

学习建议：

- 只看 `200` 会漏掉很多有价值的状态（例如 `401/407` 鉴权挑战，`403` 禁止，`404` 用户不存在，`408` 超时，`503` 服务不可用等）。
- 后续你想把注册做“真正可用”，通常需要处理 `401/407`（带认证再发一次 REGISTER）。

---

## 5. 常见坑（结合当前实现）

### 5.0 轮询重试可能导致“重复注册风暴”

由于 `RegisterProc()` 每 3 秒遍历一次，且判断条件只有 `registered == false`：

- 如果上级响应慢/丢包/需要鉴权（401/407），在 `registered` 变为 true 之前，会不断发起新的 `pjsip_regc_create/init/send`。
- 这可能导致同一个上级平台短时间内收到大量 REGISTER。

改进思路（后续做工程化时再上）：

- 增加 `registering/in_flight` 状态，避免并发重复发
- 加入退避/重试间隔（例如失败后 3s/5s/10s 递增）
- 把 `expires` 与“续注册”逻辑明确区分：注册成功后在到期前刷新，而不是靠频繁轮询

### 5.1 `pjsip_regc` 生命周期

当前代码里：

- 出错路径会 `pjsip_regc_destroy(regc)`
- 成功发送后没有销毁 `regc`

这会带来两个学习点：

- 如果你只做“一次性注册并更新状态”，通常要在“收到最终响应后”释放 `regc`（否则可能长期泄漏）。
- 如果你想做“自动续注册/定时刷新”，那就需要把 `regc` 保存为对象成员，并在合适时机 refresh/unregister，再 destroy。

另外，你现在每次 `gbRegister()` 都创建一个新的 `regc`：

- 如果你不保存 `regc`，那就很难在回调里做更完整的处理（例如鉴权后“同一个 regc”重发）。
- 学习阶段建议先把 `401/407` 打日志看清楚；工程化阶段再决定是否把 `regc` 提升为成员并复用。

### 5.2 `From/To` 里的“域”到底是什么

GB28181 里常见是 `sip:设备ID@域ID`（域ID类似 `3402000000`），而不是 `@IP`。

- 你的实现使用 `GBOJ(gConfig)->sipIp()` 作为 `@` 后面的部分。
- 如果对接某些严格实现，建议把“域ID/realm”独立成配置项，明确区分 `本端IP` 与 `SIP域`。

### 5.3 `Contact` 的可达性

`Contact` 是对端后续主动找你时用的地址：

- 如果你在 NAT 后面，`Contact` 填内网 IP 往往不可达，需要 NAT 映射/穿透策略（或由上级平台按你源地址回呼）。

### 5.4 `SipMessage` 的缓冲区与安全性

`SipMessage` 里使用 `sprintf` 写入 `char[128]`：

- 当 `sipId/ip` 字符串过长时会发生缓冲区溢出

学习阶段可以先不改，但至少要知道风险点；后续建议替换成 `snprintf` 并检查返回值。

---

## 6. 上级（SUP）侧：如何处理收到的 REGISTER（服务端视角）

对应代码：

- [SipSupService/include/SipRegister.h](../../SipSupService/include/SipRegister.h)
- [SipSupService/src/SipRegister.cpp](../../SipSupService/src/SipRegister.cpp)

### 6.1 继承关系：`SipRegister : public SipTaskBase`

SUP 侧 `SipRegister` 继承自 `SipTaskBase`，因此它通过统一入口：

- `pj_status_t run(pjsip_rx_data* rdata)`

来处理收到的 SIP 请求。

代码里 `run()` 只是把处理转发到：

- `RegisterRequestMessage(rdata)` → `dealWithRegister(rdata)`

这符合“一个业务类只关心自己的消息处理”的任务模型。

### 6.2 业务处理：`dealWithRegister()` 做了什么

在 [SipSupService/src/SipRegister.cpp](../../SipSupService/src/SipRegister.cpp) 里，`dealWithRegister()` 的核心步骤是：

1) 取出消息：`pjsip_msg* msg = rdata->msg_info.msg;`
2) 从 `From` 中解析出下级 ID：`string fromId = parseFromId(msg);`
3) 白名单/存在性校验：`GlobalCtl::checkIsExist(fromId)`
    - 不存在：返回 `SIP_FORBIDDEN`（403）
    - 存在：继续解析 `Expires` 并写入 `GlobalCtl::setExpires(fromId, expiresValue)`
4) 构造响应：`pjsip_endpt_create_response(..., status_code, ..., &txdata)`
5) 添加 Date 头：`pjsip_date_hdr_create` + `pjsip_msg_add_hdr`
6) 获取响应地址并发送：`pjsip_get_response_addr` → `pjsip_endpt_send_response`

从工程效果看：SUP 侧现在实现的是一个“最小可用”的 REGISTER 处理器：

- 允许的设备返回 200
- 不允许的设备返回 403
- 记录 Expires（但注册状态/续期策略有一部分被注释掉，后续可再工程化）

### 6.3 鉴权（401/407 Digest）：目前是注释的学习代码

你在 SUP 侧代码里能看到一大段 `dealWithAuthorRegister()` 与 `auth_cred_callback()` 的尝试（大部分注释）。

当前实际走的是：

- `RegisterRequestMessage()` 直接进入 `dealWithRegister()`

这意味着：现在的“是否允许注册”主要由 `GlobalCtl::checkIsExist(fromId)` 决定，而不是 Digest 鉴权。

---

## 7. SUB vs SUP：两个 `SipRegister` 的关键差异（记住这张对照表）

- SUB（下级）：
   - 用 `pjsip_regc_*`（REGISTER client）
   - 主动发 REGISTER，靠 `client_cb()` 异步拿响应
   - 定时线程遍历上级列表，并用 `AutoMutexLock` 保护全局列表

- SUP（上级）：
   - 继承 `SipTaskBase`，通过 `run()` 处理收到的请求
   - 主动构造并发送响应（`create_response` / `send_response`）
   - 校验逻辑目前偏“白名单/存在性”，鉴权逻辑仍在演进

---

## 8. 工程化补充：两侧都容易踩的点

### 8.1 SUB 侧：`pjsip_regc` 的生命周期

SUB 侧发送成功后当前没有销毁 `regc`，长期运行可能形成资源累积；如果要做“可续注册”，更合理的是把 `regc` 保存为成员并复用/定时 refresh。

### 8.2 SUP 侧：并发与数据一致性

SUP 侧会更新 `GlobalCtl::setExpires(fromId, expiresValue)`；如果这个全局结构也被其它线程读取/写入，建议统一加锁（你在 SUB 侧已经有 `AutoMutexLock lock(&GlobalCtl::globalLock)` 的用法）。

### 8.3 `parseFromId()` 的脆弱性

两侧都依赖 `SipTaskBase::parseFromId()` 的固定位置截取（`substr(11, 20)`）。

- 报文格式一变就可能截错
- 更稳妥方式是从 `pjsip_from_hdr` 的 URI 结构中解析 user 部分（后续再做重构）

---

## 6. 自测/抓包建议（很适合学习）

- 先确认：本端已启动并监听 SIP 端口（UDP/TCP）
- 抓包观察 REGISTER：
  - 是否真的发到 `node.addrIp:node.sipPort`
  - 响应码是什么，回调是否触发
- 把日志增强：
  - 除了 `param->code`，把 `param->reason`（如可用）也打印出来，更利于定位

---

## 7. 下一步（如果你要继续完善注册）

- 增加 `401/407` 鉴权处理（Digest Auth）
- 增加“续注册/注销（unregister）”能力
- 让 `SipRegister` 不在构造函数里直接发网络请求（便于控制时序、重试与异常处理）
