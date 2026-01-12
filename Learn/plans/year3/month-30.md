# Month 30: Reactor模式——事件驱动架构

## 本月主题概述

Reactor模式是高性能网络服务器的核心架构。本月深入学习Reactor模式的变体，包括单线程Reactor、多线程Reactor和主从Reactor，并实现完整的Reactor框架。

---

## 理论学习内容

### 第一周：Reactor模式概述

**学习目标**：理解Reactor模式的核心思想

**阅读材料**：
- [ ] Doug Schmidt的Reactor论文
- [ ] 《POSA Vol.2》Reactor章节

**核心概念**：

```
Reactor模式组件：

1. Handle（句柄）
   - 操作系统资源标识（fd）

2. Event Handler（事件处理器）
   - 处理特定事件的接口

3. Synchronous Event Demultiplexer（同步事件多路分离器）
   - epoll/select/poll

4. Reactor（反应器）
   - 管理事件注册和分发

┌────────────────────────────────────────────┐
│                 Reactor                     │
│  ┌─────────────────────────────────────┐   │
│  │    Event Demultiplexer (epoll)      │   │
│  └─────────────────────────────────────┘   │
│                    │                        │
│         dispatch events                     │
│                    ▼                        │
│  ┌─────────┬─────────┬─────────┐          │
│  │Handler1 │Handler2 │Handler3 │          │
│  └─────────┴─────────┴─────────┘          │
└────────────────────────────────────────────┘
```

### 第二周：单线程Reactor

```cpp
// single_reactor.hpp
#pragma once
#include <sys/epoll.h>
#include <functional>
#include <unordered_map>
#include <memory>

class EventHandler {
public:
    virtual ~EventHandler() = default;
    virtual int handle() const = 0;
    virtual void handle_read() = 0;
    virtual void handle_write() = 0;
    virtual void handle_close() = 0;
};

class Reactor {
public:
    Reactor() {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }

    ~Reactor() {
        if (epfd_ >= 0) close(epfd_);
    }

    void register_handler(std::shared_ptr<EventHandler> handler, uint32_t events) {
        int fd = handler->handle();
        handlers_[fd] = handler;

        epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    void modify_handler(int fd, uint32_t events) {
        epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
    }

    void remove_handler(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        handlers_.erase(fd);
    }

    void run() {
        running_ = true;
        std::vector<epoll_event> events(1024);

        while (running_) {
            int n = epoll_wait(epfd_, events.data(), events.size(), 1000);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                auto it = handlers_.find(fd);
                if (it == handlers_.end()) continue;

                auto& handler = it->second;
                uint32_t revents = events[i].events;

                if (revents & (EPOLLERR | EPOLLHUP)) {
                    handler->handle_close();
                    continue;
                }
                if (revents & EPOLLIN) {
                    handler->handle_read();
                }
                if (revents & EPOLLOUT) {
                    handler->handle_write();
                }
            }
        }
    }

    void stop() { running_ = false; }

private:
    int epfd_ = -1;
    bool running_ = false;
    std::unordered_map<int, std::shared_ptr<EventHandler>> handlers_;
};
```

### 第三周：多线程Reactor

```cpp
// multi_thread_reactor.hpp
#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

// 线程池
class ThreadPool {
public:
    ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this] {
                            return !running_ || !tasks_.empty();
                        });
                        if (!running_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
    }

    template<typename F>
    void submit(F&& f) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        cv_.notify_one();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = true;
};

// 单Reactor + 线程池
class ThreadPoolReactor {
public:
    ThreadPoolReactor(size_t num_threads)
        : pool_(num_threads) {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }

    void register_handler(int fd, uint32_t events,
                         std::function<void(int, uint32_t)> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[fd] = std::move(handler);

        epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    void run() {
        running_ = true;
        std::vector<epoll_event> events(1024);

        while (running_) {
            int n = epoll_wait(epfd_, events.data(), events.size(), 1000);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                uint32_t revents = events[i].events;

                std::function<void(int, uint32_t)> handler;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = handlers_.find(fd);
                    if (it == handlers_.end()) continue;
                    handler = it->second;
                }

                // 在线程池中执行处理
                pool_.submit([handler, fd, revents] {
                    handler(fd, revents);
                });
            }
        }
    }

private:
    int epfd_ = -1;
    bool running_ = false;
    ThreadPool pool_;
    std::unordered_map<int, std::function<void(int, uint32_t)>> handlers_;
    std::mutex mutex_;
};
```

### 第四周：主从Reactor

```cpp
// master_slave_reactor.hpp
#pragma once
#include <thread>
#include <vector>
#include <atomic>

class SubReactor {
public:
    SubReactor() {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }

    ~SubReactor() {
        stop();
        if (thread_.joinable()) thread_.join();
        if (epfd_ >= 0) close(epfd_);
    }

    void start() {
        thread_ = std::thread([this] { run(); });
    }

    void stop() {
        running_ = false;
    }

    void add_connection(int fd,
                       std::function<void(int, uint32_t)> handler) {
        // 使用eventfd通知SubReactor
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_.push_back({fd, std::move(handler)});
        // 可以用eventfd唤醒epoll_wait
    }

private:
    void run() {
        running_ = true;
        std::vector<epoll_event> events(256);

        while (running_) {
            // 处理待添加的连接
            {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                for (auto& [fd, handler] : pending_) {
                    handlers_[fd] = std::move(handler);
                    epoll_event ev;
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = fd;
                    epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
                }
                pending_.clear();
            }

            int n = epoll_wait(epfd_, events.data(), events.size(), 100);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                auto it = handlers_.find(fd);
                if (it != handlers_.end()) {
                    it->second(fd, events[i].events);
                }
            }
        }
    }

private:
    int epfd_ = -1;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::unordered_map<int, std::function<void(int, uint32_t)>> handlers_;
    std::vector<std::pair<int, std::function<void(int, uint32_t)>>> pending_;
    std::mutex pending_mutex_;
};

class MasterSlaveReactor {
public:
    MasterSlaveReactor(int port, size_t num_workers)
        : port_(port), workers_(num_workers) {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }

    bool start() {
        // 创建监听socket
        listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listen_fd_ < 0) return false;

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) return false;
        if (listen(listen_fd_, SOMAXCONN) < 0) return false;

        // 启动所有SubReactor
        for (auto& worker : workers_) {
            worker.start();
        }

        // 注册监听socket
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listen_fd_;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, listen_fd_, &ev);

        return true;
    }

    void run() {
        running_ = true;
        std::vector<epoll_event> events(64);

        while (running_) {
            int n = epoll_wait(epfd_, events.data(), events.size(), 1000);

            for (int i = 0; i < n; ++i) {
                if (events[i].data.fd == listen_fd_) {
                    handle_accept();
                }
            }
        }
    }

    void stop() {
        running_ = false;
        for (auto& worker : workers_) {
            worker.stop();
        }
    }

private:
    void handle_accept() {
        while (true) {
            int client = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK);
            if (client < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                continue;
            }

            // 轮询分配给SubReactor
            size_t idx = next_worker_++ % workers_.size();
            workers_[idx].add_connection(client, [](int fd, uint32_t events) {
                // 连接处理逻辑
                if (events & EPOLLIN) {
                    char buf[1024];
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n > 0) {
                        write(fd, buf, n);  // echo
                    } else if (n == 0) {
                        close(fd);
                    }
                }
            });
        }
    }

private:
    int port_;
    int epfd_ = -1;
    int listen_fd_ = -1;
    bool running_ = false;
    std::vector<SubReactor> workers_;
    std::atomic<size_t> next_worker_{0};
};
```

---

## 源码阅读任务

1. **muduo网络库**
   - EventLoop和Channel设计
   - 理解one loop per thread

2. **libevent源码**
   - event_base结构
   - 事件分发机制

---

## 实践项目

### 项目：完整的Reactor网络框架

```cpp
// reactor_framework.hpp - 完整框架请见实践代码

/*
框架特性：
1. 支持单线程、多线程、主从Reactor
2. 定时器支持
3. 优雅关闭
4. 连接管理
5. 缓冲区管理
*/
```

---

## 检验标准

- [ ] 理解Reactor模式的核心组件
- [ ] 实现单线程Reactor
- [ ] 理解多线程Reactor的线程安全问题
- [ ] 实现主从Reactor模式
- [ ] 理解各种Reactor模式的适用场景

### 输出物
1. `single_reactor.hpp` - 单线程Reactor
2. `multi_thread_reactor.hpp` - 多线程Reactor
3. `master_slave_reactor.hpp` - 主从Reactor
4. `echo_server.cpp` - 使用框架的服务器
5. `notes/month30_reactor.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| Reactor模式理论 | 20小时 |
| 单线程Reactor实现 | 30小时 |
| 多线程Reactor实现 | 35小时 |
| 主从Reactor实现 | 40小时 |
| 测试与优化 | 15小时 |

---

## 下月预告

Month 31将学习**Proactor模式**，理解异步完成通知架构。
