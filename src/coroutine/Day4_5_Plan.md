# Day 4-5 计划：深入 C++20 协程 (C++20 Coroutines Deep Dive)

## 0. 为什么这一步至关重要？
在高性能网络服务器、游戏引擎和 AI 推理管道中，**异步编程**是无法绕过的坎。
*   **回调地狱 (Callback Hell)**: 传统的异步回调代码支离破碎，难以维护。
*   **同步阻塞**: 浪费昂贵的线程资源等待 I/O。
*   **C++20 协程**: 允许你**用同步的代码风格写出异步逻辑**，不仅可读性极佳，而且完全零开销抽象 (Zero Overhead Abstraction)。这是 C++20 最具变革性的特性。

---

## 1. 深度教学：协程原理与设计思想

### A. 什么是协程？ (What)
协程是可以**暂停 (Suspend)** 和 **恢复 (Resume)** 的函数。
*   **普通函数**: 调用 -> 执行 -> 返回 (栈销毁)。
*   **协程**: 调用 -> 执行 -> 暂停 (保存状态) -> 返回控制权 -> ... -> 恢复 (恢复状态) -> 继续执行。

> [!NOTE]
> C++20 的协程是 **无栈协程 (Stackless Coroutines)**。这意味着协程状态（局部变量、断点）存储在堆上的“协程帧 (Coroutine Frame)”中，而不是占用几 MB 的线程栈。这使得你可以轻松创建百万计的协程。

### B. 核心组件 (The Big Three)
要理解 C++ 协程，必须理解三个核心概念的交互：

1.  **`promise_type` (承诺对象)**
    *   **角色**: 协程的大管家。控制协程的返回值、初始行为、异常处理和最终结果。
    *   **比喻**: 导演。决定戏怎么开场 (`initial_suspend`)，怎么谢幕 (`final_suspend`)，以及给观众呈现什么 (`get_return_object`)。

2.  **`std::coroutine_handle` (协程句柄)**
    *   **角色**: 协程的遥控器。用于在外部恢复 (`resume`) 或销毁 (`destroy`) 协程。
    *   **比喻**: 录像机的播放/暂停键。

3.  **`Awaitable` (等待体)**
    *   **角色**: 决定“是否暂停”以及“暂停后甚至去哪里”。这是协程与外部系统（如线程池、IO）交互的桥梁。
    *   **比喻**: 传送门。协程走到这里，问：“由于条件没满足（IO 没回来），我能进传送门去休息一会吗？”
    *   **核心方法**:
        *   `await_ready()`: “真的需要暂停吗？” (返回 false 表示如果不暂停就直接继续)。
        *   `await_suspend(handle)`: “暂停前要做什么？” (比如把自己注册到 epoll 或线程池)。
        *   `await_resume()`: “醒来后拿到什么结果？” (比如读取 socket 数据)。

### C. 解决了什么问题？ (Why)
*   **消灭回调**: 用 `co_await SocketRead()` 替代 `socket.read(..., [](auto data){ ... })`。
*   **高性能并发**: 避免了 OS 线程上下文切换的昂贵开销（微秒级 -> 纳秒级）。
*   **结构化并发**: 更容易管理异步任务的生命周期。

### D. 典型应用场景 (Where)
*   **高并发网络服务器**: 处理海量连接 (C10k/C100k 问题)。
*   **生成器 (Generators)**: 按需生成序列数据 (如无限斐波那契数列)，节省内存。
*   **异步流水线**: 游戏逻辑脚本、AI 模型推理流程 (Pre-process -> Inference -> Post-process)。

---

## 2. 实战任务：构建最小协程系统

### Day 4: 协程基础与生成器
**目标**: 不依赖线程池，纯手写一个最简单的生成器，理解协程的生命周期。
1.  **[x] 编写 `Generator<T>`**:
    *   **实现代码**: [01_generator.cpp](file:///d:/C++/Learn/src/coroutine/01_generator.cpp) (Generator), [02_lazy_task.cpp](file:///d:/C++/Learn/src/coroutine/02_lazy_task.cpp) (Lazy Task & co_return)
    *   实现 `promise_type`：`yield_value` (用于传出数据), `suspend_always`。
    *   实现迭代器接口：使得可以通过 `for (int i : range(1, 10)) ...` 遍历协程。
    *   **验证**: 打印斐波那契数列 (已验证)。
2.  **[x] 掌握 `Awaitable` (等待体)**:
    *   **实现代码**: [03_awaiter.cpp](file:///d:/C++/Learn/src/coroutine/03_awaiter.cpp)
    *   详解 `await_ready` (优化暂停), `await_suspend` (挂起逻辑), `await_resume` (返回值)。


### Day 5: 异步任务与线程池的完美融合
**目标**: 将 Day 3 的“协程预热”进化为生产级实现，让协程真正“跑”在我们的 `ThreadPoolFast` 上。
1.  **[ ] 完善 `AsyncTask` 类型**:
    *   支持返回值 (`Task<int>`)。
    *   支持异常传播 (`unhandled_exception` + `std::exception_ptr`).
    *   实现资源自动回收 (RAII handle management)。
2.  **[ ] 实现调度器 `Scheduler` Awaitable**:
    *   重写 `await_suspend`: 将协程句柄 `.address()` 封装成 task 放入线程池队列。
    *   **关键点**: 确保协程在线程池 Worker 中恢复时，线程安全地访问数据。
3.  **[ ] 综合测试**:
    *   模拟一个异步业务流：`fetch_data` (IO模拟) -> `process_data` (CPU计算) -> `save_data` (IO模拟)。
    *   这三个步骤在不同的线程中“跳跃”执行，但代码写在一个函数里！

---

## 3. 设计模式：如何思考协程设计
在设计协程库时，遵循以下心法：
1.  **Return Type Driven (返回类型驱动)**: 编译器通过协程函数的返回类型（如 `Task<T>`）来查找 `promise_type`。虽然函数体里都是 `co_await`，但行为由返回值类型定义。
2.  **Lazy Execution (懒执行)**: 绝大多数异步协程应该在 `initial_suspend` 返回 `std::suspend_always`。创建协程不等于开始执行，显式 `.resume()` 或 `co_await` 它才开始。
3.  **Fire-and-Forget vs Awaitable**:
    *   如果你需要结果，返回 `Task<T>`。
    *   如果你是完全独立的后台任务（如日志），返回 `DetachedTask`。

---

## 4. 推荐阅读与资源
*   *C++20 Coroutines: The most complete guide* (Andreas Fertig)
*   CppCon talk: "Nanocoroutines" / "C++20 Coroutines: History, Design, and Use"
