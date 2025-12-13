/**
 * Day 4 Ex.2: C++20 协程入门 - 手动控制的懒加载任务 (Lazy Task)
 * 
 * 【本例演示目标】
 * 1. 学习 `co_return` 语法：协程的返回值传递。
 * 2. 深入理解协程的生命周期控制 (Resume)。
 * 3. 演示最简单的 Lazy Evaluation（懒执行）模式。
 */

#include <coroutine>
#include <iostream>

// ==========================================
// 定义一个简单的 Task 类型
// ==========================================
// 这是一个“一次性”的异步任务，产生一个 int 结果
struct SimpleTask {
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        int result_data = 0;

        SimpleTask get_return_object() {
            return SimpleTask{Handle::from_promise(*this)};
        }

        // 懒执行：创建后不跑，等着被叫醒
        std::suspend_always initial_suspend() { return {}; }

        // 结束后不销毁，等待外部读取结果后手动销毁
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() { std::terminate(); }

        // ------------------------------------------------------------------
        // [特性接口] return_value
        // 对应 `co_return value;`
        // ------------------------------------------------------------------
        void return_value(int value) { result_data = value; }
    };

    Handle m_handle;

    explicit SimpleTask(Handle h) : m_handle(h) {}

    ~SimpleTask() {
        if (m_handle)
            m_handle.destroy();
    }

    // 禁止拷贝，允许移动
    SimpleTask(const SimpleTask&) = delete;
    SimpleTask& operator=(SimpleTask&) = delete;

    SimpleTask(SimpleTask&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    // 自定义接口：判断是否完成
    bool is_done() const { return m_handle && m_handle.done(); }

    // 自定义接口：手动恢复执行
    void start() {
        if (m_handle && !m_handle.done()) {
            m_handle.resume();
        }
    }

    // 自定义接口：获取结果
    int get_result() const { return m_handle.promise().result_data; }
};

// ==========================================
// 协程函数
// ==========================================
SimpleTask calculate_meaning_of_life() {
    std::cout << "  [Coro] Function entered. Doing detailed calculation..."
              << std::endl;

    // 模拟一些工作...
    int a = 10;
    int b = 32;

    std::cout << "  [Coro] Calculation finished. Returning via co_return."
              << std::endl;

    // 使用 co_return 返回结果
    // 这会调用 promise.return_value(42) 然后跳转到 final_suspend
    co_return a + b;
}

#include "day4_examples.h"

// ... (existing code)

void test_day4_lazy_task() {
    std::cout << "\n=== Running Day 4 Ex.2: Lazy Task ===\n";
    std::cout << "Test: Creating task..." << std::endl;

    // 1. 创建协程
    // 因为 initial_suspend 是 always，这里只会分配帧，不会执行函数体第一行
    auto task = calculate_meaning_of_life();

    std::cout << "Test: Task created. Has it started? "
              << (task.is_done() ? "Yes" : "No") << std::endl;

    // 2. 手动启动
    std::cout << "Test: Resuming task..." << std::endl;
    task.start();

    // 3. 检查结果
    if (task.is_done()) {
        std::cout << "Test: Task done. Result = " << task.get_result()
                  << std::endl;
    } else {
        std::cout << "Test: Task not done (unexpected)!" << std::endl;
    }
}
