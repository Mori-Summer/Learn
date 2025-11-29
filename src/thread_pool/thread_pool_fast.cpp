#include "thread_pool_fast.h"

// 构造函数
ThreadPoolFast::ThreadPoolFast(size_t num_threads) {
    // 1. 初始化所有队列
    // 为每个线程创建一个独立的 WorkQueue
    for (size_t i = 0; i < num_threads; ++i) {
        queues_.push_back(std::make_unique<WorkQueue>());
    }

    // 2. 启动工作线程
    // 将线程索引 (index) 传递给线程，让它知道哪个队列属于自己
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this, i] { worker_thread(i); });
    }
}

// 析构函数
ThreadPoolFast::~ThreadPoolFast() {
    // 1. 发送停止信号
    // memory_order_release 保证在此之前的所有内存写入对其他线程可见
    stop_.store(true, std::memory_order_release);

    // 2. 唤醒所有可能在休眠的线程，让它们检查 stop 标志并退出
    global_cv_.notify_all();

    // 3. 等待所有线程结束
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

// 工作线程函数：这是每个线程实际运行的代码
void ThreadPoolFast::worker_thread(size_t index) {
    // 只要没有收到停止信号，就一直循环
    // memory_order_acquire 保证能读取到最新的 stop_ 值
    while (!stop_.load(std::memory_order_acquire)) {
        std::function<void()> task;
        bool found_task = false;

        // =================================================================
        // 阶段 1: 尝试从自己的本地队列获取任务
        // =================================================================
        // 优势:
        // 1. 数据局部性最好 (L1 Cache 命中率高)。
        // 2. 锁竞争最小 (通常只有自己访问，除非被窃取)。
        {
            std::lock_guard<std::mutex> lock(queues_[index]->mtx);
            if (!queues_[index]->tasks.empty()) {
                task = std::move(queues_[index]->tasks.front());
                queues_[index]->tasks.pop_front();
                found_task = true;
            }
        }

        // =================================================================
        // 阶段 2: 任务窃取 (Work Stealing)
        // =================================================================
        // 如果本地队列为空，说明当前线程空闲。
        // 为了负载均衡，尝试从其他忙碌线程的队列中“偷”一个任务来做。
        if (!found_task) {
            for (size_t i = 0; i < queues_.size(); ++i) {
                if (i == index)
                    continue;    // 跳过自己

                // 尝试锁定目标队列
                // **关键点**: 使用 try_lock() 而不是 lock()
                // 为什么要这样做?
                // 1. 避免阻塞: 如果目标队列正在被使用（忙），我们不想在这里死等，不如去试下一个。
                // 2. 防止死锁: 简单的遍历加锁容易导致死锁，try_lock 是安全的。
                if (queues_[i]->mtx.try_lock()) {
                    // 成功获取锁，使用 adopt_lock 告诉 lock_guard 锁已经被锁住了
                    std::lock_guard<std::mutex> lock(queues_[i]->mtx,
                                                     std::adopt_lock);

                    if (!queues_[i]->tasks.empty()) {
                        // 偷取任务！
                        // 通常 Work Stealing 会从队列尾部 (back)
                        // 偷，以减少与 owner (从 front 取) 的冲突。
                        // 这里为了简化代码使用了 deque 的 front，但原理一致。
                        task = std::move(queues_[i]->tasks.front());
                        queues_[i]->tasks.pop_front();
                        found_task = true;
                        // std::cout << "Thread " << index << " stole task from "
                        // << i << "\n";
                    }
                }

                // 如果偷到了，就停止遍历，赶紧去干活
                if (found_task)
                    break;
            }
        }

        // =================================================================
        // 阶段 3: 执行任务 或 休眠等待
        // =================================================================
        if (found_task) {
            // 执行任务
            // 注意: 执行任务时不需要持有任何锁，允许其他线程并发操作队列
            task();
        } else {
            // 确实没有任务可做，进入休眠以节省 CPU 资源
            std::unique_lock<std::mutex> lock(global_mtx_);

            // 再次检查停止标志，防止在休眠前刚好收到停止信号
            if (stop_.load(std::memory_order_acquire))
                break;

            // 等待唤醒或超时
            // 设置超时 (如 10ms) 是为了防止某些极端情况下的信号丢失，
            // 或者定期醒来检查是否有新的窃取机会（虽然主要靠 notify）。
            global_cv_.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
}
