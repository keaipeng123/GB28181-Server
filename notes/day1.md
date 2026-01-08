# day1 学习笔记

## 1. 今日学习内容
- Markdown
    - 语法库整理： [markdown-语法库.md](./markdown语法库.md)
- CMake / Make
    - 基本编译流程（configure + build）
    - CMake 生成文件的作用
    - 重新编译（增量/重新配置/彻底重建）
- 第三方库
    - 第三方库文档整理：[第三方库安装.md](./第三方库安装.md)

- 3rd/src中的代码是方便查看pjsip的源码，于项目无用

## 2. make 编译开发流程

### 2.1 常见流程（在源码目录直接生成，容易“污染源码目录”）
1. `cmake .`
2. `make`

说明：这会在当前目录生成 `CMakeCache.txt`、`CMakeFiles/`、`Makefile` 等。

### 2.2 推荐流程（外部构建目录 build/）
1. `cmake -S . -B build`
2. `cmake --build build`（等价于 `make -C build`）

好处：生成物都在 build/ 里，清理只需要 `rm -rf build`。

## 3. CMake 生成的文件/目录作用

> 前提：以下通常指构建目录（例如 build/）里的文件。

- `CMakeLists.txt`（不是生成物）
    - 作用：项目“构建说明书”（目标、源码、依赖、编译选项、安装规则等）。
    - CMake 读取它来生成构建系统。

- `CMakeCache.txt`
    - 作用：CMake 的配置缓存（编译器、依赖路径、开关选项、生成器信息等）。
    - 典型处理：配置异常时可删缓存或直接删 build/ 重新配置。

- `CMakeFiles/`
    - 作用：CMake 内部工作区与中间文件（依赖信息、flags、try_compile、编译器识别等）。
    - 一般不需要手动修改。

- `Makefile`
    - 作用：由 CMake 生成的 Make 构建入口（只在使用 “Unix Makefiles” 生成器时出现）。
    - `make` 会读取它并按规则构建目标。

- `cmake_install.cmake`
    - 作用：安装脚本（由 `install()` 规则生成）。
    - 执行 `cmake --install build` 或 `make install` 时会用到。

## 4. 需要重新编译的流程（按改动类型）

### 4.1 修改本地业务源代码（.c/.cpp/.h）
- 通常只需要增量编译：`cmake --build build`

### 4.2 修改第三方源代码
- 如果第三方是“参与同工程编译”的源码（如 `add_subdirectory` / `FetchContent`）：
    - 通常：`cmake --build build`
- 如果第三方是“外部预编译库”（系统库/你单独编译安装的 .so/.a）：
    - 库文件更新后：先 `cmake --build build` 试增量（常见是重新链接）
    - 若库路径/版本变化、`find_package` 结果可能变化：
        1. `cmake -S . -B build`
        2. `cmake --build build`

### 4.3 修改 CMakeLists.txt（或 cmake/ 脚本）
- 需要重新配置 + 编译：
    1. `cmake -S . -B build`
    2. `cmake --build build`

### 4.4 彻底重建（最干净，最慢）
- 用于：换编译器/生成器、缓存混乱、依赖查找异常等
    1. `rm -rf build`
    2. `cmake -S . -B build`
    3. `cmake --build build`