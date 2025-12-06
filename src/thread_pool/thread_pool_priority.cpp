#include "thread_pool_priority.h"

#include <random>

namespace parallel {

ThreadPoolPriority::ThreadPoolPriority(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        queues_.push_back(std::make_unique<WorkQueue>());
    }

    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this, i] { worker_thread(i); });
    }
}

ThreadPoolPriority::~ThreadPoolPriority() {
    stop_.store(true, std::memory_order_release);
    global_cv_.notify_all();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadPoolPriority::worker_thread(size_t index) {
    // 线程局部随机数生成器，避免锁竞争
    std::random_device rd;
    std::mt19937 rng(rd());
    // 用于生成 [0, queues_.size() - 1] 的随机索引
    std::uniform_int_distribution<size_t> dist(0, queues_.size() - 1);

    while (!stop_.load(std::memory_order_acquire)) {
        std::function<void()> task;
        bool found_task = false;

        // 1. Check Local Queue (High -> Normal -> Low)
        {
            std::lock_guard<std::mutex> lock(queues_[index]->mtx);
            for (int p = 0; p < static_cast<int>(Priority::Count); ++p) {
                if (!queues_[index]->queues[p].empty()) {
                    task = std::move(queues_[index]->queues[p].front());
                    queues_[index]->queues[p].pop_front();
                    found_task = true;
                    break;    // 找到最高优先级的任务，立即跳出
                }
            }
        }

        // 2. Work Stealing (Random + Priority)
        if (!found_task) {
            size_t num_queues = queues_.size();
            size_t start_index = dist(rng);    // 随机起始点

            for (size_t i = 0; i < num_queues; ++i) {
                size_t target_idx = (start_index + i) % num_queues;
                if (target_idx == index)
                    continue;

                if (queues_[target_idx]->mtx.try_lock()) {
                    std::lock_guard<std::mutex> lock(queues_[target_idx]->mtx,
                                                     std::adopt_lock);

                    // 窃取优先级：优先偷 High，然后 Normal，然后 Low
                    for (int p = 0; p < static_cast<int>(Priority::Count);
                         ++p) {
                        if (!queues_[target_idx]->queues[p].empty()) {
                            // 优化：从尾部窃取 (Steal from back) 以减少与 Owner (pop_front) 的竞争
                            // 实现了 deque 的两端访问：Owner 取头，Thief 取尾。
                            task = std::move(
                                queues_[target_idx]->queues[p].back());
                            queues_[target_idx]->queues[p].pop_back();
                            found_task = true;
                            // std::cout << "Thread " << index << " stole Priority " << p << " from " << target_idx << "\n";
                            break;
                        }
                    }
                }

                if (found_task)
                    break;
            }
        }

        // 3. Execute or Sleep
        if (found_task) {
            task();
        } else {
            // -----------------------------------------------------------
            // 阶段 3: 休眠等待 (Sleep)
            // -----------------------------------------------------------
            // 如果本地队列为空，且窃取也失败了，说明当前系统负载较轻。
            // 为了避免忙等待 (Busy Waiting) 烧满 CPU，线程需要进入休眠状态。

            // 1. 获取全局锁 (Unique Lock)
            // std::condition_variable::wait 需要一个 std::unique_lock。
            // 这里使用 global_mtx_ 是为了与 submit() 中的 notify 配合。
            std::unique_lock<std::mutex> lock(global_mtx_);

            // 2. Double-Check Stop 标志
            // 在持有锁的情况下再次检查 stop_，防止"Lost Wakeup"问题：
            // 如果在进入 sleep 之前主线程刚好发出了 stop 信号并 notify_all，
            // 若不检查，当前线程可能会错误地进入睡眠而永远无法醒来（直到超时）。
            // memory_order_acquire: 确保看到主线程 release 之前的内存写入。
            if (stop_.load(std::memory_order_acquire))
                break;

            // 3. 有超时时间的等待 (Wait with Timeout)
            // wait_for 会释放 lock 并挂起线程，直到：
            // a. 被 notify (有新任务提交)
            // b. 超时 (10ms)
            //
            // 为什么要设置超时?
            // - 作为"保底"机制：万一 notify 丢失，或者有任务但某些竞态导致没被发现，
            //   线程过一会能自己醒来再试一次窃取。
            // - 避免长时间深度睡眠导致响应变慢。
            global_cv_.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
}

}    // namespace parallel
