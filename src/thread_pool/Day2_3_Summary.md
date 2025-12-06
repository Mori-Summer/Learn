# Day 2-3 总结：进阶特性与工程化 (Advanced Features & Engineering)

## 1. 核心成就
在高性能线程池的基础上，我们成功引入了**业务级特性（优先级调度）**，实施了**工程化最佳实践（单元测试）**，并迈出了**协程化（Coroutines）**的关键一步。

### 关键特性
*   **Priority Scheduling (优先级调度)**: 摒弃了传统的全局堆 (`std::priority_queue`) 方案，采用**多级队列 (Multi-Level Queues)** 架构，在支持高优先级抢占执行的同时，保持了任务窃取的高效性。
*   **Unit Testing Infrastructure (单元测试基建)**: 针对环境限制，实现了一个**轻量级无依赖的测试框架 (`fast_test.h`)**，替代了不可靠的 `main` 函数肉眼验证，支持 `TEST`、`EXPECT_EQ` 等断言。
*   **Coroutine Integration (协程集成)**: 成功打通 C++20 协程与线程池的连接，实现了 `co_await ScheduleOn(pool)`，让协程可以在线程池中挂起和恢复。

---

## 2. 技术要点与设计思想

### A. 优先级架构：为何选择多级队列？
*   **方案对比**:
    *   **堆 (`std::priority_queue`)**: 插入删除复杂度 O(log N)，且必须加锁保护整个堆，并发性能差。
    *   **多级队列 (Multi-Level Deques)**: 我们为每个线程维护 3 个独立队列 (High, Normal, Low)。
*   **优势**:
    *   **无锁/细粒度锁**: 依然可以使用 `std::deque` 的头部轻量锁操作，避免重排堆的巨大开销。
    *   **窃取友好**: 窃取逻辑自然升级——先尝试偷 High，再偷 Normal。这比从堆中“偷”元素要容易得多。

### B. 工程化：从 Demo 到 Production
*   **测试框架 (`fast_test.h`)**:
    *   **设计**: Header-only，利用宏 (`#define`) 和静态注册自动收集测试用例。
    *   **价值**: 可以在不引入庞大依赖（如 GTest）的前提下，确保核心功能的正确性与回归安全。

### C. 协程预热：未来的并发模型
*   **原理**: 线程池通过 `ScheduleOn` Awaitable 对象介入协程的 `await_suspend` 阶段。
*   **流程**: 协程暂停 -> 将 handle 封装为任务 -> 提交至线程池 -> 线程池 Worker 执行任务 -> `handle.resume()` 恢复协程。
*   这标志着我们从“传统的任务调度”向“异步协作式调度”的转变。

---

## 3. 验证结果 (Verification)
通过集成测试 (`main.cpp` + `fast_test.h`) 验证了以下场景：
*   **[PASS] Priority Ordering**: 高优先级任务 (High) 确实优先于低优先级任务 (Low) 执行。
*   **[PASS] Concurrency Stress**: 100 线程并发提交大量任务，计数器结果精确无误，证明了并发安全性。
*   **[PASS] Coroutine Resume**: 协程能够正确地在线程池工作线程中恢复执行，上下文切换正常。

## 4. 下一步计划
现在的线程池已经具备了工业级的雏形。接下来的挑战是构建**基于协程的任务调度器**：
1.  **Work Stealing 协程运行时**: 让协程像任务一样被窃取，实现真正的 M:N 调度。
2.  **IO 驱动**: 结合 `epoll`/`IOCP`，实现全异步的 IO 操作。
