# TaskTimer 学习笔记（线程 + 定时回调）

对应源码：

- [SipSubService/include/TaskTimer.h](SipSubService/include/TaskTimer.h)
- [SipSubService/src/TaskTimer.cpp](SipSubService/src/TaskTimer.cpp)

目标：读懂这个 `TaskTimer` 做了什么、怎么用、以及有哪些工程化风险点（尤其是线程生命周期与停止机制）。

---

## 1. `TaskTimer` 的定位：最小可用的“周期任务线程”

`TaskTimer` 本质上做了三件事：

1) 保存一个回调函数指针 `timerCallBack` 及其参数 `void*`。
2) 启动一个 pthread 线程，在线程中循环计时。
3) 每隔 `m_timeSecond` 秒触发一次回调。

它不是一个“精确定时器”（不是基于 `timerfd`/`clock_nanosleep`），更像是：用线程轮询 + `gettimeofday()` 做间隔判断的简易周期任务器。

---

## 2. 头文件结构：回调签名 + 线程入口

### 2.1 回调函数指针

在 [SipSubService/include/TaskTimer.h](SipSubService/include/TaskTimer.h) 里定义了：

- `typedef void (*timerCallBack)(void* param);`

含义：

- 回调函数返回 `void`
- 只接收一个不定类型参数 `void*`（由调用者自己决定指向什么）

优点：通用、简单。
缺点：类型不安全，必须保证参数生命周期与类型转换正确。

### 2.2 为什么线程入口必须是 `static`

`pthread_create` 需要一个 C 风格函数指针（形如 `void* (*)(void*)`）。

类的**非静态成员函数**隐含 `this` 参数，函数签名不匹配；所以这里用：

- `static void* timer(void* context);`

再把 `this` 通过 `context` 传进去，在函数内手动转回：

- `TaskTimer* pthis = (TaskTimer*)context;`

---

## 3. 构造/析构：默认状态与停止策略

### 3.1 构造函数做了什么

在 [SipSubService/src/TaskTimer.cpp](SipSubService/src/TaskTimer.cpp) 中：

- `m_timerFun = NULL;`
- `m_funParam = NULL;`
- `m_timeSecond = TimeSecond;`
- `m_timerStop = false;`

说明：默认不执行任何回调；`start()` 之后线程会跑，但只有设置了回调才会触发。

### 3.2 析构函数只调用 `stop()`

析构中：

- `TaskTimer::~TaskTimer() { stop(); }`

`stop()` 只是：

- `m_timerStop = true;`

重要含义：

- **没有 join**（等待线程退出）
- **没有保存线程句柄**（`pthread_t` 只在 `start()` 的栈上）

因此从工程角度，存在典型风险：

- 对象析构后，线程可能还在访问 `pthis`（use-after-free）
- `stop()` 只是“请求停止”，不是“停止完成”

---

## 4. `start()`：创建线程（通过 ECThread 封装）

`start()` 中：

- 定义 `pthread_t pid;`
- 调用 `EC::ECThread::createThread(TaskTimer::timer, (void*)this, pid);`

这里线程创建被封装到 `ECThread`，这能统一错误处理、属性设置等，但也意味着你要去看 `ECThread::createThread` 才能确认：

- 线程是否 detach？
- 栈大小是否设置？
- 是否设置线程名？

当前 `start()` 只在创建失败时打日志：

- `LOG(ERROR) << "create thread failed";`

---

## 5. 核心循环：用 `gettimeofday()` 做间隔判断

### 5.1 时间单位与计算

代码用：

- `gettimeofday(&current, NULL)` 得到秒/微秒
- 计算毫秒：`curTm = current.tv_sec*1000 + current.tv_usec/1000`

然后判断：

- `(curTm - lastTm) >= m_timeSecond * 1000`

满足就触发回调。

### 5.2 第一次会不会“立刻触发”？

会。

因为 `lastTm` 初始为 0，第一次循环 `curTm - 0` 几乎必然大于间隔，于是会立即执行一次回调。

这在“服务启动后立刻执行一次周期任务”（比如立即做一次心跳/注册续期）时是合理的；但如果你希望“严格延迟 N 秒后第一次触发”，需要把 `lastTm` 初始化为当前时间。

### 5.3 sleep 策略

当未到触发时间，代码会：

- `usleep(1000*1000);`（睡 1 秒）

这会带来两个效果：

- 最坏情况下触发误差可接近 1 秒（粒度较粗）
- `stop()` 的响应也可能延迟最多约 1 秒

---

## 6. 为什么回调前要做 `pjcall_thread_register(desc)`？

在触发回调前，代码执行：

- `pj_thread_desc desc;`
- `pjcall_thread_register(desc);`

结合工程背景（PJSIP/PJLIB）：

- PJLIB 在多线程环境里，通常要求“外部创建的线程”在调用 PJLIB/PJSIP 相关 API 前进行线程注册。
- 这样 PJLIB 才能给该线程挂上内部线程局部存储（TLS）、日志/锁等上下文。

你这里把注册放在“每次触发回调时”执行，说明：

- 回调函数很可能会调用 PJSIP/PJLIB API（例如发消息、处理事务、跑定时器逻辑）

工程建议：

- 更常见做法是在线程启动后只注册一次（而不是每次 tick 都注册）。是否能重复注册、是否有开销/副作用，取决于 `pjcall_thread_register` 的实现。

---

## 7. 最小用法（示例形状）

典型用法是：

1) 创建 `TaskTimer`，指定周期秒数
2) `setTimerFun()` 设置回调与参数
3) `start()` 启动
4) 退出时 `stop()`（并等待线程结束，当前实现缺少这一步）

回调函数形状：

```cpp
static void onTimer(void* param) {
    // param 自行转换
}
```

---

## 8. 工程化改进清单（结合当前实现的“坑”）

这部分不是为了“否定当前代码”，而是帮助你在做 GB28181 服务时避免偶现崩溃/退出卡死。

### 8.1 线程生命周期：必须能安全退出

当前问题点：

- `pthread_t pid` 没保存
- `stop()` 不 join
- `m_timerStop` 不是原子/没有内存序保证（虽然多数场景也能工作，但属于数据竞争风险）

改进方向：

- 把 `pthread_t` 存成成员：`pthread_t m_tid;`
- `start()` 成功后保存 `m_tid`
- `stop()` 后在析构里 `pthread_join(m_tid, NULL)`（或线程 detach + 其他同步方式）
- `m_timerStop` 改成 `std::atomic<bool>` 或用互斥量/条件变量

### 8.2 精度/功耗：用条件变量或更小粒度 sleep

当前 `usleep(1s)`：

- 粗粒度、误差大、停止响应慢

可选方案：

- `pthread_cond_timedwait`：既能精确到期唤醒，也能 stop 时立刻唤醒退出
- Linux 下用 `timerfd` + epoll（如果你已经有事件循环）

### 8.3 时间源：`gettimeofday()` 受系统时间回拨影响

`gettimeofday()` 是“墙上时间”，如果系统校时/回拨：

- `curTm - lastTm` 可能变小甚至变负（无符号下会产生大数）

更稳妥的是使用单调时钟：

- `clock_gettime(CLOCK_MONOTONIC, ...)`

---

## 9. 和 SipCore 事件线程的关系（你现在工程里会同时存在两个线程模型）

你工程里至少会有：

- SipCore 的 `pollingEvent()`：持续 `pjsip_endpt_handle_events()`
- TaskTimer 的 timer 线程：周期触发回调（回调里可能调用 PJLIB/PJSIP）

这意味着你需要明确一个原则：

- 哪些 PJSIP API 必须在同一个线程调用？
- 是否需要把“周期任务”投递到 SipCore 的事件线程执行，而不是在 timer 线程直接调用？

这会直接决定你后续做 REGISTER 续期、心跳 MESSAGE、Catalog 拉取等逻辑的稳定性。

---

## 10. 你可以用这份笔记做什么（学习路线）

1) 先写一个最简单回调：每 N 秒打一次日志，确认线程/停止行为。
2) 再把回调改成“调用一次 PJLIB API”（验证线程注册是否必须）。
3) 最后再做 GB28181 业务：例如周期性发送心跳/注册续期。
