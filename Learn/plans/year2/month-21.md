# Month 21: C++20协程基础——无栈协程的革命

## 本月主题概述

C++20引入的协程是一种可以暂停和恢复执行的函数。与传统线程相比，协程更轻量，非常适合异步I/O和生成器模式。

---

## 理论学习内容

### 第一周：协程基本概念

**阅读材料**：
- [ ] CppCon演讲："C++20 Coroutines"
- [ ] Lewis Baker的协程系列博客

**核心概念**：

```cpp
#include <coroutine>

// 协程是包含 co_await, co_yield, co_return 的函数
// 编译器会将其转换为状态机

// 三个关键字：
// co_await expr;   // 暂停，等待expr完成
// co_yield expr;   // 产出值并暂停
// co_return expr;  // 返回值并结束

// 协程必须返回特定类型，该类型定义了协程的行为
```

### 第二周：Promise和Awaitable

```cpp
// 每个协程类型需要定义promise_type
struct Generator {
    struct promise_type {
        int current_value;

        Generator get_return_object() {
            return Generator{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(int value) {
            current_value = value;
            return {};
        }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    ~Generator() { if (handle) handle.destroy(); }

    bool next() {
        handle.resume();
        return !handle.done();
    }

    int value() { return handle.promise().current_value; }
};

// 使用
Generator count_to(int n) {
    for (int i = 1; i <= n; ++i) {
        co_yield i;
    }
}

int main() {
    auto gen = count_to(5);
    while (gen.next()) {
        std::cout << gen.value() << "\n";
    }
}
```

### 第三周：自定义Awaitable

```cpp
// Awaitable需要实现三个方法：
// await_ready() -> bool
// await_suspend(coroutine_handle<>) -> void/bool/coroutine_handle<>
// await_resume() -> T

struct SleepAwaiter {
    std::chrono::milliseconds duration;

    bool await_ready() const { return duration.count() <= 0; }

    void await_suspend(std::coroutine_handle<> handle) {
        std::thread([=] {
            std::this_thread::sleep_for(duration);
            handle.resume();
        }).detach();
    }

    void await_resume() {}
};

auto sleep_for(std::chrono::milliseconds ms) {
    return SleepAwaiter{ms};
}

// 使用
Task async_function() {
    std::cout << "Start\n";
    co_await sleep_for(std::chrono::seconds(1));
    std::cout << "After 1 second\n";
}
```

### 第四周：异步Task实现

```cpp
// task.hpp
#pragma once
#include <coroutine>
#include <exception>
#include <optional>

template <typename T>
class Task {
public:
    struct promise_type {
        std::optional<T> result;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }

        auto final_suspend() noexcept {
            struct Awaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept {
                    if (h.promise().continuation)
                        return h.promise().continuation;
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return Awaiter{};
        }

        void return_value(T value) { result = std::move(value); }
        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(Task&& other) : handle(other.handle) { other.handle = nullptr; }
    ~Task() { if (handle) handle.destroy(); }

    // 使Task可被co_await
    bool await_ready() { return handle.done(); }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) {
        handle.promise().continuation = cont;
        return handle;
    }

    T await_resume() {
        if (handle.promise().exception)
            std::rethrow_exception(handle.promise().exception);
        return std::move(*handle.promise().result);
    }

    T get() {
        handle.resume();
        return await_resume();
    }
};
```

---

## 实践项目

### 项目：协程工具库

包含Generator、Task、以及简单的事件循环。

---

## 检验标准

- [ ] 理解协程的编译器转换
- [ ] 能实现自定义的Generator
- [ ] 能实现基本的async Task
- [ ] 理解Awaitable接口

### 输出物
1. `generator.hpp`
2. `task.hpp`
3. `test_coroutines.cpp`
4. `notes/month21_coroutines.md`

---

## 下月预告

Month 22将学习**协程应用：异步I/O**，将协程与io_uring/IOCP结合。
