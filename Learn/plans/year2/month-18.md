# Month 18: Future/Promise与异步编程——并发任务管理

## 本月主题概述

Future/Promise是管理异步操作的重要抽象。本月将深入学习std::future、std::promise、std::packaged_task、std::async，以及它们在实际项目中的应用模式。

---

## 理论学习内容

### 第一周：Future/Promise基础

**学习目标**：掌握标准库的异步原语

**阅读材料**：
- [ ] 《C++ Concurrency in Action》第4章
- [ ] CppCon演讲："Back to Basics: Futures"

**核心概念**：

```cpp
#include <future>
#include <thread>

// Promise: 设置值的一端
// Future: 获取值的一端

void producer(std::promise<int> prom) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    prom.set_value(42);  // 设置结果
}

void consumer(std::future<int> fut) {
    int value = fut.get();  // 阻塞等待结果
    std::cout << "Got: " << value << "\n";
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();

    std::thread t1(producer, std::move(prom));
    std::thread t2(consumer, std::move(fut));

    t1.join();
    t2.join();
}
```

### 第二周：std::async与任务启动

```cpp
// std::async: 简化异步任务启动
auto future = std::async(std::launch::async, []() {
    return expensive_computation();
});
auto result = future.get();

// 启动策略
std::async(std::launch::async, func);    // 必须新线程执行
std::async(std::launch::deferred, func); // 延迟到get()时执行（同一线程）
std::async(std::launch::async | std::launch::deferred, func); // 由实现决定（默认）

// packaged_task: 包装可调用对象
std::packaged_task<int(int)> task([](int x) { return x * 2; });
std::future<int> fut = task.get_future();
std::thread t(std::move(task), 21);
std::cout << fut.get();  // 42
```

### 第三周：shared_future与多消费者

```cpp
// std::future只能get一次
std::future<int> fut = std::async([]{ return 42; });
int v1 = fut.get();  // OK
// int v2 = fut.get();  // 错误！future已无效

// shared_future可以多次get
std::shared_future<int> sfut = std::async([]{ return 42; }).share();
int v1 = sfut.get();  // OK
int v2 = sfut.get();  // OK，返回相同的值

// 多个线程等待同一个结果
std::shared_future<Data> data_ready = prepare_data().share();

for (int i = 0; i < 4; ++i) {
    workers.emplace_back([data_ready] {
        const Data& data = data_ready.get();  // 所有线程获取相同数据
        process(data);
    });
}
```

### 第四周：异常传播与超时

```cpp
// 异常传播
std::promise<int> prom;
auto fut = prom.get_future();

std::thread([&prom] {
    try {
        throw std::runtime_error("Something went wrong");
    } catch (...) {
        prom.set_exception(std::current_exception());
    }
}).detach();

try {
    fut.get();  // 重新抛出异常
} catch (const std::exception& e) {
    std::cout << "Caught: " << e.what() << "\n";
}

// 超时等待
auto future = std::async([]{ /* ... */ });

if (future.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
    auto result = future.get();
} else if (future.wait_for(std::chrono::seconds(0)) == std::future_status::timeout) {
    std::cout << "Still running...\n";
} else if (future.wait_for(std::chrono::seconds(0)) == std::future_status::deferred) {
    std::cout << "Deferred\n";
}
```

---

## 实践项目

### 项目：实现增强的Future库

#### Part 1: mini_future实现
```cpp
// mini_future.hpp
#pragma once
#include <mutex>
#include <condition_variable>
#include <memory>
#include <exception>
#include <optional>
#include <functional>

template <typename T>
class mini_promise;

template <typename T>
class mini_future {
    friend class mini_promise<T>;

    struct SharedState {
        std::mutex mutex;
        std::condition_variable cv;
        std::optional<T> value;
        std::exception_ptr exception;
        bool ready = false;
    };

    std::shared_ptr<SharedState> state_;

    mini_future(std::shared_ptr<SharedState> state) : state_(state) {}

public:
    mini_future() = default;
    mini_future(mini_future&&) = default;
    mini_future& operator=(mini_future&&) = default;

    T get() {
        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->cv.wait(lock, [this] { return state_->ready; });

        if (state_->exception) {
            std::rethrow_exception(state_->exception);
        }
        return std::move(*state_->value);
    }

    void wait() const {
        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->cv.wait(lock, [this] { return state_->ready; });
    }

    template <typename Rep, typename Period>
    bool wait_for(std::chrono::duration<Rep, Period> timeout) const {
        std::unique_lock<std::mutex> lock(state_->mutex);
        return state_->cv.wait_for(lock, timeout, [this] { return state_->ready; });
    }

    bool valid() const { return state_ != nullptr; }

    bool ready() const {
        if (!state_) return false;
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->ready;
    }

    // 链式调用 (then)
    template <typename F>
    auto then(F&& func) -> mini_future<decltype(func(std::declval<T>()))> {
        using R = decltype(func(std::declval<T>()));
        mini_promise<R> promise;
        auto future = promise.get_future();

        std::thread([state = state_, func = std::forward<F>(func),
                    promise = std::move(promise)]() mutable {
            try {
                std::unique_lock<std::mutex> lock(state->mutex);
                state->cv.wait(lock, [&state] { return state->ready; });

                if (state->exception) {
                    promise.set_exception(state->exception);
                } else {
                    if constexpr (std::is_void_v<R>) {
                        func(std::move(*state->value));
                        promise.set_value();
                    } else {
                        promise.set_value(func(std::move(*state->value)));
                    }
                }
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        }).detach();

        return future;
    }
};

template <typename T>
class mini_promise {
    std::shared_ptr<typename mini_future<T>::SharedState> state_;

public:
    mini_promise()
        : state_(std::make_shared<typename mini_future<T>::SharedState>()) {}

    mini_promise(mini_promise&&) = default;
    mini_promise& operator=(mini_promise&&) = default;

    mini_future<T> get_future() {
        return mini_future<T>(state_);
    }

    void set_value(T value) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->ready) {
            throw std::runtime_error("Promise already satisfied");
        }
        state_->value = std::move(value);
        state_->ready = true;
        state_->cv.notify_all();
    }

    void set_exception(std::exception_ptr e) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->ready) {
            throw std::runtime_error("Promise already satisfied");
        }
        state_->exception = e;
        state_->ready = true;
        state_->cv.notify_all();
    }
};
```

#### Part 2: 并行算法工具
```cpp
// parallel.hpp
#pragma once
#include <future>
#include <vector>
#include <algorithm>
#include <numeric>

namespace parallel {

// 并行map
template <typename InputIt, typename OutputIt, typename F>
void transform(InputIt first, InputIt last, OutputIt out, F func,
               size_t num_threads = std::thread::hardware_concurrency()) {
    size_t size = std::distance(first, last);
    if (size == 0) return;

    size_t chunk_size = (size + num_threads - 1) / num_threads;
    std::vector<std::future<void>> futures;

    for (size_t i = 0; i < num_threads && i * chunk_size < size; ++i) {
        size_t start = i * chunk_size;
        size_t end = std::min(start + chunk_size, size);

        futures.push_back(std::async(std::launch::async,
            [=, &func] {
                std::transform(first + start, first + end, out + start, func);
            }));
    }

    for (auto& f : futures) {
        f.get();
    }
}

// 并行reduce
template <typename InputIt, typename T, typename BinaryOp>
T reduce(InputIt first, InputIt last, T init, BinaryOp op,
         size_t num_threads = std::thread::hardware_concurrency()) {
    size_t size = std::distance(first, last);
    if (size == 0) return init;

    size_t chunk_size = (size + num_threads - 1) / num_threads;
    std::vector<std::future<T>> futures;

    for (size_t i = 0; i < num_threads && i * chunk_size < size; ++i) {
        size_t start = i * chunk_size;
        size_t end = std::min(start + chunk_size, size);

        futures.push_back(std::async(std::launch::async,
            [=, &op] {
                return std::accumulate(first + start, first + end, T{}, op);
            }));
    }

    T result = init;
    for (auto& f : futures) {
        result = op(result, f.get());
    }
    return result;
}

// 并行for_each
template <typename InputIt, typename F>
void for_each(InputIt first, InputIt last, F func,
              size_t num_threads = std::thread::hardware_concurrency()) {
    size_t size = std::distance(first, last);
    if (size == 0) return;

    size_t chunk_size = (size + num_threads - 1) / num_threads;
    std::vector<std::future<void>> futures;

    for (size_t i = 0; i < num_threads && i * chunk_size < size; ++i) {
        size_t start = i * chunk_size;
        size_t end = std::min(start + chunk_size, size);

        futures.push_back(std::async(std::launch::async,
            [=, &func] {
                std::for_each(first + start, first + end, func);
            }));
    }

    for (auto& f : futures) {
        f.get();
    }
}

// when_all: 等待所有future完成
template <typename... Futures>
auto when_all(Futures&&... futures) {
    return std::make_tuple(std::forward<Futures>(futures).get()...);
}

// when_any: 等待任一future完成
template <typename T>
std::pair<size_t, T> when_any(std::vector<std::future<T>>& futures) {
    while (true) {
        for (size_t i = 0; i < futures.size(); ++i) {
            if (futures[i].wait_for(std::chrono::milliseconds(1))
                    == std::future_status::ready) {
                return {i, futures[i].get()};
            }
        }
    }
}

} // namespace parallel
```

---

## 检验标准

### 知识检验
- [ ] future和promise的关系是什么？
- [ ] std::async的启动策略有哪些？
- [ ] shared_future的用途是什么？
- [ ] 如何在future中传播异常？

### 实践检验
- [ ] mini_future正确实现了阻塞等待
- [ ] then方法实现了链式调用
- [ ] 并行算法能正确利用多核

### 输出物
1. `mini_future.hpp`
2. `parallel.hpp`
3. `test_futures.cpp`
4. `notes/month18_futures.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 30小时 | 21% |
| 源码阅读 | 25小时 | 18% |
| mini_future实现 | 35小时 | 25% |
| 并行算法实现 | 35小时 | 25% |
| 测试与文档 | 15小时 | 11% |

---

## 下月预告

Month 19将学习**线程池设计与实现**，探索工作窃取、任务优先级等高级特性。
