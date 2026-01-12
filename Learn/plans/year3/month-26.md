# Month 26: 阻塞与非阻塞I/O——I/O模型演进

## 本月主题概述

理解I/O模型是高性能网络编程的基础。本月深入学习阻塞与非阻塞I/O的区别，掌握select和poll的使用，为后续的epoll和io_uring打下基础。

---

## 理论学习内容

### 第一周：I/O模型分类

**学习目标**：理解五种I/O模型

**阅读材料**：
- [ ] 《UNIX网络编程》卷1 第6章
- [ ] Stevens的I/O模型经典论述

**核心概念**：

```
五种I/O模型：
1. 阻塞I/O (Blocking I/O)
2. 非阻塞I/O (Non-blocking I/O)
3. I/O多路复用 (I/O Multiplexing)
4. 信号驱动I/O (Signal-driven I/O)
5. 异步I/O (Asynchronous I/O)

阻塞I/O的问题：
┌─────────┐     read()      ┌─────────┐
│  应用   │ ──────────────> │  内核   │
│  进程   │ <阻塞等待数据>  │         │
│         │ <──────────────│  等待   │
└─────────┘    返回数据     └─────────┘

非阻塞I/O：
┌─────────┐    read()       ┌─────────┐
│  应用   │ ──────────────> │  内核   │
│  进程   │ <── EAGAIN ──── │  无数据 │
│  轮询   │ ──────────────> │         │
│         │ <── 数据 ────── │  有数据 │
└─────────┘                 └─────────┘
```

### 第二周：非阻塞I/O编程

```cpp
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>

// 设置非阻塞
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 非阻塞读取
ssize_t nonblocking_read(int fd, void* buf, size_t len) {
    while (true) {
        ssize_t n = read(fd, buf, len);
        if (n >= 0) {
            return n;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 没有数据可读，可以做其他事情
            continue;  // 或者返回让调用者处理
        }
        if (errno == EINTR) {
            continue;  // 被信号中断，重试
        }
        return -1;  // 真正的错误
    }
}

// 非阻塞写入（处理部分写入）
ssize_t nonblocking_write_all(int fd, const void* buf, size_t len) {
    const char* ptr = static_cast<const char*>(buf);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t n = write(fd, ptr, remaining);
        if (n > 0) {
            ptr += n;
            remaining -= n;
        } else if (n == 0) {
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 发送缓冲区满，需要等待
                continue;
            }
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
    }
    return len - remaining;
}
```

### 第三周：select系统调用

```cpp
#include <sys/select.h>
#include <vector>

class SelectServer {
public:
    void run(int listen_fd) {
        fd_set master_set, read_set;
        FD_ZERO(&master_set);
        FD_SET(listen_fd, &master_set);
        int max_fd = listen_fd;

        while (true) {
            read_set = master_set;  // select会修改fd_set

            // 超时设置（可选）
            timeval timeout{5, 0};  // 5秒

            int ready = select(max_fd + 1, &read_set, nullptr, nullptr, &timeout);

            if (ready < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (ready == 0) {
                // 超时
                continue;
            }

            // 检查监听socket
            if (FD_ISSET(listen_fd, &read_set)) {
                int client = accept(listen_fd, nullptr, nullptr);
                if (client >= 0) {
                    FD_SET(client, &master_set);
                    if (client > max_fd) max_fd = client;
                }
            }

            // 检查客户端socket
            for (int fd = 0; fd <= max_fd; ++fd) {
                if (fd == listen_fd) continue;
                if (FD_ISSET(fd, &read_set)) {
                    char buf[1024];
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n <= 0) {
                        close(fd);
                        FD_CLR(fd, &master_set);
                    } else {
                        write(fd, buf, n);  // echo
                    }
                }
            }
        }
    }
};

/*
select的限制：
1. FD_SETSIZE限制（通常1024）
2. 每次调用都要重新设置fd_set
3. O(n)遍历所有fd
4. fd_set是位图，传递开销大
*/
```

### 第四周：poll系统调用

```cpp
#include <poll.h>
#include <vector>

class PollServer {
public:
    void run(int listen_fd) {
        std::vector<pollfd> fds;
        fds.push_back({listen_fd, POLLIN, 0});

        while (true) {
            int ready = poll(fds.data(), fds.size(), 5000);  // 5秒超时

            if (ready < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (ready == 0) continue;  // 超时

            // 处理事件
            for (size_t i = 0; i < fds.size(); ) {
                if (fds[i].revents == 0) {
                    ++i;
                    continue;
                }

                if (fds[i].fd == listen_fd) {
                    // 新连接
                    if (fds[i].revents & POLLIN) {
                        int client = accept(listen_fd, nullptr, nullptr);
                        if (client >= 0) {
                            fds.push_back({client, POLLIN, 0});
                        }
                    }
                    ++i;
                } else {
                    // 客户端数据
                    if (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                        char buf[1024];
                        ssize_t n = read(fds[i].fd, buf, sizeof(buf));
                        if (n <= 0) {
                            close(fds[i].fd);
                            fds.erase(fds.begin() + i);
                            continue;  // 不增加i
                        } else {
                            write(fds[i].fd, buf, n);
                        }
                    }
                    ++i;
                }
            }
        }
    }
};

/*
poll vs select：
优点：
1. 没有fd数量限制
2. 不需要每次重建集合
3. 事件类型更丰富

缺点（与select相同）：
1. 每次调用都要传递整个数组到内核
2. 返回后仍需O(n)遍历
*/
```

---

## 源码阅读任务

1. **libevent中的select/poll封装**
   - 查看`select.c`和`poll.c`
   - 理解事件循环的实现

2. **Redis的事件处理**
   - 阅读`ae_select.c`
   - 理解Redis如何封装I/O多路复用

---

## 实践项目

### 项目：统一事件循环框架

```cpp
// event_loop.hpp
#pragma once
#include <functional>
#include <unordered_map>
#include <vector>

enum EventType {
    EVENT_READ  = 1,
    EVENT_WRITE = 2,
    EVENT_ERROR = 4
};

using EventCallback = std::function<void(int fd, int events)>;

class EventLoop {
public:
    virtual ~EventLoop() = default;

    virtual void add_fd(int fd, int events, EventCallback cb) = 0;
    virtual void modify_fd(int fd, int events) = 0;
    virtual void remove_fd(int fd) = 0;
    virtual int poll(int timeout_ms) = 0;

    void run() {
        running_ = true;
        while (running_) {
            poll(1000);
        }
    }

    void stop() { running_ = false; }

protected:
    bool running_ = false;
    std::unordered_map<int, EventCallback> callbacks_;
};

// select_loop.hpp
#include "event_loop.hpp"
#include <sys/select.h>

class SelectLoop : public EventLoop {
public:
    void add_fd(int fd, int events, EventCallback cb) override {
        callbacks_[fd] = std::move(cb);
        fd_events_[fd] = events;
        if (fd > max_fd_) max_fd_ = fd;
    }

    void modify_fd(int fd, int events) override {
        fd_events_[fd] = events;
    }

    void remove_fd(int fd) override {
        callbacks_.erase(fd);
        fd_events_.erase(fd);
        // 重新计算max_fd_
        max_fd_ = -1;
        for (auto& [f, _] : fd_events_) {
            if (f > max_fd_) max_fd_ = f;
        }
    }

    int poll(int timeout_ms) override {
        fd_set read_set, write_set;
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);

        for (auto& [fd, events] : fd_events_) {
            if (events & EVENT_READ) FD_SET(fd, &read_set);
            if (events & EVENT_WRITE) FD_SET(fd, &write_set);
        }

        timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        int n = select(max_fd_ + 1, &read_set, &write_set, nullptr, &tv);

        if (n > 0) {
            for (auto& [fd, cb] : callbacks_) {
                int revents = 0;
                if (FD_ISSET(fd, &read_set)) revents |= EVENT_READ;
                if (FD_ISSET(fd, &write_set)) revents |= EVENT_WRITE;
                if (revents) cb(fd, revents);
            }
        }
        return n;
    }

private:
    std::unordered_map<int, int> fd_events_;
    int max_fd_ = -1;
};

// poll_loop.hpp
#include "event_loop.hpp"
#include <poll.h>

class PollLoop : public EventLoop {
public:
    void add_fd(int fd, int events, EventCallback cb) override {
        callbacks_[fd] = std::move(cb);
        pollfd pfd{fd, to_poll_events(events), 0};
        pollfds_.push_back(pfd);
    }

    void modify_fd(int fd, int events) override {
        for (auto& pfd : pollfds_) {
            if (pfd.fd == fd) {
                pfd.events = to_poll_events(events);
                break;
            }
        }
    }

    void remove_fd(int fd) override {
        callbacks_.erase(fd);
        pollfds_.erase(
            std::remove_if(pollfds_.begin(), pollfds_.end(),
                [fd](const pollfd& pfd) { return pfd.fd == fd; }),
            pollfds_.end()
        );
    }

    int poll(int timeout_ms) override {
        int n = ::poll(pollfds_.data(), pollfds_.size(), timeout_ms);

        if (n > 0) {
            for (auto& pfd : pollfds_) {
                if (pfd.revents) {
                    int events = from_poll_events(pfd.revents);
                    if (auto it = callbacks_.find(pfd.fd); it != callbacks_.end()) {
                        it->second(pfd.fd, events);
                    }
                    pfd.revents = 0;
                }
            }
        }
        return n;
    }

private:
    short to_poll_events(int events) {
        short pe = 0;
        if (events & EVENT_READ) pe |= POLLIN;
        if (events & EVENT_WRITE) pe |= POLLOUT;
        return pe;
    }

    int from_poll_events(short pe) {
        int events = 0;
        if (pe & POLLIN) events |= EVENT_READ;
        if (pe & POLLOUT) events |= EVENT_WRITE;
        if (pe & (POLLERR | POLLHUP)) events |= EVENT_ERROR;
        return events;
    }

    std::vector<pollfd> pollfds_;
};
```

---

## 检验标准

- [ ] 理解五种I/O模型的区别
- [ ] 掌握非阻塞I/O编程技巧
- [ ] 能使用select编写多客户端服务器
- [ ] 能使用poll编写多客户端服务器
- [ ] 理解select和poll的优缺点

### 输出物
1. `event_loop.hpp` - 事件循环抽象基类
2. `select_loop.hpp` - select实现
3. `poll_loop.hpp` - poll实现
4. `echo_server.cpp` - 使用事件循环的echo服务器
5. `notes/month26_io_models.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| I/O模型理论 | 25小时 |
| 非阻塞I/O编程 | 30小时 |
| select/poll实践 | 50小时 |
| 事件循环框架 | 35小时 |

---

## 下月预告

Month 27将学习**epoll深度解析**，这是Linux高性能网络编程的核心。
