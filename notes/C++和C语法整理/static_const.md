# `const` 与 `static const`（C/C++）笔记

> 目标：解释你在 `SipLocalConfig.cpp` 里看到的这一行：
>
> ```cpp
> // 被所有的类对象共享且不可修改
> static const string keyLocalIp = "local_ip";
> ```
>
> 重点讲清：`const` 是什么、`static` 加在 `const` 前面又改变了什么（尤其是“链接属性/可见性”），并给出“多 .cpp（多翻译单元）”的可复现实例。

---

## 1. 先说 `const`：到底“常量”是什么意思？

### 1.1 `const` 的核心语义：只读（不可通过该名字修改）

`const` 修饰对象后，表示：**通过这个名字不能修改它的值**。

```cpp
const int a = 10;
// a = 20; // 编译错误：a 是只读
```

对指针来说，`const` 的位置不同含义不同（这里只点到为止）：

```cpp
int x = 1;
const int* p1 = &x; // 指向 const int：不能通过 p1 改 x
int* const p2 = &x; // const 指针：p2 不能改指向，但可以通过 p2 改 x
```

### 1.2 `const` 不等于“编译期常量”

- `const` 只保证“只读语义”。
- 是否能用于编译期（比如数组长度、模板参数）取决于它是不是“常量表达式”。

```cpp
const int n = 10;       // 有时可当常量表达式使用（取决于语境/标准/编译器）
constexpr int m = 10;   // 明确是编译期常量表达式
```

在工程里：
- “需要编译期”的，用 `constexpr`。
- “只读配置/字符串”等，用 `const`/`std::string_view` 等。

---

## 2. 再说 `static`：它在这里主要改变“链接属性（linkage）”

`static` 这个关键字在不同位置含义不同：

- **函数体内**：改变存储期（变成静态存储期，函数结束变量仍存在）。
- **类内**：变成“类静态成员”（被所有对象共享）。
- **命名空间作用域/全局作用域**：改变链接属性（从外部链接变为内部链接）。

你这里的 `static const string keyLocalIp = ...;` 位于 `.cpp` 文件顶层（命名空间作用域），所以属于**第三种**：

> `static` = **内部链接（internal linkage）**：这个符号只在本翻译单元（本 `.cpp` 编出来的 `.o`）可见。

---

## 3. 关键对比：`const` vs `static const`（在 `.cpp` 顶层）

### 3.1 在 C++ 中：顶层 `const` 默认就“内部链接”

这是很多人一开始不熟的点：

- 在 **C++** 里，命名空间作用域的 `const` 变量 **默认就是内部链接**（除非你写了 `extern`）。
- 所以在 `.cpp` 顶层：
  - `const int x = 1;` 往往已经是“只在本 .cpp 可见”。
  - `static const int x = 1;` 也是“只在本 .cpp 可见”。

也就是说：在 **C++ 的 `.cpp` 文件**里，`static const` 很多时候只是“把意图写得更明显”，效果和单独 `const` 接近。

### 3.2 在 C 语言中：`const` 默认是外部链接（这是 C/C++ 差异）

- 在 **C** 里，文件作用域的 `const` 变量默认是**外部链接**。
- 所以 C 代码中经常用 `static const` 来限制符号只在本文件可见。

如果你同时写 C 和 C++，这也是常见的“写 `static const` 更稳妥”的原因之一。

---

## 4. 多翻译单元例子：为什么头文件里乱放会“重复定义”？

下面用一个最小 demo 解释“链接属性 + 多个 `.cpp`”会发生什么。

假设目录结构：

```
const_demo/
  bad.h
  a.cpp
  b.cpp
  main.cpp
```

### 4.1 错误示例：在头文件里写“变量定义”

`bad.h`：

```cpp
// bad.h
#pragma once
#include <string>

const std::string kName = "Alice"; // 注意：这是“定义”，不是声明
```

`a.cpp`：

```cpp
#include "bad.h"
#include <iostream>
void fa() { std::cout << kName << "\n"; }
```

`b.cpp`：

```cpp
#include "bad.h"
#include <iostream>
void fb() { std::cout << kName << "\n"; }
```

`main.cpp`：

```cpp
void fa();
void fb();
int main() { fa(); fb(); }
```

这段在 **C++** 里“有时不报重复定义”，原因是：顶层 `const` 多数实现为内部链接，每个 `.cpp` 各有一份自己的 `kName`。

但这会带来一个隐含问题：

- 你以为你在共享一个变量，实际上**每个翻译单元一份拷贝**。
- 如果你在某些地方拿地址比较（比如 `&kName`），不同 `.cpp` 里拿到的地址可能不同。

### 4.2 如果你想“全工程只有一个实体”，该怎么写？

#### 方案 A：`extern` 声明 + `.cpp` 里唯一一次定义（经典写法）

`good.h`：

```cpp
// good.h
#pragma once
#include <string>

extern const std::string kName; // 这是声明：告诉别的 .cpp 这里有个 kName
```

`good.cpp`：

```cpp
#include "good.h"

const std::string kName = "Alice"; // 唯一一次定义
```

这样整个程序里就只有一个 `kName`。

#### 方案 B：C++17 `inline` 变量（允许头文件多处定义）

如果你就是希望把定义放头文件里，又不想每个 `.cpp` 一份拷贝，可以用：

```cpp
// good.h
#pragma once
#include <string>

inline const std::string kName = "Alice"; // C++17 起：合法的“可重复定义”
```

这时多个 `.cpp` 包含它也不会重复定义报错，并且语义上是“同一个实体”。

#### 方案 C：更推荐的头文件常量：`constexpr`/`string_view`

如果常量本质上是字符串字面量，通常更推荐：

```cpp
// good.h
#pragma once
#include <string_view>

inline constexpr std::string_view kName = "Alice";
```

- 没有动态初始化开销
- 更轻量
- 更适合放头文件

---

## 5. 回到你的代码：`static const string keyLocalIp = "local_ip";` 到底意味着什么？

你这行在 `SipLocalConfig.cpp` 顶层：

```cpp
static const string keyLocalIp = "local_ip";
```

可以拆成三层理解：

1) `const`：通过 `keyLocalIp` 这个名字不能改字符串内容（不能重新赋值）。

2) `static`（命名空间作用域）= 内部链接：
- `keyLocalIp` 这个符号**只在 `SipLocalConfig.cpp` 这个翻译单元可见**。
- 其他 `.cpp` 即使写 `extern const string keyLocalIp;` 也链接不到它。

3) “被所有类对象共享”这句话：
- 不是因为 `static` 才共享（这里不是类成员 static）。
- 而是因为它是**全局/命名空间作用域变量**，本翻译单元里所有代码都用这一份。

更精确地说：
- 它是 **本 `.cpp` 文件内共享的一份只读变量**。

---

## 6. 常见误区：`static const` 和 “类静态成员”不是一回事

很多人看到 `static` 就以为是“类的 static 成员”。但你这个不是。

对比一下：

### 6.1 `.cpp` 顶层的 `static const`（你当前这种）

```cpp
// 作用域：文件/命名空间
static const int k = 1;
```

- 不是某个类的成员
- 只影响链接可见性（内部链接）

### 6.2 类里的 `static const`（类静态成员）

```cpp
struct A {
    static const int k = 1; // 类静态成员（某些情况下可在类内给初值）
};

// 使用：A::k
```

这才是“被所有类对象共享”。

---

## 7. 工程建议（结合你的场景）

- 如果这些 key 只在 `SipLocalConfig.cpp` 用：
  - 用 `static const std::string ...` 可以；更现代一点也可以用“匿名命名空间”代替 `static`。
- 如果 key 需要跨多个 `.cpp` 共享：
  - 用 `extern` + `.cpp` 唯一定义，或者 C++17 `inline` 变量。
- 如果 key 是字面量字符串：
  - 优先考虑 `inline constexpr std::string_view`（尤其适合放头文件）。

如果你愿意，我可以顺手帮你把 `SipLocalConfig.cpp` 里的这堆 `static const string ...` 改成更轻量的 `std::string_view`（前提：`ConfReader` 支持 `string_view` 或我们在调用处转成 `std::string`）。
