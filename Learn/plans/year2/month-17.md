# Month 17: 无锁队列——并发数据结构的核心

## 本月主题概述

无锁队列是最实用的无锁数据结构，广泛应用于生产者-消费者模式、消息传递系统和任务调度。本月将深入学习Michael-Scott队列、MPSC/SPSC队列等经典实现。

---

## 理论学习内容

### 第一周：队列的并发问题

**学习目标**：理解并发队列的设计挑战

**阅读材料**：
- [ ] 论文："Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms"
- [ ] 《The Art of Multiprocessor Programming》队列章节
- [ ] folly::ProducerConsumerQueue源码

**核心概念**：

#### 队列操作的原子性挑战
```cpp
// 单链表队列的问题
template <typename T>
class NaiveQueue {
    struct Node {
        T data;
        Node* next;
    };
    Node* head_;  // 出队端
    Node* tail_;  // 入队端

    void enqueue(T value) {
        Node* new_node = new Node{value, nullptr};
        tail_->next = new_node;  // (1)
        tail_ = new_node;        // (2)
        // (1)和(2)不是原子的！
    }

    T dequeue() {
        Node* old_head = head_;
        head_ = head_->next;  // (3)
        T result = head_->data;
        delete old_head;
        return result;
        // 多个线程可能同时执行(3)
    }
};
```

### 第二周：Michael-Scott Queue

**学习目标**：掌握经典的无锁队列算法

```cpp
// michael_scott_queue.hpp
#pragma once
#include <atomic>
#include <optional>
#include "hazard_pointer.hpp"

template <typename T>
class MSQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};

        Node() = default;
        Node(T d) : data(std::move(d)) {}
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    HazardPointerDomain hp_domain_;

public:
    MSQueue() {
        Node* dummy = new Node();
        head_.store(dummy);
        tail_.store(dummy);
    }

    ~MSQueue() {
        while (dequeue()) {}
        delete head_.load();
    }

    void enqueue(T value) {
        Node* new_node = new Node(std::move(value));
        HazardPointerDomain::HazardPointerHolder hp;

        while (true) {
            Node* tail = hp.protect(tail_, hp_domain_);
            Node* next = tail->next.load(std::memory_order_acquire);

            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // tail确实是最后一个节点，尝试添加新节点
                    if (tail->next.compare_exchange_weak(next, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        // 成功添加，尝试更新tail（失败也没关系）
                        tail_.compare_exchange_strong(tail, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        return;
                    }
                } else {
                    // tail落后了，帮助推进
                    tail_.compare_exchange_strong(tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
        }
    }

    std::optional<T> dequeue() {
        HazardPointerDomain::HazardPointerHolder hp1, hp2;

        while (true) {
            Node* head = hp1.protect(head_, hp_domain_);
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = hp2.protect(head->next, hp_domain_);

            if (head == head_.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (next == nullptr) {
                        return std::nullopt;  // 队列为空
                    }
                    // tail落后了，帮助推进
                    tail_.compare_exchange_strong(tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                } else {
                    // 读取数据并尝试移动head
                    T result = next->data;
                    if (head_.compare_exchange_weak(head, next,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                        hp_domain_.retire(head);
                        return result;
                    }
                }
            }
        }
    }
};
```

### 第三周：SPSC队列（单生产者单消费者）

**学习目标**：实现高性能的SPSC队列

```cpp
// spsc_queue.hpp
#pragma once
#include <atomic>
#include <array>
#include <optional>

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    alignas(64) std::atomic<size_t> head_{0};  // 消费者读取位置
    alignas(64) std::atomic<size_t> tail_{0};  // 生产者写入位置
    alignas(64) std::array<T, Capacity> buffer_;

    size_t mask() const { return Capacity - 1; }

public:
    bool try_push(const T& value) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (tail + 1) & mask();

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // 队列满
        }

        buffer_[tail] = value;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool try_push(T&& value) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (tail + 1) & mask();

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[tail] = std::move(value);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() {
        size_t head = head_.load(std::memory_order_relaxed);

        if (head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;  // 队列空
        }

        T result = std::move(buffer_[head]);
        head_.store((head + 1) & mask(), std::memory_order_release);
        return result;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return (tail - head) & mask();
    }
};
```

### 第四周：MPSC队列（多生产者单消费者）

**学习目标**：实现MPSC队列

```cpp
// mpsc_queue.hpp
#pragma once
#include <atomic>
#include <optional>

template <typename T>
class MPSCQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};

        Node() = default;
        Node(T d) : data(std::move(d)) {}
    };

    std::atomic<Node*> head_;  // 消费者访问
    alignas(64) std::atomic<Node*> tail_;  // 生产者访问

public:
    MPSCQueue() {
        Node* dummy = new Node();
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }

    ~MPSCQueue() {
        while (try_pop()) {}
        delete head_.load(std::memory_order_relaxed);
    }

    // 多个生产者可以并发调用
    void push(T value) {
        Node* new_node = new Node(std::move(value));
        Node* prev = tail_.exchange(new_node, std::memory_order_acq_rel);
        prev->next.store(new_node, std::memory_order_release);
    }

    // 只有单个消费者调用
    std::optional<T> try_pop() {
        Node* head = head_.load(std::memory_order_relaxed);
        Node* next = head->next.load(std::memory_order_acquire);

        if (next == nullptr) {
            return std::nullopt;
        }

        T result = std::move(next->data);
        head_.store(next, std::memory_order_release);
        delete head;
        return result;
    }

    bool empty() const {
        Node* head = head_.load(std::memory_order_acquire);
        return head->next.load(std::memory_order_acquire) == nullptr;
    }
};
```

---

## 实践项目

### 项目：完整的无锁队列库

#### Part 1: 带批量操作的MPMC队列
```cpp
// mpmc_queue.hpp
#pragma once
#include <atomic>
#include <array>
#include <optional>
#include <vector>

template <typename T, size_t Capacity>
class MPMCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };

    alignas(64) std::array<Cell, Capacity> buffer_;
    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};

    size_t mask() const { return Capacity - 1; }

public:
    MPMCQueue() {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool try_push(const T& value) {
        Cell* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

        while (true) {
            cell = &buffer_[pos & mask()];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;  // 队列满
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        cell->data = value;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() {
        Cell* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

        while (true) {
            cell = &buffer_[pos & mask()];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return std::nullopt;  // 队列空
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }

        T result = std::move(cell->data);
        cell->sequence.store(pos + Capacity, std::memory_order_release);
        return result;
    }

    // 批量入队
    size_t push_batch(const std::vector<T>& items) {
        size_t pushed = 0;
        for (const auto& item : items) {
            if (!try_push(item)) break;
            ++pushed;
        }
        return pushed;
    }

    // 批量出队
    std::vector<T> pop_batch(size_t max_count) {
        std::vector<T> result;
        result.reserve(max_count);
        for (size_t i = 0; i < max_count; ++i) {
            auto item = try_pop();
            if (!item) break;
            result.push_back(std::move(*item));
        }
        return result;
    }
};
```

#### Part 2: 性能基准测试
```cpp
// queue_benchmark.cpp
#include "spsc_queue.hpp"
#include "mpsc_queue.hpp"
#include "mpmc_queue.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>

template <typename Queue>
void bench_spsc(const char* name, int iterations) {
    Queue queue;
    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&] {
        for (int i = 0; i < iterations; ++i) {
            while (!queue.try_push(i)) {}
        }
    });

    std::thread consumer([&] {
        for (int i = 0; i < iterations; ++i) {
            while (!queue.try_pop()) {}
        }
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << name << ": " << duration.count() << " ms, "
              << (iterations * 1000.0 / duration.count()) / 1000000 << " M ops/sec\n";
}

template <typename Queue>
void bench_mpmc(const char* name, int producers, int consumers, int iterations) {
    Queue queue;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;

    for (int i = 0; i < producers; ++i) {
        threads.emplace_back([&] {
            while (true) {
                int val = produced.fetch_add(1, std::memory_order_relaxed);
                if (val >= iterations) break;
                while (!queue.try_push(val)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (int i = 0; i < consumers; ++i) {
        threads.emplace_back([&] {
            while (consumed.load(std::memory_order_relaxed) < iterations) {
                if (queue.try_pop()) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << name << " (" << producers << "P/" << consumers << "C): "
              << duration.count() << " ms, "
              << (iterations * 1000.0 / duration.count()) / 1000000 << " M ops/sec\n";
}

int main() {
    const int iterations = 10000000;

    std::cout << "=== SPSC Queue ===\n";
    bench_spsc<SPSCQueue<int, 1024>>("SPSCQueue", iterations);

    std::cout << "\n=== MPMC Queue ===\n";
    bench_mpmc<MPMCQueue<int, 1024>>("MPMCQueue", 1, 1, iterations);
    bench_mpmc<MPMCQueue<int, 1024>>("MPMCQueue", 2, 2, iterations);
    bench_mpmc<MPMCQueue<int, 1024>>("MPMCQueue", 4, 4, iterations);

    return 0;
}
```

---

## 检验标准

### 知识检验
- [ ] Michael-Scott队列的"帮助"机制是什么？
- [ ] SPSC队列为什么能比MPMC更简单高效？
- [ ] 有界队列和无界队列的权衡是什么？
- [ ] 如何选择合适的队列类型？

### 实践检验
- [ ] MS队列在多线程环境下正确工作
- [ ] SPSC队列达到高吞吐量
- [ ] MPMC队列正确处理并发

### 输出物
1. `michael_scott_queue.hpp`
2. `spsc_queue.hpp`
3. `mpsc_queue.hpp`
4. `mpmc_queue.hpp`
5. `queue_benchmark.cpp`
6. `notes/month17_lockfree_queue.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 30小时 | 21% |
| 源码阅读 | 25小时 | 18% |
| 队列实现 | 50小时 | 36% |
| 基准测试 | 20小时 | 14% |
| 笔记与文档 | 15小时 | 11% |

---

## 下月预告

Month 18将学习**Future/Promise与异步编程**，探索C++的异步编程模型，为后续的协程学习做准备。
