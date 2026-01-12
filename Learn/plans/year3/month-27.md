# Month 27: epoll深度解析——Linux高性能I/O

## 本月主题概述

epoll是Linux下高性能网络编程的核心。本月深入学习epoll的实现原理、触发模式、最佳实践，并构建基于epoll的高性能服务器框架。

---

## 理论学习内容

### 第一周：epoll基础

**学习目标**：掌握epoll API和基本使用

**阅读材料**：
- [ ] `man epoll`、`man epoll_create`、`man epoll_ctl`、`man epoll_wait`
- [ ] 《Linux高性能服务器编程》epoll章节

**核心API**：

```cpp
#include <sys/epoll.h>

// 创建epoll实例
int epoll_create(int size);        // size已被忽略
int epoll_create1(int flags);      // flags: EPOLL_CLOEXEC

// 控制epoll
int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
// op: EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL

// 等待事件
int epoll_wait(int epfd, struct epoll_event* events,
               int maxevents, int timeout);
int epoll_pwait(int epfd, struct epoll_event* events,
                int maxevents, int timeout, const sigset_t* sigmask);

struct epoll_event {
    uint32_t events;    // 事件类型
    epoll_data_t data;  // 用户数据
};

union epoll_data {
    void* ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
};

// 事件类型
EPOLLIN      // 可读
EPOLLOUT     // 可写
EPOLLERR     // 错误
EPOLLHUP     // 挂起
EPOLLRDHUP   // 对端关闭
EPOLLET      // 边缘触发
EPOLLONESHOT // 一次性
EPOLLEXCLUSIVE // 独占唤醒（避免惊群）
```

### 第二周：LT与ET模式

```cpp
/*
水平触发 (Level Triggered, LT) - 默认模式：
- 只要fd就绪，每次epoll_wait都会通知
- 简单可靠，但可能有不必要的唤醒

边缘触发 (Edge Triggered, ET)：
- 只在fd状态变化时通知一次
- 高效，但必须一次处理完所有数据
- 必须使用非阻塞I/O
*/

// ET模式下的正确读取
void handle_read_et(int fd) {
    while (true) {
        char buf[4096];
        ssize_t n = read(fd, buf, sizeof(buf));

        if (n > 0) {
            process_data(buf, n);
            continue;
        }

        if (n == 0) {
            // 连接关闭
            close_connection(fd);
            break;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 数据读完了
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        // 错误
        handle_error(fd);
        break;
    }
}

// ET模式下的正确写入
void handle_write_et(int fd, const char* data, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = write(fd, data + sent, len - sent);

        if (n > 0) {
            sent += n;
            continue;
        }

        if (n == 0) {
            break;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 发送缓冲区满，需要等待EPOLLOUT
            save_pending_data(fd, data + sent, len - sent);
            modify_events(fd, EPOLLIN | EPOLLOUT | EPOLLET);
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        handle_error(fd);
        break;
    }
}
```

### 第三周：epoll内核实现原理

```
epoll数据结构：

struct eventpoll {
    struct rb_root rbr;      // 红黑树，存储所有监控的fd
    struct list_head rdllist; // 就绪链表
    ...
};

工作流程：
1. epoll_create: 创建eventpoll结构
2. epoll_ctl(ADD):
   - 创建epitem，插入红黑树
   - 在fd上注册回调函数
3. 当fd就绪时：
   - 内核调用回调函数
   - 将epitem加入就绪链表
4. epoll_wait:
   - 检查就绪链表
   - 如果空，睡眠等待
   - 有就绪事件，拷贝到用户空间

为什么epoll高效：
1. 红黑树管理fd：O(log n) 增删
2. 就绪链表：只返回就绪的fd
3. 无需每次拷贝整个fd集合
4. 事件驱动，无需轮询
```

### 第四周：EPOLLONESHOT与惊群问题

```cpp
// EPOLLONESHOT：事件触发后自动移除监听
// 用于多线程环境，确保同一fd同时只被一个线程处理

void add_oneshot(int epfd, int fd) {
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

void reset_oneshot(int epfd, int fd) {
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}

// EPOLLEXCLUSIVE：避免惊群（Linux 4.5+）
// 多个epoll实例监听同一个fd时，只唤醒一个

void add_exclusive(int epfd, int fd) {
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLEXCLUSIVE;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

/*
惊群问题：
- 多个进程/线程等待同一个fd
- fd就绪时，所有等待者都被唤醒
- 但只有一个能处理，其他白白唤醒

解决方案：
1. EPOLLEXCLUSIVE
2. SO_REUSEPORT（每个线程独立监听socket）
3. accept锁（Nginx方式）
*/
```

---

## 源码阅读任务

1. **Linux内核epoll实现**
   - `fs/eventpoll.c`
   - 理解红黑树和就绪链表

2. **libevent的epoll封装**
   - `epoll.c`
   - 理解抽象层设计

3. **Nginx的事件模块**
   - `ngx_epoll_module.c`
   - 理解高性能服务器的epoll使用

---

## 实践项目

### 项目：高性能epoll服务器框架

```cpp
// epoll_server.hpp
#pragma once
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <string>

class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;
using MessageCallback = std::function<void(ConnectionPtr, const std::string&)>;
using CloseCallback = std::function<void(ConnectionPtr)>;

class Buffer {
public:
    void append(const char* data, size_t len) {
        data_.insert(data_.end(), data, data + len);
    }

    std::string retrieve_all() {
        std::string result(data_.begin(), data_.end());
        data_.clear();
        return result;
    }

    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    const char* data() const { return data_.data(); }

    void consume(size_t len) {
        data_.erase(data_.begin(), data_.begin() + std::min(len, data_.size()));
    }

private:
    std::vector<char> data_;
};

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(int fd, const std::string& peer_addr)
        : fd_(fd), peer_addr_(peer_addr) {}

    ~Connection() {
        if (fd_ >= 0) close(fd_);
    }

    int fd() const { return fd_; }
    const std::string& peer_addr() const { return peer_addr_; }

    void send(const std::string& data) {
        output_buffer_.append(data.data(), data.size());
    }

    Buffer& input_buffer() { return input_buffer_; }
    Buffer& output_buffer() { return output_buffer_; }

private:
    int fd_;
    std::string peer_addr_;
    Buffer input_buffer_;
    Buffer output_buffer_;
};

class EpollServer {
public:
    EpollServer(int port, bool use_et = true)
        : port_(port), use_et_(use_et) {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }

    ~EpollServer() {
        if (listen_fd_ >= 0) close(listen_fd_);
        if (epfd_ >= 0) close(epfd_);
    }

    void set_message_callback(MessageCallback cb) {
        message_callback_ = std::move(cb);
    }

    void set_close_callback(CloseCallback cb) {
        close_callback_ = std::move(cb);
    }

    bool start() {
        listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listen_fd_ < 0) return false;

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) return false;
        if (listen(listen_fd_, SOMAXCONN) < 0) return false;

        add_fd(listen_fd_, EPOLLIN);
        return true;
    }

    void run() {
        running_ = true;
        std::vector<epoll_event> events(1024);

        while (running_) {
            int n = epoll_wait(epfd_, events.data(), events.size(), 1000);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                uint32_t revents = events[i].events;

                if (fd == listen_fd_) {
                    handle_accept();
                } else {
                    auto it = connections_.find(fd);
                    if (it == connections_.end()) continue;

                    if (revents & (EPOLLERR | EPOLLHUP)) {
                        handle_close(it->second);
                        continue;
                    }
                    if (revents & EPOLLIN) {
                        if (!handle_read(it->second)) {
                            handle_close(it->second);
                            continue;
                        }
                    }
                    if (revents & EPOLLOUT) {
                        handle_write(it->second);
                    }
                }
            }
        }
    }

    void stop() { running_ = false; }

private:
    void add_fd(int fd, uint32_t events) {
        epoll_event ev;
        ev.events = events | (use_et_ ? EPOLLET : 0);
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    void modify_fd(int fd, uint32_t events) {
        epoll_event ev;
        ev.events = events | (use_et_ ? EPOLLET : 0);
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
    }

    void remove_fd(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    }

    void handle_accept() {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            int client_fd = accept4(listen_fd_, (sockaddr*)&client_addr,
                                    &len, SOCK_NONBLOCK);
            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                break;
            }

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
            std::string peer = std::string(ip) + ":" +
                              std::to_string(ntohs(client_addr.sin_port));

            auto conn = std::make_shared<Connection>(client_fd, peer);
            connections_[client_fd] = conn;
            add_fd(client_fd, EPOLLIN);
        }
    }

    bool handle_read(ConnectionPtr conn) {
        while (true) {
            char buf[4096];
            ssize_t n = read(conn->fd(), buf, sizeof(buf));
            if (n > 0) {
                conn->input_buffer().append(buf, n);
                continue;
            }
            if (n == 0) return false;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            return false;
        }

        if (!conn->input_buffer().empty() && message_callback_) {
            message_callback_(conn, conn->input_buffer().retrieve_all());
        }
        if (!conn->output_buffer().empty()) {
            handle_write(conn);
        }
        return true;
    }

    void handle_write(ConnectionPtr conn) {
        auto& buf = conn->output_buffer();
        while (!buf.empty()) {
            ssize_t n = write(conn->fd(), buf.data(), buf.size());
            if (n > 0) { buf.consume(n); continue; }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                modify_fd(conn->fd(), EPOLLIN | EPOLLOUT);
                break;
            }
            if (errno == EINTR) continue;
            break;
        }
        if (buf.empty()) {
            modify_fd(conn->fd(), EPOLLIN);
        }
    }

    void handle_close(ConnectionPtr conn) {
        if (close_callback_) close_callback_(conn);
        remove_fd(conn->fd());
        connections_.erase(conn->fd());
    }

private:
    int port_;
    bool use_et_;
    int epfd_ = -1;
    int listen_fd_ = -1;
    bool running_ = false;
    std::unordered_map<int, ConnectionPtr> connections_;
    MessageCallback message_callback_;
    CloseCallback close_callback_;
};
```

---

## 检验标准

- [ ] 掌握epoll三个核心API的使用
- [ ] 理解LT和ET模式的区别和适用场景
- [ ] 能正确处理ET模式下的边界情况
- [ ] 理解epoll的内核实现原理
- [ ] 实现完整的epoll服务器框架

### 输出物
1. `epoll_server.hpp` - epoll服务器框架
2. `echo_server.cpp` - Echo服务器
3. `benchmark.cpp` - 性能测试工具
4. `notes/month27_epoll.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| epoll API学习 | 20小时 |
| LT/ET模式深入 | 30小时 |
| 内核源码阅读 | 30小时 |
| 服务器框架实现 | 50小时 |
| 性能测试与优化 | 10小时 |

---

## 下月预告

Month 28将学习**io_uring——新一代异步I/O**，掌握Linux最新的高性能I/O接口。
