/**
 * Day 4 Ex.3: 深入理解 Awaiter (等待体)
 * 
 * 【Awaiter 是什么？】
 * 如果说 `promise_type` 是协程的“大脑”（控制整体行为），那么 `Awaiter` 就是协程的“手脚”。
 * 当我们在代码中写 `co_await expr` 时，`expr` 必须返回一个 Awaiter 对象。
 * 这个 Awaiter 对象决定了：
 * 1. 【是否暂停】：`await_ready()`
 * 2. 【暂停后做什么】：`await_suspend()` —— 这是实现异步 I/O、线程切换的关键 hooks。
 * 3. 【恢复后拿什么】：`await_resume()` —— 决定了 `co_await` 表达式的返回值。
 * 
 * 【本例演示目标】
 * 实现一个 `MagicAwaiter`，它会模拟一个“异步操作”（比如把任务交给另一个线程，或者只是简单的打印日志），
 * 并在代码注释中详细剖析三个核心接口的调用时机。
 */

#include <coroutine>
#include <iostream>
#include <thread>
#include "day4_examples.h"

// -------------------------------------------------------
// 1. 基础设施：一个最简单的 Task (为了能跑起协程)
// -------------------------------------------------------
struct MiniTask {
    struct promise_type {
        MiniTask get_return_object() { return {}; }

        std::suspend_never initial_suspend() { return {}; }

        std::suspend_never final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() { std::terminate(); }
    };
};

// -------------------------------------------------------
// 2. 自定义 Awaiter 类
// -------------------------------------------------------
struct MagicAwaiter {
    std::string name;
    bool enforce_suspend;    // 是否强制暂停的开关

    // ------------------------------------------------------------------
    // [接口 1] await_ready
    // 问：现在是否需要暂停？
    // 答：
    //   - 返回 true:  "我不累，不需要暂停，直接继续跑吧"。 -> 跳过 await_suspend，直接调 await_resume。
    //   - 返回 false: "我需要暂停（比如数据还没准备好）"。   -> 真正挂起协程，调用 await_suspend。
    // ------------------------------------------------------------------
    bool await_ready() {
        std::cout << "  [Awaiter:" << name << "] await_ready() called."
                  << std::endl;
        // 如果 enforce_suspend 为 false，我们假装数据已经好了，不暂停。
        return !enforce_suspend;
    }

    // ------------------------------------------------------------------
    // [接口 2] await_suspend
    // 问：既然已经暂停了，现在该把控制权交给谁？
    // 参数：h 是当前协程的句柄。你可以把它存起来，之后在别的地方 resume()。
    // 答：
    //   - 返回 void:      "挂起当前协程，控制权立即交还给调用者 (caller)/恢复者 (resumer)"。
    //   - 返回 bool:      true 同 void；false 表示 "算了，不挂起了，直接恢复运行"。
    //   - 返回 handle:    "挂起当前协程，并立即激活（resume）返回的那个协程 handle"。(用于对称协程切换)
    // ------------------------------------------------------------------
    void await_suspend(std::coroutine_handle<> h) {
        std::cout << "  [Awaiter:" << name
                  << "] await_suspend() called. Coroutine is now SUSPENDED."
                  << std::endl;

        // 典型用法：在这里发起异步操作，比如把 h 扔到线程池队列，或者注册到 epoll。
        // 为了演示简单，我们仅仅打印一行字，然后...
        // 假如我们在这里什么都不做并返回，协程就永远停在这里了（死锁/泄露），
        // 除非有别的线程持有 handle 并 resume 它可以。

        // *本例演示*：我们在当前线程模拟“直接处理完毕随后恢复”。
        // 注意：在实际异步编程中，通常是将 handle 存入回调，由回调 resume。
        std::cout << "  [Awaiter:" << name << "] Simulating work..."
                  << std::endl;

        // 手动恢复 (Resuming)
        // 注意：在 await_suspend 内部直接 resume 可能会导致栈无限递归风险 (Stack Overflow)，
        // 生产级代码通常使用 `std::noop_coroutine()` 或返回 handle 搞定，这里仅作逻辑演示。
        h.resume();
    }

    // ------------------------------------------------------------------
    // [接口 3] await_resume
    // 问：协程恢复运行了（或者 await_ready 说不用暂停），该返回什么值给 `co_await` 表达式？
    // ------------------------------------------------------------------
    int await_resume() {
        std::cout << "  [Awaiter:" << name << "] await_resume() called."
                  << std::endl;
        return 42;    // 返回一个神奇数字
    }
};

// -------------------------------------------------------
// 3. 使用 Awaiter 的协程
// -------------------------------------------------------
MiniTask coroutine_using_awaiter() {
    std::cout << "[Coro] Start." << std::endl;

    // 情况 A：await_ready 返回 true (不暂停)
    std::cout << "[Coro] Co-awaiting 'NoSuspend'..." << std::endl;
    int result1 = co_await MagicAwaiter{"NoSuspend", false};
    std::cout << "[Coro] Result1: " << result1 << "\n" << std::endl;

    // 情况 B：await_ready 返回 false (暂停)
    std::cout << "[Coro] Co-awaiting 'DoSuspend'..." << std::endl;
    int result2 = co_await MagicAwaiter{"DoSuspend", true};
    std::cout << "[Coro] Result2: " << result2 << "\n" << std::endl;

    std::cout << "[Coro] End." << std::endl;
}

// -------------------------------------------------------
// 4. 测试入口
// -------------------------------------------------------
void test_day4_awaiter() {
    std::cout << "\n=== Running Day 4 Ex.3: Detailed Awaiter ===\n";
    coroutine_using_awaiter();
    std::cout << "=== Done ===\n";
}
