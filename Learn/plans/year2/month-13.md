# Month 13: 并发编程基础——线程与同步原语

## 本月主题概述

欢迎进入第二年的学习！并发编程是现代C++最具挑战性的领域之一。本月将从基础开始，理解线程的创建与管理、基本的同步原语，为后续的内存模型和无锁编程打下基础。

---

## 理论学习内容

### 第一周：并发与并行的基础概念

**学习目标**：建立并发编程的基本认知模型

**阅读材料**：
- [ ] 《C++ Concurrency in Action》第1-2章
- [ ] CppCon演讲："Back to Basics: Concurrency"
- [ ] 博客：Preshing on Programming - "A Introduction to Lock-Free Programming"

**核心概念**：

#### 并发 vs 并行
```cpp
// 并发（Concurrency）：多个任务在时间上重叠执行
// - 单核CPU通过时间片轮转实现
// - 关注的是程序结构

// 并行（Parallelism）：多个任务真正同时执行
// - 需要多核CPU
// - 关注的是执行方式

// 并发是并行的超集
// 你可以写并发程序而不并行执行（单核）
// 但并行执行必然是并发的
```

#### 为什么并发编程困难？
```cpp
// 1. 竞态条件（Race Condition）
int counter = 0;
void increment() {
    for (int i = 0; i < 1000000; ++i) {
        ++counter;  // 非原子操作！
        // 实际是: tmp = counter; tmp = tmp + 1; counter = tmp;
    }
}
// 两个线程同时执行，最终counter可能小于2000000

// 2. 数据竞争（Data Race）
// 多个线程同时访问同一内存位置，至少一个是写操作，没有同步
// 这是未定义行为！

// 3. 死锁（Deadlock）
std::mutex m1, m2;
// Thread 1: lock(m1), lock(m2)
// Thread 2: lock(m2), lock(m1)
// 如果交错执行，可能永久阻塞

// 4. 活锁（Livelock）
// 线程不断改变状态以响应对方，但都无法前进

// 5. 饥饿（Starvation）
// 某个线程永远无法获得所需资源
```

### 第二周：std::thread详解

**学习目标**：掌握线程的创建、管理和生命周期

```cpp
#include <thread>
#include <iostream>

// 1. 基本创建
void task() {
    std::cout << "Running in thread " << std::this_thread::get_id() << "\n";
}

int main() {
    std::thread t(task);
    t.join();  // 等待线程结束
}

// 2. 传递参数
void task_with_args(int x, const std::string& s) {
    std::cout << x << " " << s << "\n";
}

std::thread t1(task_with_args, 42, "hello");

// 注意：参数默认被拷贝！
std::string str = "world";
std::thread t2(task_with_args, 1, str);  // str被拷贝

// 传引用需要std::ref
void modify(int& x) { x = 100; }
int value = 0;
std::thread t3(modify, std::ref(value));  // 必须用std::ref

// 3. 移动语义
std::thread t4(task);
std::thread t5 = std::move(t4);  // t4不再拥有线程
// t4.join();  // 错误！t4是空的

// 4. detach vs join
std::thread t6(task);
t6.detach();  // 线程与thread对象分离，后台运行
// t6.join();  // 错误！已经detach

// 5. 检查可join性
if (t6.joinable()) {
    t6.join();
}

// 6. RAII包装器（避免忘记join）
class ThreadGuard {
    std::thread& t_;
public:
    explicit ThreadGuard(std::thread& t) : t_(t) {}
    ~ThreadGuard() {
        if (t_.joinable()) {
            t_.join();
        }
    }
    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;
};
```

#### 线程本地存储
```cpp
// thread_local: 每个线程有独立的变量副本
thread_local int tls_counter = 0;

void increment_tls() {
    ++tls_counter;  // 每个线程修改自己的副本
    std::cout << "Thread " << std::this_thread::get_id()
              << " counter = " << tls_counter << "\n";
}

int main() {
    std::thread t1([]{ for(int i=0; i<5; ++i) increment_tls(); });
    std::thread t2([]{ for(int i=0; i<5; ++i) increment_tls(); });
    t1.join();
    t2.join();
    // 每个线程的counter独立增长到5
}
```

### 第三周：互斥锁（Mutex）

**学习目标**：掌握互斥锁的正确使用

```cpp
#include <mutex>

// 1. 基本mutex
std::mutex mtx;
int shared_data = 0;

void safe_increment() {
    mtx.lock();
    ++shared_data;
    mtx.unlock();
}

// 问题：如果中间抛异常，unlock不会执行 -> 死锁！

// 2. RAII锁管理器
void better_increment() {
    std::lock_guard<std::mutex> lock(mtx);  // 构造时lock
    ++shared_data;
    // 析构时自动unlock，即使异常也能正确释放
}

// C++17: 类模板参数推导
void even_better() {
    std::lock_guard lock(mtx);  // 自动推导类型
    ++shared_data;
}

// 3. unique_lock: 更灵活
void flexible_locking() {
    std::unique_lock<std::mutex> lock(mtx);

    // 可以手动unlock/lock
    ++shared_data;
    lock.unlock();
    // ... 做一些不需要锁的工作
    lock.lock();
    ++shared_data;

    // 可以移动
    std::unique_lock<std::mutex> lock2 = std::move(lock);

    // 可以延迟锁定
    std::unique_lock<std::mutex> lock3(mtx, std::defer_lock);
    // ... 稍后
    lock3.lock();
}

// 4. 避免死锁：同时锁定多个mutex
std::mutex m1, m2;

void deadlock_prone() {
    // 线程1: lock(m1), lock(m2)
    // 线程2: lock(m2), lock(m1)
    // 可能死锁！
}

void deadlock_free() {
    // 方法1: std::lock同时锁定
    std::lock(m1, m2);
    std::lock_guard<std::mutex> lg1(m1, std::adopt_lock);
    std::lock_guard<std::mutex> lg2(m2, std::adopt_lock);

    // 方法2: C++17 scoped_lock
    std::scoped_lock lock(m1, m2);  // 同时锁定，内部避免死锁
}

// 5. 递归锁
std::recursive_mutex rmtx;

void recursive_func(int depth) {
    std::lock_guard<std::recursive_mutex> lock(rmtx);
    if (depth > 0) {
        recursive_func(depth - 1);  // 同一线程可以多次锁定
    }
}
// 注意：递归锁通常是设计问题的标志，尽量避免

// 6. 定时锁
std::timed_mutex tmtx;

void try_lock_example() {
    // 尝试锁定，立即返回
    if (tmtx.try_lock()) {
        // 获得锁
        tmtx.unlock();
    }

    // 尝试锁定，最多等待100ms
    if (tmtx.try_lock_for(std::chrono::milliseconds(100))) {
        // 获得锁
        tmtx.unlock();
    }

    // 尝试锁定，直到某个时间点
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(1);
    if (tmtx.try_lock_until(deadline)) {
        // 获得锁
        tmtx.unlock();
    }
}
```

### 第四周：条件变量

**学习目标**：掌握线程间的等待/通知机制

```cpp
#include <condition_variable>
#include <queue>

std::mutex mtx;
std::condition_variable cv;
std::queue<int> data_queue;
bool finished = false;

// 生产者
void producer() {
    for (int i = 0; i < 10; ++i) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            data_queue.push(i);
        }
        cv.notify_one();  // 通知一个等待的线程
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        finished = true;
    }
    cv.notify_all();  // 通知所有等待的线程
}

// 消费者
void consumer() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);

        // 等待条件：队列非空或已完成
        cv.wait(lock, []{ return !data_queue.empty() || finished; });

        // 处理所有可用数据
        while (!data_queue.empty()) {
            int data = data_queue.front();
            data_queue.pop();
            lock.unlock();

            // 处理数据（不持有锁）
            std::cout << "Consumed: " << data << "\n";

            lock.lock();
        }

        if (finished && data_queue.empty()) {
            break;
        }
    }
}

// wait的正确使用
// cv.wait(lock, predicate) 等价于：
// while (!predicate()) {
//     cv.wait(lock);
// }
// 这处理了虚假唤醒（spurious wakeup）
```

#### 条件变量的常见陷阱
```cpp
// 陷阱1: 忘记在修改共享状态后通知
void bad_producer() {
    std::lock_guard<std::mutex> lock(mtx);
    data_queue.push(42);
    // 忘记 cv.notify_one()!
    // 消费者可能永远等待
}

// 陷阱2: 在锁外通知（不是错误，但可能影响性能）
void questionable_producer() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        data_queue.push(42);
    }
    cv.notify_one();  // 可以工作，但消费者可能立即被唤醒又阻塞
}

// 陷阱3: 没有使用谓词，可能遭遇虚假唤醒
void vulnerable_consumer() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock);  // 危险！可能虚假唤醒
    // data_queue.front();  // 可能队列为空
}

// 陷阱4: 使用notify_one时消费者数量不匹配
// 如果有多个消费者，notify_one可能唤醒"错误"的那个
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `std::thread`构造函数实现
- [ ] `std::mutex`的平台实现（pthread_mutex / CRITICAL_SECTION）
- [ ] `std::lock_guard`和`std::unique_lock`
- [ ] `std::condition_variable::wait`实现

---

## 实践项目

### 项目：线程安全的数据结构

#### Part 1: 线程安全的队列
```cpp
// thread_safe_queue.hpp
#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>
#include <chrono>

template <typename T>
class ThreadSafeQueue {
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool closed_ = false;

public:
    ThreadSafeQueue() = default;

    // 禁止拷贝
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    // 入队
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_) {
                throw std::runtime_error("Queue is closed");
            }
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    // 尝试入队（队列关闭时返回false）
    bool try_push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_) {
                return false;
            }
            queue_.push(std::move(value));
        }
        cv_.notify_one();
        return true;
    }

    // 阻塞出队
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !queue_.empty() || closed_; });

        if (queue_.empty()) {
            return std::nullopt;  // 队列关闭且为空
        }

        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    // 非阻塞出队
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    // 带超时的出队
    template <typename Rep, typename Period>
    std::optional<T> pop_for(std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!cv_.wait_for(lock, timeout,
                          [this] { return !queue_.empty() || closed_; })) {
            return std::nullopt;  // 超时
        }

        if (queue_.empty()) {
            return std::nullopt;  // 关闭
        }

        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    // 关闭队列（不再接受新元素）
    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    // 查询
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_;
    }
};
```

#### Part 2: 线程安全的栈
```cpp
// thread_safe_stack.hpp
#pragma once
#include <mutex>
#include <stack>
#include <optional>
#include <memory>
#include <stdexcept>

template <typename T>
class ThreadSafeStack {
    mutable std::mutex mtx_;
    std::stack<T> stack_;

public:
    ThreadSafeStack() = default;

    ThreadSafeStack(const ThreadSafeStack& other) {
        std::lock_guard<std::mutex> lock(other.mtx_);
        stack_ = other.stack_;
    }

    ThreadSafeStack& operator=(const ThreadSafeStack&) = delete;

    void push(T value) {
        std::lock_guard<std::mutex> lock(mtx_);
        stack_.push(std::move(value));
    }

    // 方案1: 返回shared_ptr（避免pop时拷贝构造抛异常的问题）
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stack_.empty()) {
            throw std::runtime_error("Stack is empty");
        }
        auto result = std::make_shared<T>(std::move(stack_.top()));
        stack_.pop();
        return result;
    }

    // 方案2: 通过引用参数返回
    bool pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stack_.empty()) {
            return false;
        }
        value = std::move(stack_.top());
        stack_.pop();
        return true;
    }

    // 方案3: 返回optional
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stack_.empty()) {
            return std::nullopt;
        }
        T value = std::move(stack_.top());
        stack_.pop();
        return value;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return stack_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return stack_.size();
    }
};
```

#### Part 3: 简单的线程池
```cpp
// thread_pool.hpp
#pragma once
#include "thread_safe_queue.hpp"
#include <thread>
#include <vector>
#include <functional>
#include <future>
#include <atomic>

class ThreadPool {
    std::vector<std::thread> workers_;
    ThreadSafeQueue<std::function<void()>> tasks_;
    std::atomic<bool> stop_{false};

    void worker_thread() {
        while (true) {
            auto task = tasks_.pop();
            if (!task) {
                break;  // 队列关闭
            }
            (*task)();
        }
    }

public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_thread(); });
        }
    }

    ~ThreadPool() {
        tasks_.close();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 提交任务
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {

        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();

        tasks_.push([task]() { (*task)(); });

        return result;
    }

    // 等待所有任务完成
    void wait_all() {
        tasks_.close();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    size_t size() const {
        return workers_.size();
    }
};
```

#### Part 4: 并发计数器对比
```cpp
// counter_benchmark.cpp
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iostream>

// 方案1: 无保护（错误！）
int unsafe_counter = 0;

void unsafe_increment(int n) {
    for (int i = 0; i < n; ++i) {
        ++unsafe_counter;
    }
}

// 方案2: 互斥锁
std::mutex counter_mutex;
int mutex_counter = 0;

void mutex_increment(int n) {
    for (int i = 0; i < n; ++i) {
        std::lock_guard<std::mutex> lock(counter_mutex);
        ++mutex_counter;
    }
}

// 方案3: 原子变量
std::atomic<int> atomic_counter{0};

void atomic_increment(int n) {
    for (int i = 0; i < n; ++i) {
        ++atomic_counter;
    }
}

// 方案4: 本地累加后合并
std::atomic<int> local_atomic_counter{0};

void local_increment(int n) {
    int local = 0;
    for (int i = 0; i < n; ++i) {
        ++local;
    }
    local_atomic_counter += local;
}

template <typename Func>
void benchmark(const char* name, Func f, int threads, int iterations) {
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

    std::cout << name << ": " << duration.count() << " ms\n";
}

int main() {
    const int threads = 4;
    const int iterations = 1000000;

    benchmark("Unsafe", unsafe_increment, threads, iterations);
    std::cout << "  Result: " << unsafe_counter
              << " (expected: " << threads * iterations << ")\n";

    benchmark("Mutex", mutex_increment, threads, iterations);
    std::cout << "  Result: " << mutex_counter << "\n";

    benchmark("Atomic", atomic_increment, threads, iterations);
    std::cout << "  Result: " << atomic_counter.load() << "\n";

    benchmark("Local", local_increment, threads, iterations);
    std::cout << "  Result: " << local_atomic_counter.load() << "\n";

    return 0;
}
```

---

## 检验标准

### 知识检验
- [ ] 并发与并行的区别是什么？
- [ ] 什么是数据竞争？为什么是未定义行为？
- [ ] std::thread的join和detach有什么区别？
- [ ] 为什么不应该直接使用mutex.lock()/unlock()？
- [ ] 条件变量为什么需要配合谓词使用？

### 实践检验
- [ ] ThreadSafeQueue支持多生产者多消费者
- [ ] ThreadSafeStack的三种pop方式都能正确工作
- [ ] ThreadPool能正确执行任务并返回结果
- [ ] 理解不同计数器实现的性能差异

### 输出物
1. `thread_safe_queue.hpp`
2. `thread_safe_stack.hpp`
3. `thread_pool.hpp`
4. `counter_benchmark.cpp`
5. `notes/month13_concurrency_basics.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 35小时 | 25% |
| 源码阅读 | 25小时 | 18% |
| ThreadSafeQueue实现 | 25小时 | 18% |
| ThreadPool实现 | 30小时 | 21% |
| 基准测试与文档 | 25小时 | 18% |

---

## 下月预告

Month 14将深入**C++内存模型**，这是理解并发编程的核心。我们将学习顺序一致性、acquire-release语义、relaxed原子操作，以及它们在不同硬件上的映射。
