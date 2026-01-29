# 智能锁（RAII Lock）笔记（C++ / pthread）

> 目标：把“锁的正确打开方式”写成一套固定心智模型：**拿锁就要保证一定会释放**。最推荐的做法就是 RAII（Resource Acquisition Is Initialization）：
>
> - 构造时加锁
> - 析构时解锁
>
> 这样就算函数中途 `return`、`break`、`throw`，锁也不会忘记释放。

---

## 1. 什么叫“智能锁/RAII 锁”？

在 C++ 里，“智能锁”通常指：**用对象生命周期管理互斥锁的加/解锁**。

对比一下两种写法：

### 1.1 手动加解锁（容易漏）

```cpp
pthread_mutex_lock(&mtx);

if (error) {
    // 这里如果 return/throw 了，就会忘记 unlock
    return;
}

pthread_mutex_unlock(&mtx);
```

### 1.2 RAII（推荐）

```cpp
{
    AutoMutexLock g(&mtx); // 构造：加锁
    // ... 临界区 ...
} // 作用域结束：析构，自动解锁
```

核心收益：**异常安全 + 早返回安全 + 少写样板代码**。

---

## 2. 标准库智能锁：`std::lock_guard` / `std::unique_lock` / `std::scoped_lock`

如果你用的是 C++ 标准库互斥量（`std::mutex`），优先使用标准库的智能锁。

### 2.1 `std::lock_guard`（最常用，最轻量）

```cpp
#include <mutex>

std::mutex m;

void f() {
    std::lock_guard<std::mutex> lock(m);
    // 临界区
}
```

特点：
- 构造加锁、析构解锁
- 不可手动 unlock（更“傻瓜”，也更安全）

### 2.2 `std::unique_lock`（更灵活）

```cpp
#include <mutex>

std::mutex m;

void f() {
    std::unique_lock<std::mutex> lock(m); // 默认立即加锁

    // 需要的时候可以手动 unlock/lock
    lock.unlock();
    // ... 非临界区 ...
    lock.lock();

    // 也可以配合条件变量
}
```

常见用途：
- 需要“先解锁再做点事再加锁”
- 需要和 `std::condition_variable` 配合（条件变量等待通常要求 `unique_lock`）

### 2.3 `std::scoped_lock`（多把锁一起锁，避免死锁）

```cpp
#include <mutex>

std::mutex m1, m2;

void f() {
    std::scoped_lock lock(m1, m2);
    // 同时持有 m1 与 m2
}
```

特点：
- 一次性锁多把 mutex，内部会用避免死锁的策略
- C++17 引入

---

## 3. pthread 的“智能锁”：基于 RAII 的封装（对应你项目里的 `AutoMutexLock`）

你的工程里在 `SipSupService/include/Common.h`、`SipSubService/include/Common.h` 都有类似封装：

```cpp
class AutoMutexLock {
public:
    AutoMutexLock(pthread_mutex_t* l) : lock(l) { pthread_mutex_lock(lock); }
    ~AutoMutexLock() { pthread_mutex_unlock(lock); }
private:
    AutoMutexLock();
    AutoMutexLock(const AutoMutexLock&);
    AutoMutexLock& operator=(const AutoMutexLock&);
    pthread_mutex_t* lock;
};
```

它的思想就是 `lock_guard` 的 pthread 版。

### 3.1 正确使用姿势

- 锁对象（`AutoMutexLock`）应该是“局部变量”
- 临界区建议用“花括号”明确范围

```cpp
void update() {
    {
        AutoMutexLock g(&m_mutex);
        // 临界区
        shared_state++;
    }
    // 非临界区
}
```

---

## 4. 智能锁的常见坑（非常重要）

### 4.1 不要传空指针

`AutoMutexLock(pthread_mutex_t* l)` 如果传入 `nullptr`，`pthread_mutex_lock` 会崩。

建议约定：
- 构造前确保 mutex 已初始化
- 不要对空指针上锁

### 4.2 避免“同一线程重复加同一把非递归锁”

默认的 `pthread_mutex_t` 通常不是递归锁（不是 `PTHREAD_MUTEX_RECURSIVE`），同一线程二次 `lock` 可能死锁。

策略：
- 设计上避免递归进入同一临界区
- 真需要递归锁再显式使用 recursive mutex（并评估风险）

### 4.3 析构里做“复杂逻辑/日志”要谨慎

你当前的 `AutoMutexLock` 构造/析构里有：

```cpp
LOG(INFO) << "getLock";
LOG(INFO) << "freeLock";
```

注意：
- 这会导致日志非常多（每次加解锁都打）
- 更关键：析构函数里调用日志系统如果涉及锁/内存分配，可能在极端情况下引入死锁/重入问题（尤其是程序退出阶段、全局对象析构阶段）

经验建议：
- 调试阶段可以保留，但线上建议降低频率（例如改成 `VLOG(1)`）或去掉

### 4.4 RAII 对“取消/强杀线程”不一定有用

如果线程被“强杀”（例如某些情况下的异步取消/异常终止），RAII 析构不一定能跑到。

原则：
- 正常路径（return/throw）→ RAII 很可靠
- 非正常强杀 → 要靠更严格的线程退出策略（退出标志、join、条件变量等）

---

## 5. 推荐的统一写法（你项目的两种可选路线）

### 路线 A：继续用 pthread + `AutoMutexLock`

- 好处：改动小，贴合当前工程
- 建议：把 `AutoMutexLock` 当成 pthread 世界的 `std::lock_guard`

### 路线 B：逐步迁移到 `std::mutex` + `std::lock_guard`

- 好处：标准库语义更统一，和 `std::thread/std::condition_variable` 更契合
- 代价：需要逐步替换 pthread 相关 API

---

## 6. 一句话总结

- 想写对锁：**永远优先 RAII 智能锁**
- pthread 用法：`pthread_mutex_t` + RAII 封装
- C++ 标准库：`std::mutex` + `std::lock_guard/unique_lock/scoped_lock`
