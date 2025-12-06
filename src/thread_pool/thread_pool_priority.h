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

namespace parallel {

/**
 * @brief 任务优先级定义
 */
enum class Priority {
    High = 0,
    Normal = 1,
    Low = 2,
    Count = 3    // 辅助计数
};

/**
 * @brief 支持优先级的任务调度器 (Based on ThreadPoolFast)
 * 
 * **Day 2 特性**:
 * 1.  **Priority Scheduling (优先级调度)**:
 *     - 支持 High, Normal, Low 三种优先级。
 *     - 内部使用多级队列 (Multi-Level Queues) 管理任务。
 *     - 工作线程总是优先处理高优先级任务。
 * 
 * 2.  **Optimized Work Stealing (优化的窃取策略)**:
 *     - **优先级窃取**: 窃取时也优先窃取受害者的高优先级任务。
 *     - **随机窃取 (Random Stealing)**: 随机选择受害者，减少多线程同时尝试窃取同一目标的锁竞争。
 */
class ThreadPoolPriority {
   public:
    explicit ThreadPoolPriority(
        size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPoolPriority();

    ThreadPoolPriority(const ThreadPoolPriority&) = delete;
    ThreadPoolPriority& operator=(const ThreadPoolPriority&) = delete;

    /**
     * @brief 提交带优先级的任务
     * 
     * @param prio 任务优先级
     * @param f 函数对象
     * @param args 函数参数
     * @return std::future<返回值类型>
     */
    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    auto submit(Priority prio, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>;

    // 为了兼容性，提供默认优先级的重载（默认 Normal）
    template <typename F, typename... Args>
        requires std::invocable<F, Args...>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>> {
        return submit(Priority::Normal, std::forward<F>(f),
                      std::forward<Args>(args)...);
    }

   private:
    void worker_thread(size_t index);

    // 对齐到 Cache Line (64 bytes) 避免 False Sharing
    struct alignas(64) WorkQueue {
        // 多级队列：idx 0=High, 1=Normal, 2=Low
        // 使用定长数组管理不同优先级的 deque
        std::deque<std::function<void()>>
            queues[static_cast<int>(Priority::Count)];

        std::mutex mtx;    // 保护该线程的所有优级队列
    };

    std::vector<std::unique_ptr<WorkQueue>> queues_;
    std::vector<std::thread> threads_;
    std::atomic<bool> stop_{false};
    std::mutex global_mtx_;
    std::condition_variable global_cv_;
};

// 模板实现
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto ThreadPoolPriority::submit(Priority prio, F&& f, Args&&... args)
    -> std::future<typename std::invoke_result_t<F, Args...>> {
    using return_type = typename std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();

    // 简单的 Round-Robin 分发策略，也可以优化为“分发到当前负载最小”或“当前线程”
    static std::atomic<size_t> current_queue_index{0};
    size_t index = current_queue_index.fetch_add(1, std::memory_order_relaxed) %
                   queues_.size();

    {
        std::lock_guard<std::mutex> lock(queues_[index]->mtx);
        // 根据优先级放入对应的队列
        int p_idx = static_cast<int>(prio);
        if (p_idx < 0 || p_idx >= static_cast<int>(Priority::Count)) {
            p_idx = static_cast<int>(Priority::Normal);
        }
        queues_[index]->queues[p_idx].emplace_back([task]() { (*task)(); });
    }

    global_cv_.notify_one();
    return res;
}

}    // namespace parallel
