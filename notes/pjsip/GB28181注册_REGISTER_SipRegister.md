# GB28181 注册（REGISTER）学习笔记（围绕 `SipRegister`）

> 对应工程代码：
>
> - [SipSubService/include/SipRegister.h](../../SipSubService/include/SipRegister.h)
> - [SipSubService/src/SipRegister.cpp](../../SipSubService/src/SipRegister.cpp)
>
> 前置建议先读：
>
> - [notes/pjsip/pjsip初始化_SipCore.md](pjsip初始化_SipCore.md)
>
> 目标：
>
> 1) 看懂本工程如何用 PJSIP 发起 REGISTER。
> 2) 明确 `From/To/Request-URI/Contact/Expires` 在 GB28181 注册里的含义。
> 3) 知道回调如何拿到注册结果，以及常见坑。

---

## 1. `SipRegister` 在工程里的定位

`SipRegister` 负责“向上级平台（SUP）注册”。它的行为很直接：

- 构造函数里遍历 `GlobalCtl::instance()->getSupDomainInfoList()`
- 对每个未注册（`registered == false`）的上级节点 `node` 调用 `gbRegister(node)`
- 注册成功后在回调里把该 `node.registered = true`

> 这里的 `GlobalCtl::SupDomainInfo` 可以理解为“一个上级平台的注册目标”，里面通常会有：对端 `addrIp/sipPort`、SIP ID、transport、expires、以及 `registered` 状态。

---

## 2. 注册关键字段：From / To / Request-URI / Contact

在 [SipSubService/src/SipRegister.cpp](../../SipSubService/src/SipRegister.cpp) 里，`gbRegister()` 拼了 4 个字符串：

1) `fromHeader`：`<sip:本端ID@本端域>`
2) `toHeader`：`<sip:本端ID@本端域>`（REGISTER 场景里通常与 From 一致）
3) `requestUrl`：`sip:对端ID@对端IP:对端端口;transport=udp|tcp`
4) `contactUrl`：`<sip:本端ID@本端IP:本端端口>`

对应到 SIP REGISTER 的语义：

- `From/To`：声明“谁在注册”（AOR，Address-Of-Record），GB28181里通常形如 `sip:设备ID@域ID`。
  - 你当前代码用的是 `sipId@sipIp`（`sipIp` 更像 IP，不像“域ID”）；很多实现也能跑通，但学习时要知道规范里常见写法是 `设备ID@域ID`。
- `Request-URI`：发给谁（Registrar 的地址）。你这里发给 `node.addrIp:node.sipPort`，并加了 `transport=` 参数。
- `Contact`：注册成功后，对端以后“如何联系到你”（你实际可接收请求的地址/端口）。
- `Expires`：有效期（秒），来自 `node.expires`。

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

学习建议：

- 只看 `200` 会漏掉很多有价值的状态（例如 `401/407` 鉴权挑战，`403` 禁止，`404` 用户不存在，`408` 超时，`503` 服务不可用等）。
- 后续你想把注册做“真正可用”，通常需要处理 `401/407`（带认证再发一次 REGISTER）。

---

## 5. 常见坑（结合当前实现）

### 5.1 `pjsip_regc` 生命周期

当前代码里：

- 出错路径会 `pjsip_regc_destroy(regc)`
- 成功发送后没有销毁 `regc`

这会带来两个学习点：

- 如果你只做“一次性注册并更新状态”，通常要在“收到最终响应后”释放 `regc`（否则可能长期泄漏）。
- 如果你想做“自动续注册/定时刷新”，那就需要把 `regc` 保存为对象成员，并在合适时机 refresh/unregister，再 destroy。

### 5.2 `From/To` 里的“域”到底是什么

GB28181 里常见是 `sip:设备ID@域ID`（域ID类似 `3402000000`），而不是 `@IP`。

- 你的实现使用 `GBOJ(gConfig)->sipIp()` 作为 `@` 后面的部分。
- 如果对接某些严格实现，建议把“域ID/realm”独立成配置项，明确区分 `本端IP` 与 `SIP域`。

### 5.3 `Contact` 的可达性

`Contact` 是对端后续主动找你时用的地址：

- 如果你在 NAT 后面，`Contact` 填内网 IP 往往不可达，需要 NAT 映射/穿透策略（或由上级平台按你源地址回呼）。

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
