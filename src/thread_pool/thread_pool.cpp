#include "thread_pool.h"
#include <mutex>

// 构造函数：初始化线程池
// num_threads: 指定线程池中线程的数量
ThreadPool::ThreadPool(size_t num_threads) : m_bStop(false) {
    // 循环创建指定数量的工作线程
    for (size_t i = 0; i < num_threads; ++i) {
        // emplace_back 直接在 vector 尾部构造线程对象，避免拷贝
        // 使用 lambda 表达式作为线程的执行函数
        // [this] 捕获当前对象的指针，以便在 lambda 中调用成员函数 WorkerThread
        m_vecWorkers.emplace_back([this]() { this->WorkerThread(); });
    }
}

// 析构函数：销毁线程池，回收资源
ThreadPool::~ThreadPool() {
    {
        // 创建一个作用域，使用 unique_lock 加锁
        // 这里加锁是为了保护 m_bStop 变量的修改
        std::unique_lock<std::mutex> lock(m_mutex);

        // 将停止标志设置为 true，告诉所有线程该下班了
        m_bStop = true;
    }

    // 通知所有正在等待条件变量的线程（唤醒它们）
    // 这样它们就能检查到 m_bStop 变为了 true
    m_condition.notify_all();

    // 等待所有线程执行完毕
    for (std::thread& worker : m_vecWorkers) {
        // 如果线程是可以 join 的（即还在运行且关联了线程对象）
        if (worker.joinable()) {
            // join() 会阻塞当前线程，直到 worker 线程执行结束
            // 确保线程池销毁前，所有内部线程都已安全退出
            worker.join();
        }
    }
}

// 工作线程函数：这是每个线程实际运行的代码
void ThreadPool::WorkerThread() {
    // 线程会一直循环，直到被要求停止
    while (true) {
        std::function<void()> task;

        {
            // 获取互斥锁，保护任务队列 m_qTasks
            // unique_lock 相比 lock_guard 更灵活，配合 condition_variable 使用
            std::unique_lock<std::mutex> lock(m_mutex);

            // 等待条件满足：
            // 1. 收到停止信号 (m_bStop == true)
            // 2. 或者 任务队列不为空 (!m_qTasks.empty())
            // wait 会自动释放锁并阻塞线程，直到被 notify 唤醒且 lambda 返回 true
            m_condition.wait(lock, [this] {
                return this->m_bStop || !this->m_qTasks.empty();
            });

            // 如果收到了停止信号，并且任务队列已经空了
            // 说明所有任务都做完了，且不再接收新任务，线程可以结束了
            if (this->m_bStop && this->m_qTasks.empty()) {
                return;
            }

            // 取出队列头部的任务
            // std::move 将任务的所有权转移给局部变量 task
            task = std::move(this->m_qTasks.front());
            // 将任务从队列中移除
            this->m_qTasks.pop();
        }
        // 锁在这里（作用域结束）会自动释放
        // 这一点非常重要：执行任务的时候不需要持有锁！
        // 这样其他线程就可以并发地操作任务队列（比如添加任务）

        // 执行任务
        task();
    }
}
