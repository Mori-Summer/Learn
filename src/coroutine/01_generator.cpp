/**
 * Day 4 Ex.1: C++20 协程入门 - 斐波那契数列生成器 (Generator)
 * 
 * 【协程是什么？】
 * 协程是一个可以“暂停”执行，稍后又能从暂停点“恢复”的函数。
 * 
 * 【本例演示目标】
 * 1. 学习 `co_yield` 语法：暂停协程并向外传出一个值。
 * 2. 理解 `promise_type` 的核心作用：它是协程的“大脑”。
 * 3. 实现一个支持 range-based for loop 的生成器。
 */

#include <coroutine>
#include <exception>
#include <iostream>

// ==========================================
// 1. 定义协程的返回类型 (Return Object)
// ==========================================
// 编译器通过查看函数的返回值类型（这里是 Generator），来决定如何创建协程。
struct Generator {
    // ------------------------------------------
    // 2. 定义 promise_type (必须是这个名字)
    // ------------------------------------------
    // 每一个协程返回类型内部必须定义由 `promise_type` 命名的嵌套类型。
    // 它是协程和外部代码沟通的桥梁。
    struct promise_type {
        // [数据槽位] 用于存放协程产生的值
        int current_value;

        // ------------------------------------------------------------------
        // [必要接口 1] get_return_object
        // 协程首次创建时调用，用于构造并返回协程对象（即外层的 Generator）。
        // ------------------------------------------------------------------
        Generator get_return_object() {
            // from_promise 可以通过 promise 对象的引用拿到对应的协程句柄
            return Generator{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // ------------------------------------------------------------------
        // [必要接口 2] initial_suspend
        // 协程启动时的行为。
        // 返回 std::suspend_always 表示协程创建后立即暂停（懒执行）。
        // 返回 std::suspend_never 表示创建后立即开始执行代码。
        // ------------------------------------------------------------------
        std::suspend_always initial_suspend() { return {}; }

        // ------------------------------------------------------------------
        // [必要接口 3] final_suspend
        // 协程执行结束时的行为。
        // 通常返回 suspend_always 以保留协程状态供外部检查（否则句柄会自动销毁）。
        // ------------------------------------------------------------------
        std::suspend_always final_suspend() noexcept { return {}; }

        // ------------------------------------------------------------------
        // [必要接口 4] unhandled_exception
        // 协程体内抛出未捕获异常时的处理。
        // ------------------------------------------------------------------
        void unhandled_exception() {
            std::terminate();    // 简单起见，直接终止
        }

        // ------------------------------------------------------------------
        // [特性接口] yield_value
        // 当代码中调用 `co_yield expr` 时，编译器会转化为：
        // await_suspend(promise.yield_value(expr));
        // 这里我们将值保存，并让协程暂停（suspend_always）。
        // ------------------------------------------------------------------
        std::suspend_always yield_value(int value) {
            current_value = value;
            return {};    // 暂停协程，控制权交回调用者
        }

        // ------------------------------------------------------------------
        // [特性接口] return_void
        // 当协程执行完成（隐式或显式 co_return）时调用。
        // 如果协程有返回值，则定义 return_value(T)。
        // ------------------------------------------------------------------
        void return_void() {}
    };

    // ==========================================
    // Generator 类的成员与接口
    // ==========================================

    // 协程句柄，类似于一个“遥控器”，可以恢复或销毁协程
    using Handle = std::coroutine_handle<promise_type>;
    Handle m_handle;

    // 构造函数：只能由 promise_type 创建
    explicit Generator(Handle h) : m_handle(h) {}

    // 析构函数：负责销毁协程，防止内存泄漏
    // 协程的状态是分配在堆上的，必须手动通过句柄销毁
    ~Generator() {
        if (m_handle)
            m_handle.destroy();
    }

    // 禁用拷贝（协程句柄通常是独占资源），允许移动（Move）
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (m_handle)
                m_handle.destroy();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    // ------------------------------------------
    // 迭代器接口 (为了支持 range-based for loop)
    // ------------------------------------------
    struct Iterator {
        Handle m_coro;

        // 解引用：从 promise 中拿到当前值
        int operator*() const { return m_coro.promise().current_value; }

        // 推进 (++it)：恢复协程执行，直到下一次暂停或结束
        Iterator& operator++() {
            m_coro.resume();
            return *this;
        }

        // 判断是否结束：检查协程是否已经完成
        bool operator!=(std::default_sentinel_t) const {
            return !m_coro.done();
        }
    };

    Iterator begin() {
        // 开始前，如果你在这个实现里是懒加载的，可能需要先 resume 一次让它执行到第一个 co_yield
        // 但在这个例子中，begin 的时候我们手动 resume 一次
        if (m_handle)
            m_handle.resume();
        return Iterator{m_handle};
    }

    std::default_sentinel_t end() { return {}; }
};

// ==========================================
// 3. 编写协程函数
// ==========================================
// 只要函数体包含 co_await, co_yield, co_return 之一，它就是协程。
Generator fibonacci_sequence(int max_count) {
    int a = 0, b = 1;

    // co_yield 用法：产出值并暂停
    // 第一次 yield 0
    co_yield a;

    if (max_count > 1) {
        co_yield b;
    }

    for (int i = 2; i < max_count; ++i) {
        int next = a + b;
        a = b;
        b = next;

        // 每次运行到这里，协程暂停，main 函数的循环继续
        co_yield next;
    }

    // 隐式 co_return; 协程结束
}

#include "day4_examples.h"

// ... (existing code)

void test_day4_generator() {
    std::cout << "\n=== Running Day 4 Ex.1: Generator ===\n";
    std::cout << "Starting Fibonacci Generator..." << std::endl;

    // 创建协程，但由于 initial_suspend 是 suspend_always，代码还没开始跑
    auto gen = fibonacci_sequence(10);

    // Range-based for loop 会自动调用 gen.begin(), it++, *it
    // 控制流会在 main 和 fibonacci_sequence 之间反复横跳
    for (int val : gen) {
        std::cout << "Generated: " << val << " ";
    }
    std::cout << "\nDone." << std::endl;
}
