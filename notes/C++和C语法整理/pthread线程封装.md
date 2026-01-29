# pthread 线程封装笔记（结合 `ECThread` 代码）

> 目标：把工程里的 `SipSupService/include/ECThread.h` + `SipSupService/src/ECThread.cpp` 拆开讲清楚：
>
> - 这个封装“帮你做了什么”
> - pthread 的核心概念（joinable/detached、join、cancel、exit）
> - 代码里最容易踩坑的点
> - 你在项目里如何正确使用/改进

---

## 1. 文件与结构概览

- 头文件：`SipSupService/include/ECThread.h`
  - 定义命名空间 `EC`
  - 定义线程函数指针类型 `ECThreadFunc`
  - 定义工具类 `ECThread`（**全静态方法**，不对外提供实例）

- 源文件：`SipSupService/src/ECThread.cpp`
  - 实现上述静态方法

这个封装的核心思想：**把 pthread 的常用调用流程包成几个静态函数**，让业务侧少写一些样板代码。

---

## 2. `ECThreadFunc`：线程函数指针 typedef

代码：

```cpp
typedef void* (*ECThreadFunc)(void*);
```

解释：

- 线程入口函数必须匹配 pthread 约定的签名：
  - 入参：`void*`（你自己传的参数，随便塞结构体/对象指针）
  - 返回：`void*`（线程退出时的“返回值”，可被 `pthread_join` 接收）

常见用法：

```cpp
void* worker(void* arg) {
    // ...
    return nullptr;
}

pthread_t tid;
EC::ECThread::createThread(worker, arg, tid);
```

---

## 3. `ECThread` 的“工具类”设计

`ECThread` 把构造/析构写在 `private`，并且所有方法都是 `static`：

- 目的：**不让你创建对象实例**（这类工具类只提供函数集合）
- 优点：调用简单：`ECThread::createThread(...)`
- 缺点：
  - 无法用 RAII 自动管理线程生命周期
  - 无法自然表达“这个线程是 joinable 还是 detached”这样的状态

---

## 4. `createThread`：创建线程 + 设置属性

实现要点（简化描述）：

1) `pthread_attr_t threadAttr;`
2) `pthread_attr_init(&threadAttr);` 初始化属性对象
3) `pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);`
4) `pthread_create(&id, &threadAttr, startRoutine, args);`
5) `pthread_attr_destroy(&threadAttr);` 销毁属性对象

### 4.1 detached vs joinable（非常关键）

pthread 线程有两类：

- **joinable（可被 join）**：默认就是 joinable
  - 其他线程可 `pthread_join(tid, &ret)` 等它结束
  - join 后系统回收线程资源

- **detached（分离态）**：线程结束时系统会自动回收资源
  - **不能被 join**（对 detached 调 `pthread_join` 通常会失败）

本封装在 `createThread` 里固定设置为：

- `PTHREAD_CREATE_DETACHED`

所以：**由 `createThread` 创建出来的线程，一开始就是 detached。**

### 4.2 `do { ... } while(0)` 的意义

这是一种常见的“流程块”写法：

- 让你可以用 `break` 在中间失败时提前跳出
- 避免写很多层 `if (ret != 0) { ... }`

---

## 5. `detachSelf`：把“当前线程”分离

```cpp
pthread_detach(pthread_self());
```

语义：

- 调用后，**当前线程变成 detached**
- detached 线程退出后会自动回收资源

注意：

- 如果线程已经 detached，再 detach 一次会返回错误码
- detached 线程无法被 `pthread_join`

---

## 6. `exitSelf`：线程主动退出

```cpp
pthread_exit(rval);
```

语义：

- 立即结束当前线程
- `rval` 会成为“线程返回值”
  - 如果线程是 joinable，其他线程可以 `pthread_join` 拿到它
  - 如果线程是 detached，没人能 join，它也就没意义了

---

## 7. `waitThread`：等待指定线程结束（join）

```cpp
pthread_join(id, rval);
```

语义：

- 阻塞等待线程 `id` 结束
- 获取线程的返回值 `rval`

### 7.1 重要坑：本工程当前实现存在“逻辑冲突”

因为 `createThread()` **把线程创建成 detached**，而 `waitThread()` 却试图 `pthread_join()`：

- 这两者在 pthread 语义上是冲突的
- detached 线程通常 join 不上，会返回错误（常见 `EINVAL`）

结论：如果你的线程真的是用 `createThread()` 创建的，那么 `waitThread()` 这条路大概率走不通。

你可以把这个冲突当成一个“学习点”：

- 要么：创建 joinable 线程（不要设置 DETACHED），后续用 `waitThread`
- 要么：创建 detached 线程，就不要 join（也不该设计 `waitThread` 去 join 它）

---

## 8. `terminateThread`：向指定线程发出取消请求（cancel）

```cpp
pthread_cancel(id);
```

语义（更准确说法）：

- 发送“取消请求”给目标线程
- 目标线程是否真的立刻退出，取决于：
  - 取消状态（enable/disable）
  - 取消类型（deferred/async）
  - 是否到达取消点（例如 `pthread_testcancel`、`read`、`sleep` 等可能是取消点）

**实务经验**：`pthread_cancel` 并不是“可靠的强杀”。

- 如果线程屏蔽取消、或长期不经过取消点，你 cancel 了也未必马上死
- 如果资源清理没写好，会导致锁没释放、内存没释放、状态损坏

通常更推荐业务层用“退出标志 + 条件变量/事件”实现可控退出。

---

## 9. 代码里的其他值得注意点

### 9.1 头文件里 `#include <sys/prctl.h>` 没用上

`ECThread.h` 里包含了：

```cpp
#include <sys/prctl.h>
```

但 `ECThread` 的实现没有用到 `prctl`。

你可能原本想做的是“设置线程名”，常见写法是：

```cpp
prctl(PR_SET_NAME, "worker", 0, 0, 0);

---

## 10. 补充：临界区建议用“智能锁（RAII）”

线程封装之外，项目里大量并发共享数据的地方还会用到互斥锁。

建议的写法是：**不要手写 `pthread_mutex_lock/unlock` 配对**，而是统一用 RAII 智能锁（构造加锁、析构解锁），避免中途 `return/throw` 导致漏解锁。

- 相关笔记：见 `C++和C语法整理/智能锁_RAII.md`

```

（线程名在调试/`top -H`/`ps -L`/日志定位时很有用。）

### 9.2 错误码被“抹平”了

现在这些封装在失败时统一返回 `-1`：

- 好处：调用者只要判断成功/失败
- 坏处：丢失了 pthread 的真实错误码（例如 `EINVAL`、`ESRCH`、`EPERM`）

学习时建议记住：pthread 系列函数 **返回值本身就是错误码**（不是 `errno`），调试时打印它会很有帮助。

### 9.3 C++ 异常不要跨过 pthread 线程入口

pthread 线程入口是 C 风格函数指针。

- 如果 `startRoutine` 里抛 C++ 异常并“逃逸”出线程入口函数边界，行为可能不安全。
- 实务上建议在线程入口最外层 `try/catch`，把异常吞掉并转换成错误码/日志。

---

## 10. 如何正确使用这份封装（示例）

### 10.1 最适合的用法：只创建 detached“后台线程”

```cpp
struct Ctx {
    int x;
};

void* worker(void* arg) {
    auto* ctx = static_cast<Ctx*>(arg);
    // ... do work ...
    return nullptr;
}

int main() {
    pthread_t tid;
    Ctx ctx{123};
    EC::ECThread::createThread(worker, &ctx, tid);

    // detached：不能 join，所以主线程不要 waitThread
    // ...
}
```

### 10.2 如果你需要 join：封装需要支持 joinable

当前 `createThread()` 固定 detached，所以若业务真要 join：

- 需要把 `createThread` 做成可选：joinable/detached
- 或者新增一个 `createJoinableThread(...)`

这部分属于“封装演进方向”，适合你后续练手。

---

## 11. 你可以用它练到的 pthread 核心点（背下来）

- `pthread_create` 创建线程
- `pthread_attr_*` 配置线程属性（例如 detached）
- joinable vs detached 的区别：
  - joinable：`pthread_join` 等它结束并回收
  - detached：结束自动回收，不能 join
- `pthread_exit`：线程主动退出
- `pthread_cancel`：取消请求，不是可靠强杀

---

## 12. 建议你下一步怎么学（结合本工程）

1) 在工程里搜一下 `ECThread::createThread` 的实际调用场景，看看业务是把它当“后台线程”还是“可等待线程”。
2) 如果确实需要等待线程：把封装改成“可选 detached/joinable”，并在 notes 里记录一次完整改造过程。
3) 学会用 `top -H -p <pid>` 或 `ps -L` 观察线程名/线程数量，理解线程生命周期。
