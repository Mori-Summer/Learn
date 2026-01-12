# Month 19: 线程池设计与实现——高效任务调度

## 本月主题概述

线程池是管理并发任务的核心组件。本月将学习线程池的各种设计模式，包括工作窃取、任务优先级、动态扩缩容等高级特性。

---

## 理论学习内容

### 第一周：线程池基础设计

**核心组件**：
- 工作线程池
- 任务队列
- 任务提交接口
- 生命周期管理

### 第二周：工作窃取（Work Stealing）

```cpp
// 每个线程有自己的任务队列
// 空闲时从其他线程的队列"窃取"任务
// 优点：更好的负载均衡，减少竞争
```

### 第三周：任务优先级与依赖

```cpp
// 优先级队列
// 任务依赖图（DAG）
// 延迟执行
```

### 第四周：动态扩缩容

```cpp
// 根据负载动态调整线程数
// 最小/最大线程数限制
// 空闲超时回收
```

---

## 实践项目

### 项目：生产级线程池

```cpp
// advanced_thread_pool.hpp
#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <future>
#include <atomic>
#include <condition_variable>
#include <deque>

class ThreadPool {
    struct WorkerData {
        std::deque<std::function<void()>> local_queue;
        std::mutex mutex;
        std::condition_variable cv;
    };

    std::vector<std::thread> workers_;
    std::vector<std::unique_ptr<WorkerData>> worker_data_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> next_worker_{0};

    static thread_local size_t worker_index_;

    void worker_thread(size_t index) {
        worker_index_ = index;
        auto& data = *worker_data_[index];

        while (!stop_) {
            std::function<void()> task;

            // 尝试从本地队列获取
            {
                std::unique_lock<std::mutex> lock(data.mutex);
                if (!data.local_queue.empty()) {
                    task = std::move(data.local_queue.front());
                    data.local_queue.pop_front();
                }
            }

            // 尝试从其他队列窃取
            if (!task) {
                task = steal_task(index);
            }

            // 等待新任务
            if (!task) {
                std::unique_lock<std::mutex> lock(data.mutex);
                data.cv.wait_for(lock, std::chrono::milliseconds(100),
                    [&] { return !data.local_queue.empty() || stop_; });
                continue;
            }

            task();
        }
    }

    std::function<void()> steal_task(size_t thief_index) {
        for (size_t i = 0; i < worker_data_.size(); ++i) {
            size_t victim = (thief_index + i + 1) % worker_data_.size();
            auto& data = *worker_data_[victim];

            std::unique_lock<std::mutex> lock(data.mutex, std::try_to_lock);
            if (lock && !data.local_queue.empty()) {
                auto task = std::move(data.local_queue.back());
                data.local_queue.pop_back();
                return task;
            }
        }
        return nullptr;
    }

public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < threads; ++i) {
            worker_data_.push_back(std::make_unique<WorkerData>());
        }

        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back(&ThreadPool::worker_thread, this, i);
        }
    }

    ~ThreadPool() {
        stop_ = true;
        for (auto& data : worker_data_) {
            data->cv.notify_all();
        }
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {

        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<R> result = task->get_future();

        // 优先提交到当前线程的队列
        size_t target = (worker_index_ < workers_.size())
            ? worker_index_
            : next_worker_.fetch_add(1) % workers_.size();

        {
            std::lock_guard<std::mutex> lock(worker_data_[target]->mutex);
            worker_data_[target]->local_queue.emplace_back([task] { (*task)(); });
        }
        worker_data_[target]->cv.notify_one();

        return result;
    }
};

thread_local size_t ThreadPool::worker_index_ = SIZE_MAX;
```

---

## 检验标准

- [ ] 理解工作窃取的优势
- [ ] 实现支持工作窃取的线程池
- [ ] 基准测试验证性能

### 输出物
1. `advanced_thread_pool.hpp`
2. `test_thread_pool.cpp`
3. `notes/month19_thread_pool.md`

---

## 下月预告

Month 20将学习**Actor模型与消息传递**，探索另一种并发编程范式。
