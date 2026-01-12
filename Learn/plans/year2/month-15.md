# Month 15: 原子操作与CAS——无锁编程的基石

## 本月主题概述

原子操作是并发编程的基本构建块，而Compare-And-Swap（CAS）是实现无锁算法的核心原语。本月将深入理解各种原子操作的语义、实现和应用，为无锁数据结构的学习打下基础。

---

## 理论学习内容

### 第一周：原子操作基础

**学习目标**：掌握std::atomic的完整API

**阅读材料**：
- [ ] 《C++ Concurrency in Action》第5章原子操作部分
- [ ] cppreference std::atomic完整文档
- [ ] Intel/ARM手册中的原子指令部分

**核心概念**：

#### std::atomic的特化
```cpp
#include <atomic>

// 通用模板
template <typename T>
struct atomic;

// 整数类型特化：提供算术操作
std::atomic<int> ai;
ai.fetch_add(1);  // 原子加
ai.fetch_sub(1);  // 原子减
ai.fetch_and(mask);  // 原子与
ai.fetch_or(mask);   // 原子或
ai.fetch_xor(mask);  // 原子异或

// 指针特化：提供指针算术
std::atomic<int*> ap;
ap.fetch_add(1);  // 指针+1（即+sizeof(int)字节）
ap.fetch_sub(1);

// 布尔特化
std::atomic<bool> ab;
// 没有算术操作

// atomic_flag：最简单的原子类型
std::atomic_flag flag = ATOMIC_FLAG_INIT;
flag.test_and_set();  // 设置并返回旧值
flag.clear();         // 清除
// 保证无锁！
```

#### 原子性保证
```cpp
// 原子操作的三个层次：

// 1. 无锁（Lock-free）
// 操作直接映射到硬件原子指令
std::atomic<int> a;
static_assert(a.is_lock_free());  // 通常为true

// 2. 地址无锁（Address-free）
// 同一地址的操作是无锁的

// 3. 有锁实现
// 对于大对象或不支持的类型，可能使用内部锁
struct BigStruct { int data[100]; };
std::atomic<BigStruct> big;
// 可能不是lock-free

// 检查是否无锁
std::cout << std::atomic<int>::is_always_lock_free << "\n";  // 编译期
std::cout << a.is_lock_free() << "\n";  // 运行时
```

### 第二周：Compare-And-Swap深度

**学习目标**：彻底理解CAS的语义和用法

#### CAS基本原理
```cpp
// CAS的语义（伪代码）：
bool compare_and_swap(T* ptr, T expected, T desired) {
    if (*ptr == expected) {
        *ptr = desired;
        return true;
    }
    return false;
}
// 整个操作是原子的！

// C++中的两个版本
std::atomic<int> value{5};

// compare_exchange_strong
int expected = 5;
bool success = value.compare_exchange_strong(expected, 10);
// 如果value==5，则设为10，返回true
// 如果value!=5，则expected被更新为当前value，返回false

// compare_exchange_weak
// 可能虚假失败（spurious failure）
// 即使value==expected也可能返回false
// 但在某些架构上更高效
// 通常在循环中使用
```

#### CAS循环模式
```cpp
// 原子地将value翻倍
std::atomic<int> value{5};

void double_value() {
    int expected = value.load();
    while (!value.compare_exchange_weak(expected, expected * 2)) {
        // expected已被更新为当前值，继续尝试
    }
}

// 通用的Read-Modify-Write模式
template <typename T, typename F>
T atomic_update(std::atomic<T>& atom, F&& f) {
    T expected = atom.load(std::memory_order_relaxed);
    T desired;
    do {
        desired = f(expected);
    } while (!atom.compare_exchange_weak(expected, desired,
                std::memory_order_release,
                std::memory_order_relaxed));
    return desired;
}

// 使用
atomic_update(value, [](int x) { return x * 2; });
```

#### strong vs weak
```cpp
// compare_exchange_strong:
// - 只有value != expected时才失败
// - 适合单次尝试场景

// compare_exchange_weak:
// - 可能虚假失败
// - 在LL/SC架构（ARM/POWER）上更高效
// - 适合循环中使用

// 何时用weak？
// - 在循环中（反正要重试）
// - 性能关键路径

// 何时用strong？
// - 单次尝试
// - 失败后有复杂逻辑（不想虚假执行）

// LL/SC架构原理：
// Load-Linked: 读取值并设置监视
// Store-Conditional: 只有监视未被破坏时才写入
// 任何对该地址的其他写入都会破坏监视
// 因此可能"虚假失败"
```

### 第三周：高级原子操作

**学习目标**：掌握fetch_*操作和内存栅栏

#### Fetch操作家族
```cpp
std::atomic<int> counter{0};

// 所有fetch_*操作返回旧值
int old = counter.fetch_add(1);  // old=0, counter=1
old = counter.fetch_sub(1);       // old=1, counter=0

// 位操作
std::atomic<unsigned> flags{0};
flags.fetch_or(0x01);   // 设置bit 0
flags.fetch_and(~0x01); // 清除bit 0
flags.fetch_xor(0x01);  // 翻转bit 0

// 与++/--的区别
counter++;        // 返回旧值（但通常被丢弃）
++counter;        // 返回新值
counter.fetch_add(1);  // 返回旧值

// 指定内存序
counter.fetch_add(1, std::memory_order_relaxed);
counter.fetch_add(1, std::memory_order_acq_rel);
```

#### 原子交换
```cpp
std::atomic<int> value{5};

// exchange: 设置新值，返回旧值
int old = value.exchange(10);  // old=5, value=10

// 常用于实现自旋锁
std::atomic<bool> locked{false};

void lock() {
    while (locked.exchange(true, std::memory_order_acquire)) {
        // 自旋
    }
}

void unlock() {
    locked.store(false, std::memory_order_release);
}
```

#### 原子栅栏（Fence）
```cpp
// 独立于原子变量的内存栅栏

std::atomic_thread_fence(std::memory_order_acquire);
// 等价于在所有之前的relaxed load后加acquire

std::atomic_thread_fence(std::memory_order_release);
// 等价于在所有之后的relaxed store前加release

std::atomic_thread_fence(std::memory_order_seq_cst);
// 完整栅栏

// 信号栅栏（用于信号处理器和主线程通信）
std::atomic_signal_fence(std::memory_order_seq_cst);
// 只阻止编译器重排，不发出硬件栅栏
```

### 第四周：CAS应用模式

**学习目标**：学习CAS的常见应用模式

#### 无锁计数器
```cpp
class LockFreeCounter {
    std::atomic<int64_t> count_{0};

public:
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }

    void decrement() {
        count_.fetch_sub(1, std::memory_order_relaxed);
    }

    int64_t get() const {
        return count_.load(std::memory_order_relaxed);
    }

    // 条件增加
    bool try_increment_if_below(int64_t limit) {
        int64_t current = count_.load(std::memory_order_relaxed);
        while (current < limit) {
            if (count_.compare_exchange_weak(current, current + 1,
                    std::memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }
};
```

#### 无锁标志位
```cpp
class AtomicFlags {
    std::atomic<uint32_t> flags_{0};

public:
    bool set_flag(int bit) {
        uint32_t old = flags_.fetch_or(1u << bit, std::memory_order_acq_rel);
        return !(old & (1u << bit));  // 返回之前是否未设置
    }

    bool clear_flag(int bit) {
        uint32_t old = flags_.fetch_and(~(1u << bit), std::memory_order_acq_rel);
        return old & (1u << bit);  // 返回之前是否已设置
    }

    bool test_flag(int bit) const {
        return flags_.load(std::memory_order_acquire) & (1u << bit);
    }

    // 原子地设置一个标志，清除另一个
    void set_and_clear(int set_bit, int clear_bit) {
        uint32_t expected = flags_.load(std::memory_order_relaxed);
        uint32_t desired;
        do {
            desired = (expected | (1u << set_bit)) & ~(1u << clear_bit);
        } while (!flags_.compare_exchange_weak(expected, desired,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed));
    }
};
```

#### 无锁单例
```cpp
template <typename T>
class LockFreeSingleton {
    static std::atomic<T*> instance_;

public:
    static T* get() {
        T* ptr = instance_.load(std::memory_order_acquire);
        if (ptr == nullptr) {
            T* new_instance = new T();
            if (!instance_.compare_exchange_strong(ptr, new_instance,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                // 其他线程先创建了，删除我们的
                delete new_instance;
                // ptr已被更新为其他线程创建的实例
            } else {
                ptr = new_instance;
            }
        }
        return ptr;
    }
};

template <typename T>
std::atomic<T*> LockFreeSingleton<T>::instance_{nullptr};
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] GCC/Clang的`__atomic_*`内置函数
- [ ] x86的`LOCK`前缀指令
- [ ] ARM的`LDREX/STREX`和`LDADD`等指令
- [ ] folly/AtomicHashMap的CAS使用

---

## 实践项目

### 项目：无锁数据结构基础

#### Part 1: 无锁栈（Treiber Stack）
```cpp
// lockfree_stack.hpp
#pragma once
#include <atomic>
#include <memory>
#include <optional>

template <typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;

        template <typename... Args>
        Node(Args&&... args) : data(std::forward<Args>(args)...), next(nullptr) {}
    };

    std::atomic<Node*> head_{nullptr};

public:
    ~LockFreeStack() {
        while (pop()) {}
    }

    void push(T value) {
        Node* new_node = new Node(std::move(value));
        new_node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(new_node->next, new_node,
                std::memory_order_release,
                std::memory_order_relaxed)) {
            // new_node->next已被更新为当前head
        }
    }

    std::optional<T> pop() {
        Node* old_head = head_.load(std::memory_order_relaxed);
        while (old_head != nullptr) {
            if (head_.compare_exchange_weak(old_head, old_head->next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                T value = std::move(old_head->data);
                delete old_head;  // 危险！其他线程可能正在读取
                return value;
            }
        }
        return std::nullopt;
    }

    bool empty() const {
        return head_.load(std::memory_order_relaxed) == nullptr;
    }
};

// 注意：上面的实现有ABA问题和内存回收问题
// 下个月会学习如何解决
```

#### Part 2: 原子指针包装器
```cpp
// atomic_shared_ptr.hpp
#pragma once
#include <atomic>
#include <memory>

// 简化版原子shared_ptr（C++20有std::atomic<std::shared_ptr<T>>）
template <typename T>
class AtomicSharedPtr {
    // 使用tagged pointer或分离计数方案
    // 这里使用简化的自旋锁方案

    std::atomic<std::shared_ptr<T>*> ptr_{nullptr};
    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;

    void acquire_lock() const {
        while (lock_.test_and_set(std::memory_order_acquire)) {
            // 自旋
        }
    }

    void release_lock() const {
        lock_.clear(std::memory_order_release);
    }

public:
    AtomicSharedPtr() = default;

    explicit AtomicSharedPtr(std::shared_ptr<T> ptr) {
        store(std::move(ptr));
    }

    ~AtomicSharedPtr() {
        auto p = ptr_.load(std::memory_order_relaxed);
        if (p) delete p;
    }

    std::shared_ptr<T> load() const {
        acquire_lock();
        auto p = ptr_.load(std::memory_order_relaxed);
        std::shared_ptr<T> result = p ? *p : nullptr;
        release_lock();
        return result;
    }

    void store(std::shared_ptr<T> desired) {
        auto new_ptr = new std::shared_ptr<T>(std::move(desired));
        acquire_lock();
        auto old = ptr_.exchange(new_ptr, std::memory_order_relaxed);
        release_lock();
        if (old) delete old;
    }

    std::shared_ptr<T> exchange(std::shared_ptr<T> desired) {
        auto new_ptr = new std::shared_ptr<T>(std::move(desired));
        acquire_lock();
        auto old = ptr_.exchange(new_ptr, std::memory_order_relaxed);
        std::shared_ptr<T> result = old ? std::move(*old) : nullptr;
        release_lock();
        if (old) delete old;
        return result;
    }

    bool compare_exchange_strong(std::shared_ptr<T>& expected,
                                  std::shared_ptr<T> desired) {
        acquire_lock();
        auto p = ptr_.load(std::memory_order_relaxed);
        std::shared_ptr<T> current = p ? *p : nullptr;

        if (current == expected) {
            auto new_ptr = new std::shared_ptr<T>(std::move(desired));
            ptr_.store(new_ptr, std::memory_order_relaxed);
            release_lock();
            if (p) delete p;
            return true;
        } else {
            expected = current;
            release_lock();
            return false;
        }
    }
};
```

#### Part 3: 双字CAS（DCAS）模拟
```cpp
// dcas.hpp
#pragma once
#include <atomic>
#include <cstdint>

// 在64位系统上，可以用128位CAS
// 在32位系统上，可以用64位CAS
// 这里演示64位系统的实现

struct alignas(16) DoubleWord {
    void* ptr;
    uint64_t counter;

    bool operator==(const DoubleWord& other) const {
        return ptr == other.ptr && counter == other.counter;
    }
};

// 检查平台是否支持
static_assert(sizeof(DoubleWord) == 16, "DoubleWord must be 16 bytes");

class AtomicDoubleWord {
    // 注意：需要16字节对齐
    alignas(16) DoubleWord data_{nullptr, 0};

public:
    DoubleWord load() const {
        DoubleWord result;
        #if defined(__x86_64__) || defined(_M_X64)
        // x86-64: 使用CMPXCHG16B
        __atomic_load(&data_, &result, __ATOMIC_SEQ_CST);
        #elif defined(__aarch64__)
        // ARM64: 使用LDAXP/STLXP
        __atomic_load(&data_, &result, __ATOMIC_SEQ_CST);
        #else
        #error "Platform not supported"
        #endif
        return result;
    }

    void store(DoubleWord desired) {
        #if defined(__x86_64__) || defined(_M_X64)
        __atomic_store(&data_, &desired, __ATOMIC_SEQ_CST);
        #elif defined(__aarch64__)
        __atomic_store(&data_, &desired, __ATOMIC_SEQ_CST);
        #endif
    }

    bool compare_exchange_strong(DoubleWord& expected, DoubleWord desired) {
        #if defined(__x86_64__) || defined(_M_X64)
        return __atomic_compare_exchange(&data_, &expected, &desired,
                                         false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        #elif defined(__aarch64__)
        return __atomic_compare_exchange(&data_, &expected, &desired,
                                         false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        #endif
    }
};
```

#### Part 4: CAS性能基准测试
```cpp
// cas_benchmark.cpp
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

std::atomic<int> counter{0};

void bench_fetch_add(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

void bench_cas_loop(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        int expected = counter.load(std::memory_order_relaxed);
        while (!counter.compare_exchange_weak(expected, expected + 1,
                std::memory_order_relaxed)) {
        }
    }
}

void bench_cas_strong(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        int expected = counter.load(std::memory_order_relaxed);
        while (!counter.compare_exchange_strong(expected, expected + 1,
                std::memory_order_relaxed)) {
        }
    }
}

template <typename Func>
void run_benchmark(const char* name, Func f, int threads, int iterations) {
    counter = 0;
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    for (int i = 0; i < threads; ++i) {
        workers.emplace_back(f, iterations);
    }
    for (auto& w : workers) {
        w.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << name << ": " << duration.count() << " ms"
              << ", counter = " << counter
              << " (expected: " << threads * iterations << ")\n";
}

int main() {
    const int threads = 4;
    const int iterations = 1000000;

    std::cout << "Running with " << threads << " threads, "
              << iterations << " iterations each\n\n";

    run_benchmark("fetch_add", bench_fetch_add, threads, iterations);
    run_benchmark("CAS weak", bench_cas_loop, threads, iterations);
    run_benchmark("CAS strong", bench_cas_strong, threads, iterations);

    return 0;
}
```

---

## 检验标准

### 知识检验
- [ ] compare_exchange_weak和strong的区别是什么？
- [ ] 为什么CAS循环中通常使用weak？
- [ ] fetch_add和CAS循环实现加法有什么区别？
- [ ] LL/SC架构是什么？为什么会有虚假失败？
- [ ] 双字CAS的用途是什么？

### 实践检验
- [ ] 无锁栈的push和pop正确工作
- [ ] 原子操作的内存序选择正确
- [ ] 基准测试展示不同方法的性能差异

### 输出物
1. `lockfree_stack.hpp`
2. `atomic_shared_ptr.hpp`
3. `dcas.hpp`
4. `cas_benchmark.cpp`
5. `notes/month15_atomic_cas.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 35小时 | 25% |
| 源码阅读 | 25小时 | 18% |
| 无锁数据结构实现 | 45小时 | 32% |
| 基准测试 | 20小时 | 14% |
| 笔记与文档 | 15小时 | 11% |

---

## 下月预告

Month 16将学习**ABA问题与内存回收**，这是无锁编程中最棘手的问题。我们将学习危险指针、Epoch-based回收和引用计数等解决方案。
