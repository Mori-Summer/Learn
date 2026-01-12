# Month 44: Sanitizers——运行时错误检测利器

## 本月主题概述

本月深入学习编译器内置的Sanitizers工具：AddressSanitizer（ASan）、ThreadSanitizer（TSan）、UndefinedBehaviorSanitizer（UBSan）和MemorySanitizer（MSan）。这些工具能够在运行时检测内存错误、数据竞争和未定义行为。

**学习目标**：
- 掌握各种Sanitizer的使用方法和适用场景
- 理解Sanitizer的工作原理
- 学会在CI/CD中集成Sanitizer测试
- 能够分析和修复Sanitizer报告的问题

---

## 理论学习内容

### 第一周：AddressSanitizer（ASan）

**学习目标**：掌握内存错误检测

**阅读材料**：
- [ ] AddressSanitizer官方文档
- [ ] Google Testing Blog: ASan介绍
- [ ] LLVM ASan文档

**核心概念**：

```bash
# ==========================================
# 编译启用ASan
# ==========================================

# Clang
clang++ -fsanitize=address -fno-omit-frame-pointer -g source.cpp -o app

# GCC
g++ -fsanitize=address -fno-omit-frame-pointer -g source.cpp -o app

# CMake
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"

# 运行时选项
ASAN_OPTIONS=detect_leaks=1:halt_on_error=0 ./app

# 常用ASAN_OPTIONS
# detect_leaks=1        - 启用内存泄漏检测
# halt_on_error=0       - 遇到错误继续运行
# abort_on_error=1      - 遇到错误时abort
# print_stats=1         - 打印统计信息
# check_initialization_order=1 - 检测静态初始化顺序问题
# strict_string_checks=1      - 严格字符串检查
# detect_stack_use_after_return=1 - 检测返回后使用栈
```

**ASan能检测的错误类型**：

```cpp
// ==========================================
// 1. 堆缓冲区溢出 (Heap-buffer-overflow)
// ==========================================
void heap_buffer_overflow() {
    int* arr = new int[10];
    arr[10] = 42;  // 越界写入
    delete[] arr;
}

// ==========================================
// 2. 栈缓冲区溢出 (Stack-buffer-overflow)
// ==========================================
void stack_buffer_overflow() {
    int arr[10];
    arr[10] = 42;  // 越界写入
}

// ==========================================
// 3. 全局缓冲区溢出 (Global-buffer-overflow)
// ==========================================
int global_arr[10];
void global_buffer_overflow() {
    global_arr[10] = 42;  // 越界写入
}

// ==========================================
// 4. 释放后使用 (Use-after-free / Heap-use-after-free)
// ==========================================
void use_after_free() {
    int* p = new int(42);
    delete p;
    *p = 100;  // 释放后使用
}

// ==========================================
// 5. 返回后使用栈 (Stack-use-after-return)
// ==========================================
int* get_stack_ptr() {
    int local = 42;
    return &local;  // 返回局部变量地址
}

void stack_use_after_return() {
    int* p = get_stack_ptr();
    *p = 100;  // 使用已失效的栈地址
}

// ==========================================
// 6. 作用域外使用 (Stack-use-after-scope)
// ==========================================
void stack_use_after_scope() {
    int* p;
    {
        int local = 42;
        p = &local;
    }
    *p = 100;  // 局部变量已超出作用域
}

// ==========================================
// 7. 双重释放 (Double-free)
// ==========================================
void double_free() {
    int* p = new int(42);
    delete p;
    delete p;  // 再次释放
}

// ==========================================
// 8. 内存泄漏 (Memory leak) - LeakSanitizer
// ==========================================
void memory_leak() {
    int* p = new int(42);
    // 忘记 delete p;
}

// ==========================================
// 9. 初始化顺序错误 (Initialization order fiasco)
// ==========================================
// file1.cpp
extern int global_b;
int global_a = global_b + 1;  // global_b可能还未初始化

// file2.cpp
int global_b = 10;
```

**理解ASan报告**：

```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000028 at pc 0x55555555518d bp 0x7fffffffe0a0 sp 0x7fffffffe098
WRITE of size 4 at 0x602000000028 thread T0
    #0 0x55555555518c in main /path/to/source.cpp:10:14
    #1 0x7ffff7a2d082 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x24082)
    #2 0x5555555550cd in _start (/path/to/app+0x10cd)

0x602000000028 is located 0 bytes to the right of 40-byte region [0x602000000000,0x602000000028)
allocated by thread T0 here:
    #0 0x7ffff7f3e587 in operator new[](unsigned long) (/lib/x86_64-linux-gnu/libasan.so.6+0xb2587)
    #1 0x555555555168 in main /path/to/source.cpp:8:18
    #2 0x7ffff7a2d082 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x24082)

SUMMARY: AddressSanitizer: heap-buffer-overflow /path/to/source.cpp:10:14 in main
```

### 第二周：ThreadSanitizer（TSan）

**学习目标**：掌握数据竞争检测

**阅读材料**：
- [ ] ThreadSanitizer文档
- [ ] Data Race的定义和后果
- [ ] C++内存模型基础

```bash
# ==========================================
# 编译启用TSan
# ==========================================

# 编译
clang++ -fsanitize=thread -fno-omit-frame-pointer -g source.cpp -o app -pthread

# 注意：TSan与ASan不兼容，不能同时使用

# 运行时选项
TSAN_OPTIONS=halt_on_error=0:second_deadlock_stack=1 ./app

# 常用TSAN_OPTIONS
# halt_on_error=0           - 遇到错误继续运行
# second_deadlock_stack=1   - 显示死锁的第二个栈
# report_signal_unsafe=1    - 报告信号不安全的调用
# history_size=7            - 历史大小（0-7，越大越慢）
```

**TSan能检测的错误类型**：

```cpp
// ==========================================
// 1. 数据竞争 (Data Race)
// ==========================================
#include <thread>

int shared_data = 0;

void data_race_example() {
    std::thread t1([] {
        shared_data = 1;  // 竞争写入
    });

    std::thread t2([] {
        shared_data = 2;  // 竞争写入
    });

    t1.join();
    t2.join();
}

// 修复方法1：使用互斥锁
std::mutex mtx;
void fixed_with_mutex() {
    std::thread t1([&] {
        std::lock_guard<std::mutex> lock(mtx);
        shared_data = 1;
    });

    std::thread t2([&] {
        std::lock_guard<std::mutex> lock(mtx);
        shared_data = 2;
    });

    t1.join();
    t2.join();
}

// 修复方法2：使用原子变量
#include <atomic>
std::atomic<int> atomic_data{0};

void fixed_with_atomic() {
    std::thread t1([] {
        atomic_data.store(1, std::memory_order_release);
    });

    std::thread t2([] {
        atomic_data.store(2, std::memory_order_release);
    });

    t1.join();
    t2.join();
}

// ==========================================
// 2. 读写竞争
// ==========================================
void read_write_race() {
    std::thread writer([] {
        shared_data = 42;  // 写入
    });

    std::thread reader([] {
        int value = shared_data;  // 并发读取
        (void)value;
    });

    writer.join();
    reader.join();
}

// ==========================================
// 3. 锁顺序反转 (可能导致死锁)
// ==========================================
std::mutex mutex_a, mutex_b;

void thread_1() {
    std::lock_guard<std::mutex> lock_a(mutex_a);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::lock_guard<std::mutex> lock_b(mutex_b);  // 先a后b
}

void thread_2() {
    std::lock_guard<std::mutex> lock_b(mutex_b);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::lock_guard<std::mutex> lock_a(mutex_a);  // 先b后a - 可能死锁！
}

// 修复：使用std::lock或std::scoped_lock
void thread_1_fixed() {
    std::scoped_lock lock(mutex_a, mutex_b);
    // ...
}

void thread_2_fixed() {
    std::scoped_lock lock(mutex_a, mutex_b);
    // ...
}

// ==========================================
// 4. 信号处理中的竞争
// ==========================================
#include <signal.h>

volatile sig_atomic_t flag = 0;  // 必须使用sig_atomic_t

void signal_handler(int) {
    flag = 1;  // 只能设置sig_atomic_t类型的变量
}

void signal_race_example() {
    signal(SIGINT, signal_handler);

    while (!flag) {
        // 等待信号
    }
}
```

**理解TSan报告**：

```
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x5629c6e8f000 by thread T2:
    #0 lambda at /path/to/source.cpp:15:19 (app+0x1234)
    #1 std::thread::_State_impl<...>::_M_run() (/lib/x86_64-linux-gnu/libstdc++.so.6+0xd6de3)

  Previous write of size 4 at 0x5629c6e8f000 by thread T1:
    #0 lambda at /path/to/source.cpp:11:19 (app+0x1200)
    #1 std::thread::_State_impl<...>::_M_run() (/lib/x86_64-linux-gnu/libstdc++.so.6+0xd6de3)

  Location is global 'shared_data' of size 4 at 0x5629c6e8f000 (app+0x5000)

  Thread T2 (tid=12347, running) created by main thread at:
    #0 pthread_create (/lib/x86_64-linux-gnu/libtsan.so.0+0x5ea79)
    #1 std::thread::_M_start_thread(...) (/lib/x86_64-linux-gnu/libstdc++.so.6+0xd70a8)
    #2 main /path/to/source.cpp:18:17 (app+0x1280)

  Thread T1 (tid=12346, finished) created by main thread at:
    ...

SUMMARY: ThreadSanitizer: data race /path/to/source.cpp:15:19 in lambda
==================
```

### 第三周：UndefinedBehaviorSanitizer（UBSan）

**学习目标**：检测未定义行为

**阅读材料**：
- [ ] UBSan文档
- [ ] C++未定义行为分类
- [ ] 为什么UB很危险

```bash
# ==========================================
# 编译启用UBSan
# ==========================================

# 基本用法
clang++ -fsanitize=undefined -fno-omit-frame-pointer -g source.cpp -o app

# 可以与ASan组合使用
clang++ -fsanitize=address,undefined -g source.cpp -o app

# 指定特定检查
clang++ -fsanitize=signed-integer-overflow,null,alignment source.cpp

# 运行时选项
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 ./app

# 常用UBSAN_OPTIONS
# print_stacktrace=1   - 打印栈追踪
# halt_on_error=0      - 遇到错误继续运行
# report_error_type=1  - 报告错误类型
```

**UBSan检测的错误类型**：

```cpp
// ==========================================
// 1. 有符号整数溢出 (signed-integer-overflow)
// ==========================================
void signed_overflow() {
    int a = INT_MAX;
    int b = a + 1;  // 未定义行为！
}

// ==========================================
// 2. 无符号整数溢出（不是UB，但可以检测）
// ==========================================
// 使用 -fsanitize=unsigned-integer-overflow

// ==========================================
// 3. 除以零 (integer-divide-by-zero)
// ==========================================
void divide_by_zero() {
    int a = 10;
    int b = 0;
    int c = a / b;  // 未定义行为！
}

// ==========================================
// 4. 空指针解引用 (null)
// ==========================================
void null_dereference() {
    int* p = nullptr;
    int x = *p;  // 未定义行为！
}

// ==========================================
// 5. 未对齐访问 (alignment)
// ==========================================
void unaligned_access() {
    char buffer[sizeof(int) + 1];
    int* p = reinterpret_cast<int*>(&buffer[1]);  // 未对齐
    *p = 42;  // 可能是未定义行为
}

// ==========================================
// 6. 数组越界 (bounds)
// ==========================================
void array_bounds() {
    int arr[10];
    int x = arr[100];  // 越界
}

// ==========================================
// 7. 类型转换溢出 (float-cast-overflow)
// ==========================================
void float_cast_overflow() {
    double d = 1e100;
    int i = static_cast<int>(d);  // 溢出
}

// ==========================================
// 8. 移位错误 (shift)
// ==========================================
void shift_errors() {
    int x = 1;
    int a = x << 32;   // 移位超过类型宽度
    int b = x << -1;   // 负数移位
    int c = -1 << 1;   // 负数左移
}

// ==========================================
// 9. 不可达代码 (unreachable)
// ==========================================
void unreachable_code() {
    if (false) {
        __builtin_unreachable();  // 如果到达这里就是UB
    }
}

// ==========================================
// 10. 虚函数调用错误 (vptr)
// ==========================================
class Base {
public:
    virtual void foo() {}
    virtual ~Base() = default;
};

class Derived : public Base {
public:
    void foo() override {}
};

void vptr_error() {
    Base* b = new Derived();
    b->~Base();  // 析构
    b->foo();    // 调用已析构对象的虚函数 - UB
}

// ==========================================
// 11. 对象生命周期错误 (object-size)
// ==========================================
struct Small { int x; };
struct Large { int x, y, z; };

void object_size_error() {
    Small s;
    Large* l = reinterpret_cast<Large*>(&s);
    l->z = 42;  // 访问超出对象边界
}

// ==========================================
// 12. 返回值未初始化 (return)
// ==========================================
int return_uninitialized() {
    int x;  // 未初始化
    return x;  // 返回未初始化的值
}

// ==========================================
// 13. bool类型越界 (bool)
// ==========================================
void bool_error() {
    bool b;
    *reinterpret_cast<char*>(&b) = 2;  // bool只能是0或1
    if (b) {}  // UB
}

// ==========================================
// 14. 枚举越界 (enum)
// ==========================================
enum Color { Red = 0, Green = 1, Blue = 2 };

void enum_error() {
    Color c = static_cast<Color>(100);  // 无效的枚举值
}
```

### 第四周：MemorySanitizer与集成实践

**学习目标**：使用MSan检测未初始化内存，集成到CI

**阅读材料**：
- [ ] MemorySanitizer文档
- [ ] Sanitizer在CI中的使用
- [ ] 性能影响分析

```bash
# ==========================================
# MemorySanitizer（仅Clang支持）
# ==========================================

# 编译（需要使用msan-instrumented的标准库）
clang++ -fsanitize=memory -fno-omit-frame-pointer -g source.cpp -o app

# MSan需要所有代码都被instrumented，包括标准库
# 因此实际使用中通常需要自己编译libc++

# 运行时选项
MSAN_OPTIONS=halt_on_error=0:print_stats=1 ./app

# 注意：MSan与ASan、TSan都不兼容
```

**MSan示例**：

```cpp
// ==========================================
// 未初始化内存使用
// ==========================================
void use_uninitialized() {
    int x;  // 未初始化
    if (x > 0) {  // 使用未初始化的值做条件判断
        printf("positive\n");
    }
}

void use_uninitialized_parameter(int* out) {
    int local;  // 未初始化
    *out = local;  // 传播未初始化的值
}

void use_heap_uninitialized() {
    int* p = (int*)malloc(sizeof(int));
    if (*p > 0) {  // 使用未初始化的堆内存
        printf("positive\n");
    }
    free(p);
}
```

**CMake集成**：

```cmake
# ==========================================
# cmake/Sanitizers.cmake
# ==========================================
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer" OFF)

function(enable_sanitizers target)
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
            CMAKE_CXX_COMPILER_ID MATCHES ".*Clang"))
        message(WARNING "Sanitizers only supported with GCC and Clang")
        return()
    endif()

    set(SANITIZERS "")

    if(ENABLE_ASAN)
        list(APPEND SANITIZERS "address")
    endif()

    if(ENABLE_TSAN)
        if(ENABLE_ASAN)
            message(WARNING "TSAN is incompatible with ASAN")
        else()
            list(APPEND SANITIZERS "thread")
        endif()
    endif()

    if(ENABLE_UBSAN)
        list(APPEND SANITIZERS "undefined")
    endif()

    if(ENABLE_MSAN)
        if(ENABLE_ASAN OR ENABLE_TSAN)
            message(WARNING "MSAN is incompatible with ASAN and TSAN")
        elseif(NOT CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
            message(WARNING "MSAN only supported with Clang")
        else()
            list(APPEND SANITIZERS "memory")
        endif()
    endif()

    if(SANITIZERS)
        list(JOIN SANITIZERS "," SANITIZERS_STR)

        target_compile_options(${target} PRIVATE
            -fsanitize=${SANITIZERS_STR}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
        )

        target_link_options(${target} PRIVATE
            -fsanitize=${SANITIZERS_STR}
        )

        # 设置运行时选项
        if(ENABLE_ASAN)
            set_property(TARGET ${target} APPEND PROPERTY
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1:halt_on_error=0"
            )
        endif()

        if(ENABLE_TSAN)
            set_property(TARGET ${target} APPEND PROPERTY
                ENVIRONMENT "TSAN_OPTIONS=halt_on_error=0"
            )
        endif()

        if(ENABLE_UBSAN)
            set_property(TARGET ${target} APPEND PROPERTY
                ENVIRONMENT "UBSAN_OPTIONS=print_stacktrace=1"
            )
        endif()
    endif()
endfunction()

# 使用
# enable_sanitizers(mytarget)
```

**GitHub Actions集成**：

```yaml
# .github/workflows/sanitizers.yml
name: Sanitizer Tests

on:
  push:
    branches: [main]
  pull_request:

jobs:
  asan:
    name: AddressSanitizer
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: |
          cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_ASAN=ON \
            -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build

      - name: Test
        run: ctest --test-dir build --output-on-failure
        env:
          ASAN_OPTIONS: detect_leaks=1:halt_on_error=1

  tsan:
    name: ThreadSanitizer
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: |
          cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_TSAN=ON \
            -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build

      - name: Test
        run: ctest --test-dir build --output-on-failure
        env:
          TSAN_OPTIONS: halt_on_error=1

  ubsan:
    name: UndefinedBehaviorSanitizer
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: |
          cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_UBSAN=ON \
            -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build

      - name: Test
        run: ctest --test-dir build --output-on-failure
        env:
          UBSAN_OPTIONS: print_stacktrace=1:halt_on_error=1
```

---

## 源码阅读任务

### 本月源码阅读

1. **LLVM Sanitizer源码**
   - 路径：`compiler-rt/lib/asan/`
   - 重点：`asan_interceptors.cpp`
   - 学习目标：理解内存操作拦截原理

2. **Sanitizer报告解析器**
   - 工具：`llvm-symbolizer`
   - 学习目标：理解符号化过程

3. **知名项目的Sanitizer配置**
   - Chromium的Sanitizer使用
   - LLVM自身的测试配置

---

## 实践项目

### 项目：Sanitizer集成测试框架

创建一个自动运行Sanitizer测试并生成报告的框架。

**项目结构**：

```
sanitizer-framework/
├── CMakeLists.txt
├── cmake/
│   └── Sanitizers.cmake
├── include/
│   └── sanitizer_helpers.hpp
├── src/
│   └── sanitizer_helpers.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_memory.cpp
│   ├── test_threading.cpp
│   └── test_undefined.cpp
├── examples/
│   ├── memory_bugs.cpp
│   ├── race_conditions.cpp
│   └── undefined_behavior.cpp
└── scripts/
    ├── run_all_sanitizers.sh
    └── parse_sanitizer_output.py
```

**include/sanitizer_helpers.hpp**：

```cpp
#pragma once

#include <string>
#include <functional>
#include <stdexcept>

namespace sanitizer {

/**
 * @brief 检查是否在特定Sanitizer下运行
 */
bool is_asan_enabled();
bool is_tsan_enabled();
bool is_ubsan_enabled();
bool is_msan_enabled();

/**
 * @brief 抑制特定的Sanitizer检查
 */
class ScopedSanitizerDisable {
public:
    explicit ScopedSanitizerDisable(const std::string& check);
    ~ScopedSanitizerDisable();

private:
    std::string check_;
};

/**
 * @brief 标记预期的Sanitizer错误
 */
void expect_sanitizer_error(const std::string& type);

/**
 * @brief Sanitizer感知的内存分配器
 */
template<typename T>
class SanitizerAwareAllocator {
public:
    using value_type = T;

    T* allocate(std::size_t n);
    void deallocate(T* p, std::size_t n);

    // 标记内存为已初始化（用于MSan）
    static void mark_initialized(void* ptr, std::size_t size);

    // 标记内存为未初始化
    static void mark_uninitialized(void* ptr, std::size_t size);

    // 检查内存是否初始化
    static bool is_initialized(const void* ptr, std::size_t size);
};

/**
 * @brief ASan特定功能
 */
namespace asan {
    // 手动毒化内存区域
    void poison_memory_region(void* addr, std::size_t size);
    void unpoison_memory_region(void* addr, std::size_t size);

    // 检查地址是否可访问
    bool is_accessible(const void* addr, std::size_t size);

    // 描述地址
    std::string describe_address(const void* addr);
}

/**
 * @brief TSan特定功能
 */
namespace tsan {
    // 标记acquire/release语义
    void acquire(void* addr);
    void release(void* addr);

    // 标记happens-before关系
    void happens_before(void* addr);
    void happens_after(void* addr);

    // 忽略特定同步
    class ScopedIgnoreSync {
    public:
        ScopedIgnoreSync();
        ~ScopedIgnoreSync();
    };
}

} // namespace sanitizer
```

**src/sanitizer_helpers.cpp**：

```cpp
#include "sanitizer_helpers.hpp"

// Sanitizer接口头文件
#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
#define HAS_ASAN 1
#else
#define HAS_ASAN 0
#endif

#if __has_feature(thread_sanitizer) || defined(__SANITIZE_THREAD__)
#include <sanitizer/tsan_interface.h>
#define HAS_TSAN 1
#else
#define HAS_TSAN 0
#endif

#if __has_feature(memory_sanitizer)
#include <sanitizer/msan_interface.h>
#define HAS_MSAN 1
#else
#define HAS_MSAN 0
#endif

namespace sanitizer {

bool is_asan_enabled() {
#if HAS_ASAN
    return true;
#else
    return false;
#endif
}

bool is_tsan_enabled() {
#if HAS_TSAN
    return true;
#else
    return false;
#endif
}

bool is_ubsan_enabled() {
    // UBSan没有运行时检测API
    return false;
}

bool is_msan_enabled() {
#if HAS_MSAN
    return true;
#else
    return false;
#endif
}

// ASan功能
namespace asan {

void poison_memory_region(void* addr, std::size_t size) {
#if HAS_ASAN
    ASAN_POISON_MEMORY_REGION(addr, size);
#else
    (void)addr;
    (void)size;
#endif
}

void unpoison_memory_region(void* addr, std::size_t size) {
#if HAS_ASAN
    ASAN_UNPOISON_MEMORY_REGION(addr, size);
#else
    (void)addr;
    (void)size;
#endif
}

bool is_accessible(const void* addr, std::size_t size) {
#if HAS_ASAN
    return __asan_region_is_poisoned(const_cast<void*>(addr), size) == nullptr;
#else
    (void)addr;
    (void)size;
    return true;
#endif
}

std::string describe_address(const void* addr) {
#if HAS_ASAN
    char buffer[1024];
    __asan_describe_address(const_cast<void*>(addr));
    return "See ASan output";
#else
    (void)addr;
    return "ASan not enabled";
#endif
}

} // namespace asan

// TSan功能
namespace tsan {

void acquire(void* addr) {
#if HAS_TSAN
    __tsan_acquire(addr);
#else
    (void)addr;
#endif
}

void release(void* addr) {
#if HAS_TSAN
    __tsan_release(addr);
#else
    (void)addr;
#endif
}

void happens_before(void* addr) {
    release(addr);
}

void happens_after(void* addr) {
    acquire(addr);
}

ScopedIgnoreSync::ScopedIgnoreSync() {
#if HAS_TSAN
    __tsan_ignore_thread_begin();
#endif
}

ScopedIgnoreSync::~ScopedIgnoreSync() {
#if HAS_TSAN
    __tsan_ignore_thread_end();
#endif
}

} // namespace tsan

// 内存分配器
template<typename T>
T* SanitizerAwareAllocator<T>::allocate(std::size_t n) {
    T* ptr = static_cast<T*>(::operator new(n * sizeof(T)));
#if HAS_MSAN
    __msan_allocated_memory(ptr, n * sizeof(T));
#endif
    return ptr;
}

template<typename T>
void SanitizerAwareAllocator<T>::deallocate(T* p, std::size_t n) {
    ::operator delete(p);
    (void)n;
}

template<typename T>
void SanitizerAwareAllocator<T>::mark_initialized(void* ptr, std::size_t size) {
#if HAS_MSAN
    __msan_unpoison(ptr, size);
#else
    (void)ptr;
    (void)size;
#endif
}

template<typename T>
void SanitizerAwareAllocator<T>::mark_uninitialized(void* ptr, std::size_t size) {
#if HAS_MSAN
    __msan_poison(ptr, size);
#else
    (void)ptr;
    (void)size;
#endif
}

template<typename T>
bool SanitizerAwareAllocator<T>::is_initialized(const void* ptr, std::size_t size) {
#if HAS_MSAN
    return __msan_test_shadow(ptr, size) == -1;
#else
    (void)ptr;
    (void)size;
    return true;
#endif
}

// 显式实例化
template class SanitizerAwareAllocator<char>;
template class SanitizerAwareAllocator<int>;

} // namespace sanitizer
```

**tests/test_memory.cpp**：

```cpp
#include <gtest/gtest.h>
#include "sanitizer_helpers.hpp"
#include <vector>
#include <memory>

class MemorySanitizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 记录是否启用了ASan
        asan_enabled_ = sanitizer::is_asan_enabled();
    }

    bool asan_enabled_ = false;
};

// 这些测试故意包含内存错误，用于验证ASan能正确检测

TEST_F(MemorySanitizerTest, HeapBufferOverflow) {
    if (!asan_enabled_) {
        GTEST_SKIP() << "ASan not enabled";
    }

    // 这个测试会被ASan检测到
    // std::vector<int> v(10);
    // v[10] = 42;  // 越界

    // 正确的代码
    std::vector<int> v(10);
    v[9] = 42;
    EXPECT_EQ(v[9], 42);
}

TEST_F(MemorySanitizerTest, StackBufferOverflow) {
    if (!asan_enabled_) {
        GTEST_SKIP() << "ASan not enabled";
    }

    int arr[10];
    for (int i = 0; i < 10; ++i) {
        arr[i] = i;
    }
    EXPECT_EQ(arr[9], 9);
}

TEST_F(MemorySanitizerTest, UseAfterFree) {
    if (!asan_enabled_) {
        GTEST_SKIP() << "ASan not enabled";
    }

    auto ptr = std::make_unique<int>(42);
    int value = *ptr;
    ptr.reset();
    // *ptr = 100;  // 这会被ASan检测到
    EXPECT_EQ(value, 42);
}

TEST_F(MemorySanitizerTest, MemoryLeak) {
    if (!asan_enabled_) {
        GTEST_SKIP() << "ASan not enabled";
    }

    // 故意泄漏（会被LeakSanitizer检测）
    // int* leak = new int(42);

    // 正确的代码
    auto ptr = std::make_unique<int>(42);
    EXPECT_EQ(*ptr, 42);
}

TEST_F(MemorySanitizerTest, DoubleFree) {
    if (!asan_enabled_) {
        GTEST_SKIP() << "ASan not enabled";
    }

    // 双重释放会被检测
    // int* p = new int(42);
    // delete p;
    // delete p;

    // 正确的代码
    auto ptr = std::make_unique<int>(42);
    EXPECT_EQ(*ptr, 42);
}

// 使用ASan API进行自定义检测
TEST_F(MemorySanitizerTest, ManualPoisoning) {
    if (!asan_enabled_) {
        GTEST_SKIP() << "ASan not enabled";
    }

    char buffer[100];

    // 初始状态：可访问
    EXPECT_TRUE(sanitizer::asan::is_accessible(buffer, 100));

    // 毒化一部分
    sanitizer::asan::poison_memory_region(buffer + 50, 50);

    // 前50字节仍可访问
    EXPECT_TRUE(sanitizer::asan::is_accessible(buffer, 50));

    // 后50字节被毒化
    // EXPECT_FALSE(sanitizer::asan::is_accessible(buffer + 50, 50));

    // 解毒
    sanitizer::asan::unpoison_memory_region(buffer + 50, 50);
    EXPECT_TRUE(sanitizer::asan::is_accessible(buffer, 100));
}
```

**scripts/run_all_sanitizers.sh**：

```bash
#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Running all sanitizer tests...${NC}"

# ASan测试
echo -e "\n${YELLOW}=== AddressSanitizer ===${NC}"
cmake -B "$BUILD_DIR/asan" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_ASAN=ON \
    -DBUILD_TESTS=ON
cmake --build "$BUILD_DIR/asan" --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=0 \
    ctest --test-dir "$BUILD_DIR/asan" --output-on-failure || true

# TSan测试
echo -e "\n${YELLOW}=== ThreadSanitizer ===${NC}"
cmake -B "$BUILD_DIR/tsan" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_TSAN=ON \
    -DBUILD_TESTS=ON
cmake --build "$BUILD_DIR/tsan" --parallel
TSAN_OPTIONS=halt_on_error=0 \
    ctest --test-dir "$BUILD_DIR/tsan" --output-on-failure || true

# UBSan测试
echo -e "\n${YELLOW}=== UndefinedBehaviorSanitizer ===${NC}"
cmake -B "$BUILD_DIR/ubsan" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_UBSAN=ON \
    -DBUILD_TESTS=ON
cmake --build "$BUILD_DIR/ubsan" --parallel
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 \
    ctest --test-dir "$BUILD_DIR/ubsan" --output-on-failure || true

echo -e "\n${GREEN}All sanitizer tests completed!${NC}"
```

---

## 检验标准

- [ ] 能够使用ASan检测内存错误
- [ ] 能够使用TSan检测数据竞争
- [ ] 能够使用UBSan检测未定义行为
- [ ] 能够分析Sanitizer输出报告
- [ ] 能够将Sanitizer集成到CI/CD
- [ ] 理解各Sanitizer的性能影响

### 知识检验问题

1. ASan和TSan为什么不能同时使用？
2. LeakSanitizer是如何检测内存泄漏的？
3. 如何抑制已知的Sanitizer警告？
4. 使用Sanitizer时的性能开销是多少？

### Sanitizer对比

| 特性 | ASan | TSan | UBSan | MSan |
|------|------|------|-------|------|
| 内存错误 | 是 | 否 | 否 | 是 |
| 数据竞争 | 否 | 是 | 否 | 否 |
| 未定义行为 | 否 | 否 | 是 | 否 |
| 未初始化 | 否 | 否 | 部分 | 是 |
| 性能开销 | 2-3x | 5-15x | 1.2x | 3x |
| 内存开销 | 3x | 5-10x | 小 | 2x |
| 兼容性 | 独占 | 独占 | 可组合 | 独占 |

---

## 输出物清单

1. **框架代码**
   - `sanitizer-framework/` - 完整的集成框架

2. **CMake模块**
   - `cmake/Sanitizers.cmake` - 可复用模块

3. **文档**
   - `notes/month44_sanitizers.md` - 学习笔记
   - `notes/sanitizer_guide.md` - 使用指南

4. **脚本**
   - `scripts/run_all_sanitizers.sh`
   - CI/CD配置文件

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | AddressSanitizer | 15h | 15h | 5h |
| 第2周 | ThreadSanitizer | 12h | 18h | 5h |
| 第3周 | UBSan | 10h | 20h | 5h |
| 第4周 | MSan与集成 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 45将学习**性能分析与Profiling**，掌握CPU、内存和I/O性能分析工具的使用。
