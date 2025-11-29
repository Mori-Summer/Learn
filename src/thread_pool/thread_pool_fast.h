#pragma once

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

/**
 * @brief 高性能线程池 (Work Stealing 实现)
 * 
 * **核心优化点解析**:
 * 1.  **Work Stealing (任务窃取)**: 
 *     - **机制**: 每个线程都有自己的任务队列。线程优先处理自己队列的任务，当自己队列为空时，尝试从其他线程的队列“窃取”任务。
 *     - **优势**: 极大地减少了对单一全局锁的争用。在传统线程池中，所有线程争抢一个锁，导致严重的性能瓶颈。
 * 
 * 2.  **Cache Locality & False Sharing (缓存局部性与伪共享)**:
 *     - **机制**: 使用 `alignas(64)` 对 `WorkQueue` 进行内存对齐。
 *     - **优势**: 确保每个线程的队列独占 CPU 缓存行 (Cache Line)。防止因为一个线程修改了自己的队列，导致相邻内存地址（其他线程的队列）的缓存失效，从而引发“伪共享”导致的性能急剧下降。
 * 
 * 3.  **Fine-Grained Locking (细粒度锁)**:
 *     - **机制**: 每个队列一把锁，而不是整个池一把锁。
 *     - **优势**: 允许高并发操作，不同线程操作不同队列时完全无锁冲突。
 */
class ThreadPoolFast {
   public:
    // 构造函数：默认使用硬件支持的并发线程数
    explicit ThreadPoolFast(
        size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPoolFast();

    // 禁用拷贝和移动，确保线程池实例的唯一性和安全性
    ThreadPoolFast(const ThreadPoolFast&) = delete;
    ThreadPoolFast& operator=(const ThreadPoolFast&) = delete;

    /**
     * @brief 提交任务到线程池
     * 
     * @tparam F 任务函数类型
     * @tparam Args 参数类型
     * @param f 函数对象
     * @param args 函数参数
     * @return std::future<返回值类型> 用于获取异步结果
     * 
     * **C++20 特性**: 使用 `requires std::invocable` 概念 (Concept) 进行编译期约束，
     * 确保传入的函数和参数是可以被调用的，提供更友好的编译错误信息。
     */
    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>;

   private:
    // 工作线程的主循环函数
    void worker_thread(size_t index);

    // **关键数据结构**: 任务队列
    // alignas(64) 是为了适配常见的 L1 Cache Line 大小 (64字节)
    // 强制每个 WorkQueue 对象的起始地址是 64 的倍数，避免 False Sharing。
    struct alignas(64) WorkQueue {
        std::deque<std::function<void()>> tasks;    // 双端队列，支持从尾部窃取
        std::mutex mtx;                             // 专属于该队列的互斥锁
    };

    // 使用 unique_ptr 管理队列，确保队列对象的地址固定，不会因为 vector 扩容而移动
    std::vector<std::unique_ptr<WorkQueue>> queues_;
    std::vector<std::thread> threads_;

    // 原子停止标志，使用 memory_order 控制可见性
    std::atomic<bool> stop_{false};

    // 全局同步原语，仅用于处理线程休眠和唤醒（当所有队列都为空时）
    std::mutex global_mtx_;
    std::condition_variable global_cv_;
};

// 模板函数实现
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto ThreadPoolFast::submit(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result_t<F, Args...>> {
    using return_type = typename std::invoke_result_t<F, Args...>;

    // 包装任务，以便获取返回值
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();

    // **负载均衡策略**: 简单的轮询 (Round-Robin) 分发
    // 作用: 将新任务均匀地分配给各个线程的队列，避免热点。
    // 为什么用 atomic: 保证多线程同时提交任务时，index 计算是安全的。
    // memory_order_relaxed: 这里不需要严格的同步顺序，只要计数增加即可。
    static std::atomic<size_t> current_queue_index{0};
    size_t index = current_queue_index.fetch_add(1, std::memory_order_relaxed) %
                   queues_.size();

    {
        // **细粒度锁**: 只锁定目标队列的锁，而不是全局锁
        // 这样其他线程可以并发地向其他队列提交任务。
        std::lock_guard<std::mutex> lock(queues_[index]->mtx);
        queues_[index]->tasks.emplace_back([task]() { (*task)(); });
    }

    // 唤醒一个可能正在休眠的工作线程
    global_cv_.notify_one();

    return res;
}
