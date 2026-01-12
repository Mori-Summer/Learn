# Month 14: C++内存模型——穿透硬件迷雾

## 本月主题概述

C++内存模型是并发编程最难理解的部分，也是区分"使用多线程"和"理解多线程"的关键。本月将深入学习顺序一致性、内存序的六种模式，以及它们如何映射到实际硬件。

---

## 理论学习内容

### 第一周：为什么需要内存模型？

**学习目标**：理解内存模型存在的根本原因

**阅读材料**：
- [ ] 《C++ Concurrency in Action》第5章
- [ ] 博客：Preshing "Memory Barriers Are Like Source Control Operations"
- [ ] 论文：Boehm & Adve "Foundations of the C++ Concurrency Memory Model"

**核心概念**：

#### 编译器和CPU的重排序
```cpp
// 源代码
int x = 0, y = 0;

void thread1() {
    x = 1;  // (1)
    y = 2;  // (2)
}

// 编译器可能重排为：
void thread1_reordered() {
    y = 2;  // (2) 先执行
    x = 1;  // (1) 后执行
}

// 为什么？因为编译器只保证单线程语义
// 在单线程中，(1)(2)的顺序不影响结果
// 但在多线程中，另一个线程可能观察到y=2但x=0
```

#### CPU乱序执行
```cpp
// 即使编译器不重排，CPU也可能乱序执行

// CPU优化：
// 1. 指令流水线
// 2. 乱序执行（Out-of-Order Execution）
// 3. 推测执行（Speculative Execution）
// 4. Store Buffer（写缓冲区）
// 5. 缓存一致性延迟

// 经典例子：Dekker算法失败
int flag1 = 0, flag2 = 0;
int turn = 0;

// Thread 1
void thread1() {
    flag1 = 1;           // 写入可能在store buffer中
    if (flag2 == 0) {    // 读取可能先执行
        // 临界区
    }
}

// Thread 2
void thread2() {
    flag2 = 1;
    if (flag1 == 0) {
        // 临界区
    }
}

// 在x86上可能两个线程都进入临界区！
// 因为store buffer导致写操作延迟可见
```

#### 不同架构的内存模型强度
```
强内存模型 ←──────────────────────→ 弱内存模型

x86/x64          ARM/POWER           DEC Alpha
(TSO)            (弱序)              (最弱)

- 只有StoreLoad    - 所有重排都可能      - 甚至有dependent
  重排可能发生      发生                  load重排
```

### 第二周：C++内存序（Memory Order）

**学习目标**：掌握六种内存序的语义

```cpp
#include <atomic>

// C++定义了6种内存序
enum memory_order {
    memory_order_relaxed,    // 最弱
    memory_order_consume,    // 弱（不推荐使用）
    memory_order_acquire,    // 获取
    memory_order_release,    // 释放
    memory_order_acq_rel,    // 获取+释放
    memory_order_seq_cst     // 最强（默认）
};
```

#### memory_order_seq_cst（顺序一致性）
```cpp
// 最强保证：
// 1. 所有线程看到的原子操作顺序一致
// 2. 存在一个全局总顺序

std::atomic<bool> x{false}, y{false};
std::atomic<int> z{0};

void write_x() { x.store(true); }  // 默认seq_cst
void write_y() { y.store(true); }

void read_x_then_y() {
    while (!x.load());  // 等待x为true
    if (y.load()) ++z;
}

void read_y_then_x() {
    while (!y.load());  // 等待y为true
    if (x.load()) ++z;
}

// 四个线程分别执行上述四个函数
// 最终z至少为1

// 但seq_cst有性能开销：
// 在x86上，store需要MFENCE或使用XCHG
// 在ARM上，需要DMB（数据内存屏障）
```

#### memory_order_acquire / memory_order_release
```cpp
// Release-Acquire语义：建立同步关系

std::atomic<int> data{0};
std::atomic<bool> ready{false};

void producer() {
    data.store(42, std::memory_order_relaxed);  // (1)
    ready.store(true, std::memory_order_release);  // (2) release
}

void consumer() {
    while (!ready.load(std::memory_order_acquire));  // (3) acquire
    int value = data.load(std::memory_order_relaxed);  // (4)
    assert(value == 42);  // 保证成功！
}

// 原理：
// - release保证：(1)在(2)之前完成（不会重排到后面）
// - acquire保证：(4)在(3)之后执行（不会重排到前面）
// - (2)和(3)建立"同步"关系：release的写对acquire的读可见
// - 因此(1)的结果对(4)可见
```

#### Synchronizes-With关系
```cpp
// 当一个线程的release操作被另一个线程的acquire操作读取时
// 建立synchronizes-with关系

// Release操作之前的所有写入
// 对Acquire操作之后的所有读取可见

// 这称为"单向栅栏"：
// - Release阻止之前的操作重排到后面
// - Acquire阻止之后的操作重排到前面
```

#### memory_order_relaxed
```cpp
// 最弱保证：只保证原子性，不保证顺序

std::atomic<int> counter{0};

void increment() {
    // 用于简单计数，不需要与其他操作同步
    counter.fetch_add(1, std::memory_order_relaxed);
}

// 适用场景：
// - 纯计数器，只关心最终结果
// - 引用计数（减少时需要更强的序）
// - 统计数据收集
```

### 第三周：内存屏障与硬件映射

**学习目标**：理解内存序如何映射到硬件指令

#### 内存屏障类型
```cpp
// 概念上的四种屏障：
// LoadLoad:   阻止Load重排到后面的Load之后
// LoadStore:  阻止Load重排到后面的Store之后
// StoreLoad:  阻止Store重排到后面的Load之后（最强）
// StoreStore: 阻止Store重排到后面的Store之后

// C++内存序映射：
// acquire = LoadLoad + LoadStore
// release = LoadStore + StoreStore
// seq_cst = 全部四种
```

#### x86/x64上的映射
```cpp
// x86是强内存模型（TSO: Total Store Order）
// 只有StoreLoad重排可能发生

// acquire: 不需要额外指令（硬件保证）
// release: 不需要额外指令（硬件保证）
// seq_cst load: 普通load
// seq_cst store: MFENCE; MOV 或 XCHG

// 因此在x86上，acquire/release几乎免费
// 只有seq_cst store有额外开销
```

#### ARM上的映射
```cpp
// ARM是弱内存模型，所有重排都可能发生

// acquire:
//   LDR r0, [address]
//   DMB ISH  ; 数据内存屏障

// release:
//   DMB ISH
//   STR r0, [address]

// seq_cst:
//   DMB ISH
//   LDR/STR
//   DMB ISH

// ARMv8有专门的acquire/release指令：
// LDAR (Load-Acquire)
// STLR (Store-Release)
```

### 第四周：实际应用与常见模式

**学习目标**：学会在实际代码中正确使用内存序

#### 自旋锁实现
```cpp
class SpinLock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        while (locked_.exchange(true, std::memory_order_acquire)) {
            // 自旋等待
            // 可以加入退避策略
            while (locked_.load(std::memory_order_relaxed)) {
                // 减少缓存行争用
            }
        }
    }

    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};

// 为什么用acquire/release而不是seq_cst？
// - 性能更好
// - 语义足够：lock获取(acquire)，unlock释放(release)
```

#### 双重检查锁定（DCLP）
```cpp
class Singleton {
    static std::atomic<Singleton*> instance_;
    static std::mutex mutex_;

public:
    static Singleton* getInstance() {
        Singleton* tmp = instance_.load(std::memory_order_acquire);
        if (tmp == nullptr) {
            std::lock_guard<std::mutex> lock(mutex_);
            tmp = instance_.load(std::memory_order_relaxed);
            if (tmp == nullptr) {
                tmp = new Singleton();
                instance_.store(tmp, std::memory_order_release);
            }
        }
        return tmp;
    }
};

// 更简单的方式：C++11静态局部变量保证线程安全
Singleton& getInstance() {
    static Singleton instance;
    return instance;
}
```

#### 引用计数（shared_ptr风格）
```cpp
class RefCounted {
    mutable std::atomic<int> ref_count_{1};

public:
    void add_ref() const {
        // 增加引用计数：relaxed足够
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void release() const {
        // 减少引用计数：需要更强的序
        if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // 最后一个引用，删除对象
            // acq_rel确保：
            // - acquire: 看到其他线程对对象的所有修改
            // - release: 确保本线程的修改对清理代码可见
            delete this;
        }
    }
};
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `std::atomic`的实现（GCC/Clang）
- [ ] `__atomic_*` 内置函数
- [ ] `std::atomic_thread_fence`实现
- [ ] Linux内核的内存屏障宏

---

## 实践项目

### 项目：实现各种同步原语

#### Part 1: 自旋锁变体
```cpp
// spinlock.hpp
#pragma once
#include <atomic>
#include <thread>

// 基本自旋锁
class SpinLock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // 自旋
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }

    bool try_lock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }
};

// 带退避的自旋锁
class BackoffSpinLock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        int backoff = 1;
        while (true) {
            // 先尝试快速获取
            if (!locked_.exchange(true, std::memory_order_acquire)) {
                return;
            }

            // 自旋等待，使用relaxed减少总线流量
            while (locked_.load(std::memory_order_relaxed)) {
                for (int i = 0; i < backoff; ++i) {
                    // 暂停指令，降低功耗
                    #if defined(__x86_64__) || defined(_M_X64)
                    __builtin_ia32_pause();
                    #elif defined(__aarch64__)
                    asm volatile("yield");
                    #endif
                }
                // 指数退避
                backoff = std::min(backoff * 2, 1024);
            }
        }
    }

    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};

// 票据自旋锁（公平）
class TicketSpinLock {
    std::atomic<size_t> next_ticket_{0};
    std::atomic<size_t> now_serving_{0};

public:
    void lock() {
        size_t my_ticket = next_ticket_.fetch_add(1, std::memory_order_relaxed);
        while (now_serving_.load(std::memory_order_acquire) != my_ticket) {
            // 自旋
        }
    }

    void unlock() {
        now_serving_.fetch_add(1, std::memory_order_release);
    }
};
```

#### Part 2: 读写锁
```cpp
// rwlock.hpp
#pragma once
#include <atomic>
#include <thread>

class RWSpinLock {
    // 状态编码：
    // 正数: 读者数量
    // -1: 有写者
    // 0: 空闲
    std::atomic<int> state_{0};

public:
    void lock_read() {
        while (true) {
            int expected = state_.load(std::memory_order_relaxed);
            // 只有非负（没有写者）时才能获取读锁
            if (expected >= 0) {
                if (state_.compare_exchange_weak(expected, expected + 1,
                        std::memory_order_acquire,
                        std::memory_order_relaxed)) {
                    return;
                }
            } else {
                // 有写者，自旋等待
                std::this_thread::yield();
            }
        }
    }

    void unlock_read() {
        state_.fetch_sub(1, std::memory_order_release);
    }

    void lock_write() {
        while (true) {
            int expected = 0;
            // 只有空闲时才能获取写锁
            if (state_.compare_exchange_weak(expected, -1,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                return;
            }
            std::this_thread::yield();
        }
    }

    void unlock_write() {
        state_.store(0, std::memory_order_release);
    }

    bool try_lock_read() {
        int expected = state_.load(std::memory_order_relaxed);
        if (expected >= 0) {
            return state_.compare_exchange_strong(expected, expected + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed);
        }
        return false;
    }

    bool try_lock_write() {
        int expected = 0;
        return state_.compare_exchange_strong(expected, -1,
            std::memory_order_acquire,
            std::memory_order_relaxed);
    }
};
```

#### Part 3: 序列锁（适合读多写少）
```cpp
// seqlock.hpp
#pragma once
#include <atomic>

template <typename T>
class SeqLock {
    std::atomic<unsigned> seq_{0};
    T data_;

public:
    // 写者（独占）
    void write(const T& value) {
        unsigned seq = seq_.load(std::memory_order_relaxed);
        seq_.store(seq + 1, std::memory_order_relaxed);  // 奇数表示写入中
        std::atomic_thread_fence(std::memory_order_release);

        data_ = value;

        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(seq + 2, std::memory_order_release);  // 偶数表示完成
    }

    // 读者（可以并发）
    T read() const {
        T result;
        unsigned seq1, seq2;
        do {
            seq1 = seq_.load(std::memory_order_acquire);
            while (seq1 & 1) {  // 奇数表示有写者
                seq1 = seq_.load(std::memory_order_acquire);
            }

            std::atomic_thread_fence(std::memory_order_acquire);
            result = data_;
            std::atomic_thread_fence(std::memory_order_acquire);

            seq2 = seq_.load(std::memory_order_acquire);
        } while (seq1 != seq2);  // 如果不相等，说明读取期间有写入

        return result;
    }
};
```

#### Part 4: 内存序测试
```cpp
// memory_order_test.cpp
#include <atomic>
#include <thread>
#include <cassert>
#include <iostream>

// 测试acquire-release语义
void test_acquire_release() {
    std::atomic<int> data{0};
    std::atomic<bool> ready{false};
    int observed = -1;

    std::thread writer([&] {
        data.store(42, std::memory_order_relaxed);
        ready.store(true, std::memory_order_release);
    });

    std::thread reader([&] {
        while (!ready.load(std::memory_order_acquire)) {
            // 自旋
        }
        observed = data.load(std::memory_order_relaxed);
    });

    writer.join();
    reader.join();

    assert(observed == 42);
    std::cout << "Acquire-Release test passed!\n";
}

// 测试seq_cst（经典的存储缓冲区测试）
void test_seq_cst() {
    std::atomic<int> x{0}, y{0};
    int r1 = 0, r2 = 0;

    auto thread1 = [&] {
        x.store(1, std::memory_order_seq_cst);
        r1 = y.load(std::memory_order_seq_cst);
    };

    auto thread2 = [&] {
        y.store(1, std::memory_order_seq_cst);
        r2 = x.load(std::memory_order_seq_cst);
    };

    // 运行多次，检查是否出现r1==0 && r2==0
    int both_zero = 0;
    for (int i = 0; i < 100000; ++i) {
        x = 0; y = 0; r1 = 0; r2 = 0;

        std::thread t1(thread1);
        std::thread t2(thread2);
        t1.join();
        t2.join();

        if (r1 == 0 && r2 == 0) {
            ++both_zero;
        }
    }

    // seq_cst保证不会出现both_zero
    std::cout << "Seq_cst test: both_zero = " << both_zero
              << " (should be 0)\n";
}

// 测试relaxed的无序性
void test_relaxed() {
    std::atomic<int> x{0}, y{0};
    int r1 = 0, r2 = 0;

    auto thread1 = [&] {
        x.store(1, std::memory_order_relaxed);
        r1 = y.load(std::memory_order_relaxed);
    };

    auto thread2 = [&] {
        y.store(1, std::memory_order_relaxed);
        r2 = x.load(std::memory_order_relaxed);
    };

    int both_zero = 0;
    for (int i = 0; i < 100000; ++i) {
        x = 0; y = 0; r1 = 0; r2 = 0;

        std::thread t1(thread1);
        std::thread t2(thread2);
        t1.join();
        t2.join();

        if (r1 == 0 && r2 == 0) {
            ++both_zero;
        }
    }

    // relaxed可能出现both_zero（在弱内存模型架构上）
    std::cout << "Relaxed test: both_zero = " << both_zero
              << " (may be non-zero on weak memory models)\n";
}

int main() {
    test_acquire_release();
    test_seq_cst();
    test_relaxed();
    return 0;
}
```

---

## 检验标准

### 知识检验
- [ ] 为什么编译器和CPU会重排指令？
- [ ] x86和ARM内存模型有什么区别？
- [ ] acquire和release语义各自阻止什么重排？
- [ ] 什么时候可以安全使用relaxed？
- [ ] seq_cst的开销在哪里？

### 实践检验
- [ ] 实现的自旋锁在多线程环境下正确工作
- [ ] 读写锁能正确处理并发读和独占写
- [ ] 序列锁在写入期间读者能正确重试
- [ ] 内存序测试展示不同内存序的行为差异

### 输出物
1. `spinlock.hpp`（含多种变体）
2. `rwlock.hpp`
3. `seqlock.hpp`
4. `memory_order_test.cpp`
5. `notes/month14_memory_model.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 45小时 | 32% |
| 源码阅读 | 25小时 | 18% |
| 同步原语实现 | 40小时 | 29% |
| 测试与验证 | 20小时 | 14% |
| 笔记与文档 | 10小时 | 7% |

---

## 下月预告

Month 15将学习**原子操作与CAS**，深入理解compare_exchange的实现，以及它在无锁算法中的核心作用。
