#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <vector>

class ThreadPool {
   public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();

    template <typename F, typename... Args>
    auto AddTask(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

   private:
    // 工作线程需要运行的函数
    void WorkerThread();

   private:
    // 线程池中的工作线程
    std::vector<std::thread> m_vecWorkers;

    // 任务队列
    std::queue<std::function<void()>> m_qTasks;

    // 同步原语
    std::mutex m_mutex;
    std::condition_variable m_condition;

    // 停止标志
    bool m_bStop;
};

// 模板函数 AddTask：向线程池提交一个新的任务
// F: 任务函数的类型
// Args: 任务函数参数的类型
template <typename F, typename... Args>
auto ThreadPool::AddTask(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    // 推导任务函数的返回值类型
    using return_type = typename std::invoke_result<F, Args...>::type;

    // 创建一个 packaged_task，它可以包装函数和参数，并允许我们获取返回值（通过 future）
    // std::bind 将函数 f 和参数 args 绑定在一起，形成一个可调用的对象
    // std::forward 完美转发，保持参数的左值/右值属性
    // 使用 make_shared 是因为 std::function 要求对象是可复制的，
    // 而 packaged_task 是不可复制的（只能移动），所以用 shared_ptr 包装一下
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    // 获取与 packaged_task 关联的 future，用于在将来获取任务执行的结果
    std::future<return_type> res = task->get_future();
    {
        // 加锁，保护任务队列
        std::unique_lock lock(m_mutex);

        // 如果线程池已经停止，则不允许提交新任务
        if (m_bStop) {
            throw std::runtime_error("AddTask is stop");
        }

        // 将任务添加到队列中
        // 这里 lambda 捕获了 task 智能指针（复制了一份指针），
        // 当 lambda 被执行时，会调用 (*task)()，即执行 packaged_task
        m_qTasks.emplace([task]() { (*task)(); });
    }
    // 唤醒一个正在等待的工作线程来处理这个新任务
    m_condition.notify_one();

    // 返回 future 给调用者
    return res;
}
