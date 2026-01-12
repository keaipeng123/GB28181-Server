
# inline 关键字整理（C++）

> 结论先行：`inline` **不是**“强制内联展开”，而是一个“允许在多个翻译单元重复定义”的语义（ODR 相关）+ 给编译器的内联优化提示。

## 1. inline 是做什么的？

在 C++ 里，`inline` 主要有两层含义：

1）**链接/ODR 层面**：
- 允许一个函数/变量（C++17 起）在多个 `.cpp`（翻译单元）里出现**相同定义**，链接时不会因为“重复定义”报错。
- 典型用途：把函数定义写在头文件里，然后被很多 `.cpp` `#include`。

2）**优化层面（提示）**：
- 编译器“可能”把函数调用点展开成函数体（内联展开），以减少调用开销。
- 但是否真的内联由编译器决定（受优化级别、函数复杂度、递归、地址被取用、可见性等影响）。

## 2. 语法怎么写？

### 2.1 普通函数

```cpp
inline int add(int a, int b) {
	return a + b;
}
```

### 2.2 类内定义的成员函数（隐式 inline）

类定义内部直接写了函数体的成员函数，**默认就是 inline**（即使不写 `inline` 关键字）。

```cpp
class A {
public:
	int f() { return 1; } // 隐式 inline
};
```

### 2.3 类外定义的成员函数（常见写法）

```cpp
class A {
public:
	int f();
};

inline int A::f() { return 1; }
```

### 2.4 C++17：inline 变量（解决头文件全局变量重复定义）

```cpp
// header.h
inline int g_counter = 0;
```

## 3. 为什么头文件里经常写 inline？

头文件会被多个 `.cpp` 包含。

- 如果你在头文件里写了“普通函数定义”（非 inline），每个包含它的 `.cpp` 都会生成一份定义，链接时可能报 **multiple definition**。
- 把它标成 `inline`（或让它成为类内定义的成员函数），就符合 ODR 的“可重复定义”规则，链接不会报错。

这也是你看到的常见代码形态：

```cpp
inline std::string localIp() { return m_localIp; }
```

## 3.1 一个“重复定义/ODR”最小例子（多 `.cpp` + 同一个头文件）

下面这个例子专门用来解释你提到的这句话：

> 允许一个函数/变量（C++17 起）在多个 `.cpp`（翻译单元）里出现相同定义，链接时不会因为“重复定义”报错。

### 3.1.1 先看：不加 inline 会发生什么？（会链接失败）

假设有如下文件结构：

```
demo/
  util.h
  a.cpp
  b.cpp
  main.cpp
```

`util.h`（在头文件里写了“普通函数定义”，没有 `inline`）：

```cpp
// util.h
#pragma once

int add(int a, int b) {
	return a + b;
}
```

`a.cpp`：

```cpp
// a.cpp
#include "util.h"

int fa() {
	return add(1, 2);
}
```

`b.cpp`：

```cpp
// b.cpp
#include "util.h"

int fb() {
	return add(3, 4);
}
```

`main.cpp`：

```cpp
// main.cpp
#include <iostream>

int fa();
int fb();

int main() {
	std::cout << fa() + fb() << "\n";
	return 0;
}
```

编译链接（以 g++ 为例）：

```bash
g++ -std=c++17 a.cpp b.cpp main.cpp -o demo
```

你大概率会在“链接阶段”看到类似错误（信息略有差异）：

```
multiple definition of `add(int, int)`
```

原因是：

- `a.cpp` 包含了 `util.h`，因此在 **翻译 `a.cpp`** 时，编译器会把 `add` 的定义编进 `a.o`。
- `b.cpp` 也包含了 `util.h`，因此在 **翻译 `b.cpp`** 时，编译器又把 **同一个 `add` 定义** 编进 `b.o`。
- 链接器把 `a.o`、`b.o`、`main.o` 链在一起时，发现 **全局符号 `add` 被定义了两次**，就报“重复定义”。

### 3.1.2 再看：加上 inline 后为什么就不报错了？

把 `util.h` 改成 `inline` 函数：

```cpp
// util.h
#pragma once

inline int add(int a, int b) {
	return a + b;
}
```

此时：

- `a.cpp` 和 `b.cpp` 仍然都会各自“看到”一份 `add` 的函数体（因为头文件被 include 进来了）。
- 但是 C++ 规则允许 `inline` 函数在多个翻译单元中出现**相同定义**，只要这些定义一致，链接器会把它们视为“同一个实体”的合法重复定义（ODR 允许的那种）。

注意：这里的“允许重复定义”是**语言规则/链接语义**，并不等价于“强制把调用点展开”。

### 3.1.3 同理：C++17 的 inline 变量（解决头文件全局变量重复定义）

再给一个变量版本的对比例子。

不使用 `inline` 变量（会链接失败）：

```cpp
// config.h
#pragma once

int g_counter = 0; // ❌ 头文件里定义了一个全局变量
```

只要 `a.cpp`、`b.cpp` 都 `#include "config.h"`，链接时也会出现 `multiple definition of g_counter`。

改为 C++17 `inline` 变量（合法）：

```cpp
// config.h
#pragma once

inline int g_counter = 0; // ✅ C++17 起：允许在多个翻译单元里重复定义(相同定义)
```

这样 `a.cpp`、`b.cpp`、`main.cpp` 都包含它也不会因为重复定义而报错。

### 3.1.4 小结：这句话到底在说什么？

- “多个 `.cpp` 出现相同定义”指的是：多个翻译单元都包含了同一个头文件，而头文件里写了实体的“定义”。
- 对普通函数/普通全局变量来说：这样会导致链接阶段出现 `multiple definition`。
- 对 `inline` 函数（以及 C++17 的 `inline` 变量）来说：语言规则允许这种“同定义的重复出现”，所以链接不会报错。

## 4. inline 与宏（#define）有什么区别？

- `inline` 是“真正的函数”，有类型检查、作用域规则、可被调试器识别。
- 宏是纯文本替换，可能出现副作用/优先级问题：
  - `#define SQR(x) ((x)*(x))`，`SQR(i++)` 会把 `i` 自增两次。

解释一下为什么会这样：

- 宏在预处理阶段做的是**纯文本替换**，不会像函数那样“先把参数算出来，再传进去”。
- `SQR(x)` 这个宏体里把 `x` 写了两遍，所以当你写 `SQR(i++)` 时，展开后等价于：

```c
((i++) * (i++))
```

从字面上看，`i++` 出现了两次，因此 `i` 会被自增两次。

但更关键的是：在 C/C++ 中，这类写法通常属于**未定义行为（Undefined Behavior）**。

- `*` 两侧子表达式的求值顺序并不保证；
- 同一个“完整表达式”里对同一个变量 `i` 做了多次修改（两个 `i++`），而修改之间的先后关系没被语言规则约束。

因此它不只是“自增两次”这么简单，而是结果整体都不可靠。

工程上更推荐用 `inline/constexpr` 函数替代（参数只会求值一次）：

```cpp
template <class T>
constexpr T sqr(T x) { return x * x; }

// sqr(i++) 只会让 i 自增一次（作为实参求值一次）
```

一般建议：能用 `inline`/`constexpr` 函数就别用宏函数。

## 5. 常见坑与注意点

### 5.1 inline 不保证一定内联

即使写了 `inline`，编译器也可能不展开（例如函数太大、递归、优化关闭、需要生成可见符号等）。

### 5.2 “inline 只是头文件技巧”也不完全对

`inline` 也可以写在 `.cpp` 中，但如果它只在一个翻译单元里使用，`inline` 的 ODR 价值就不大（更多是提示/风格）。

### 5.3 代码膨胀

过度内联（尤其大函数）可能导致可执行文件体积变大、指令缓存压力增加，反而变慢。

### 5.4 取函数地址时

取地址并不意味着不能内联（编译器可以同时内联和保留一个可寻址实体），但通常会影响优化决策。

## 6. 与 const / constexpr 的关系（快速记忆）

- `inline`：允许多处定义 + 可能内联优化。
- `constexpr`：强调“可在编译期求值”（函数/变量），常用于常量表达式；`constexpr` 函数也经常放头文件里。
- `const`：只读语义；全局 `const` 在 C++ 里默认内部链接（但不要依赖这个来乱放定义）。

## 7. 一句话建议（工程实践）

- 小型、频繁调用、放头文件的函数：可以用 `inline`（或类内定义）。
- 大函数/复杂逻辑：优先放 `.cpp`，让链接结构更清晰、编译更快。
- 头文件里尽量避免“非 inline 的可被多处包含的定义”（尤其是全局变量）。

