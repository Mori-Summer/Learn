# Month 31: Proactor模式——异步完成通知

## 本月主题概述

Proactor模式与Reactor不同，它基于异步I/O操作完成后的通知。本月学习Proactor模式原理，Windows IOCP，以及如何用io_uring实现Proactor。

---

## 理论学习内容

### 第一周：Proactor vs Reactor

**学习目标**：理解两种模式的本质区别

**阅读材料**：
- [ ] 《POSA Vol.2》Proactor章节
- [ ] Douglas Schmidt的ACE框架论文

**核心概念**：

```
Reactor模式（同步I/O多路复用）：
┌─────────┐  1.注册   ┌─────────┐
│ Handler │ ────────> │ Reactor │
└─────────┘           └────┬────┘
                           │ 2.等待就绪
                           ▼
                      ┌─────────┐
                      │ epoll   │
                      └────┬────┘
                           │ 3.通知就绪
┌─────────┐  <──────────────┘
│ Handler │  4.用户执行I/O操作
└─────────┘

Proactor模式（异步I/O）：
┌─────────┐  1.发起异步I/O  ┌─────────┐
│ Handler │ ─────────────> │Proactor │
└─────────┘                └────┬────┘
                                │ 2.内核执行I/O
                                ▼
                           ┌─────────┐
                           │  内核   │
                           └────┬────┘
                                │ 3.I/O完成，通知
┌─────────┐  <──────────────────┘
│ Handler │  4.处理完成的数据
└─────────┘

关键区别：
Reactor: 内核告知"可以读了"，用户自己读
Proactor: 用户发起读请求，内核完成后告知"读完了"
```

### 第二周：Windows IOCP

```cpp
// IOCP概念（跨平台理解很重要）
/*
IOCP核心概念：
1. 完成端口 - 内核对象，收集完成通知
2. 重叠I/O - 异步操作句柄
3. 完成键 - 用户自定义数据

工作流程：
1. CreateIoCompletionPort() - 创建完成端口
2. 关联socket到完成端口
3. WSARecv/WSASend - 发起异步操作
4. GetQueuedCompletionStatus() - 等待完成通知
*/

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

class IOCPServer {
public:
    IOCPServer() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    }

    ~IOCPServer() {
        if (iocp_) CloseHandle(iocp_);
        WSACleanup();
    }

    void associate(SOCKET sock, ULONG_PTR key) {
        CreateIoCompletionPort((HANDLE)sock, iocp_, key, 0);
    }

    void run() {
        while (running_) {
            DWORD bytes;
            ULONG_PTR key;
            OVERLAPPED* overlapped;

            BOOL result = GetQueuedCompletionStatus(
                iocp_, &bytes, &key, &overlapped, INFINITE);

            if (result) {
                // 处理完成的I/O
                handle_completion(key, overlapped, bytes);
            }
        }
    }

private:
    HANDLE iocp_ = nullptr;
    bool running_ = true;

    void handle_completion(ULONG_PTR key, OVERLAPPED* overlapped, DWORD bytes);
};
#endif
```

### 第三周：用io_uring实现Proactor

```cpp
// proactor_uring.hpp
#pragma once
#include <liburing.h>
#include <functional>
#include <memory>
#include <unordered_map>

// 异步操作基类
class AsyncOperation {
public:
    virtual ~AsyncOperation() = default;
    virtual void on_complete(int result) = 0;
};

// 异步读操作
class AsyncRead : public AsyncOperation {
public:
    using Callback = std::function<void(int result, const char* data, size_t len)>;

    AsyncRead(int fd, Callback cb)
        : fd_(fd), callback_(std::move(cb)) {
        buffer_.resize(4096);
    }

    int fd() const { return fd_; }
    char* buffer() { return buffer_.data(); }
    size_t buffer_size() const { return buffer_.size(); }

    void on_complete(int result) override {
        if (result > 0) {
            callback_(result, buffer_.data(), result);
        } else {
            callback_(result, nullptr, 0);
        }
    }

private:
    int fd_;
    std::vector<char> buffer_;
    Callback callback_;
};

// 异步写操作
class AsyncWrite : public AsyncOperation {
public:
    using Callback = std::function<void(int result)>;

    AsyncWrite(int fd, std::string data, Callback cb)
        : fd_(fd), data_(std::move(data)), callback_(std::move(cb)) {}

    int fd() const { return fd_; }
    const char* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }

    void on_complete(int result) override {
        callback_(result);
    }

private:
    int fd_;
    std::string data_;
    Callback callback_;
};

// Proactor实现
class Proactor {
public:
    Proactor(int queue_depth = 256) {
        io_uring_queue_init(queue_depth, &ring_, 0);
    }

    ~Proactor() {
        io_uring_queue_exit(&ring_);
    }

    // 发起异步读
    void async_read(int fd, AsyncRead::Callback callback) {
        auto op = std::make_unique<AsyncRead>(fd, std::move(callback));
        auto* sqe = io_uring_get_sqe(&ring_);

        io_uring_prep_recv(sqe, op->fd(), op->buffer(), op->buffer_size(), 0);
        io_uring_sqe_set_data(sqe, op.get());

        pending_ops_[op.get()] = std::move(op);
    }

    // 发起异步写
    void async_write(int fd, std::string data, AsyncWrite::Callback callback) {
        auto op = std::make_unique<AsyncWrite>(fd, std::move(data), std::move(callback));
        auto* sqe = io_uring_get_sqe(&ring_);

        io_uring_prep_send(sqe, op->fd(), op->data(), op->size(), 0);
        io_uring_sqe_set_data(sqe, op.get());

        pending_ops_[op.get()] = std::move(op);
    }

    // 发起异步accept
    void async_accept(int listen_fd, std::function<void(int)> callback) {
        struct AcceptOp : AsyncOperation {
            std::function<void(int)> cb;
            void on_complete(int result) override { cb(result); }
        };

        auto op = std::make_unique<AcceptOp>();
        op->cb = std::move(callback);

        auto* sqe = io_uring_get_sqe(&ring_);
        io_uring_prep_accept(sqe, listen_fd, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, op.get());

        pending_ops_[op.get()] = std::move(op);
    }

    void run() {
        running_ = true;

        while (running_) {
            io_uring_submit(&ring_);

            struct io_uring_cqe* cqe;
            int ret = io_uring_wait_cqe(&ring_, &cqe);
            if (ret < 0) continue;

            // 处理所有完成事件
            unsigned head;
            unsigned count = 0;

            io_uring_for_each_cqe(&ring_, head, cqe) {
                auto* op = static_cast<AsyncOperation*>(
                    io_uring_cqe_get_data(cqe));

                if (op) {
                    op->on_complete(cqe->res);
                    pending_ops_.erase(op);
                }
                count++;
            }

            io_uring_cq_advance(&ring_, count);
        }
    }

    void stop() { running_ = false; }

private:
    struct io_uring ring_;
    bool running_ = false;
    std::unordered_map<void*, std::unique_ptr<AsyncOperation>> pending_ops_;
};
```

### 第四周：跨平台Proactor封装

```cpp
// async_io.hpp - 跨平台异步I/O接口
#pragma once
#include <functional>
#include <memory>
#include <string>

// 平台无关的异步I/O接口
class IAsyncIO {
public:
    virtual ~IAsyncIO() = default;

    using ReadCallback = std::function<void(int result, const char* data, size_t len)>;
    using WriteCallback = std::function<void(int result)>;
    using AcceptCallback = std::function<void(int client_fd)>;

    virtual void async_read(int fd, ReadCallback cb) = 0;
    virtual void async_write(int fd, std::string data, WriteCallback cb) = 0;
    virtual void async_accept(int listen_fd, AcceptCallback cb) = 0;

    virtual void run() = 0;
    virtual void stop() = 0;
};

// 工厂函数
std::unique_ptr<IAsyncIO> create_async_io();

// 实现选择
#ifdef _WIN32
// Windows使用IOCP
std::unique_ptr<IAsyncIO> create_async_io() {
    return std::make_unique<IOCPAsyncIO>();
}
#elif defined(__linux__)
// Linux使用io_uring
std::unique_ptr<IAsyncIO> create_async_io() {
    return std::make_unique<UringAsyncIO>();
}
#else
// 其他平台使用模拟的Proactor（基于epoll）
std::unique_ptr<IAsyncIO> create_async_io() {
    return std::make_unique<SimulatedProactor>();
}
#endif

// 使用示例
void example() {
    auto io = create_async_io();

    int listen_fd = /* create listening socket */;

    std::function<void(int)> on_accept = [&](int client_fd) {
        if (client_fd >= 0) {
            // 异步读取
            io->async_read(client_fd, [&, client_fd](int result, const char* data, size_t len) {
                if (result > 0) {
                    // 异步写回
                    io->async_write(client_fd, std::string(data, len), [client_fd](int r) {
                        if (r < 0) close(client_fd);
                    });
                    // 继续读取
                    io->async_read(client_fd, /* ... */);
                } else {
                    close(client_fd);
                }
            });
        }
        // 继续accept
        io->async_accept(listen_fd, on_accept);
    };

    io->async_accept(listen_fd, on_accept);
    io->run();
}
```

---

## 源码阅读任务

1. **Boost.Asio源码**
   - io_context实现
   - 异步操作调度

2. **libuv源码**
   - Windows IOCP封装
   - Linux平台模拟

---

## 实践项目

### 项目：基于Proactor的Echo服务器

```cpp
// proactor_echo_server.cpp
#include "proactor_uring.hpp"
#include <netinet/in.h>
#include <cstdio>

class EchoSession : public std::enable_shared_from_this<EchoSession> {
public:
    EchoSession(Proactor& proactor, int fd)
        : proactor_(proactor), fd_(fd) {}

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        proactor_.async_read(fd_, [self](int result, const char* data, size_t len) {
            if (result > 0) {
                self->do_write(std::string(data, len));
            } else {
                close(self->fd_);
            }
        });
    }

    void do_write(std::string data) {
        auto self = shared_from_this();
        proactor_.async_write(fd_, std::move(data), [self](int result) {
            if (result > 0) {
                self->do_read();
            } else {
                close(self->fd_);
            }
        });
    }

private:
    Proactor& proactor_;
    int fd_;
};

int main() {
    Proactor proactor;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    bind(listen_fd, (sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, SOMAXCONN);

    std::function<void(int)> do_accept;
    do_accept = [&](int client_fd) {
        if (client_fd >= 0) {
            auto session = std::make_shared<EchoSession>(proactor, client_fd);
            session->start();
        }
        proactor.async_accept(listen_fd, do_accept);
    };

    printf("Proactor Echo Server on port 8080\n");
    proactor.async_accept(listen_fd, do_accept);
    proactor.run();

    return 0;
}
```

---

## 检验标准

- [ ] 理解Reactor和Proactor的本质区别
- [ ] 了解Windows IOCP的工作原理
- [ ] 能用io_uring实现Proactor模式
- [ ] 理解跨平台异步I/O的设计思路
- [ ] 实现基于Proactor的服务器

### 输出物
1. `proactor_uring.hpp` - io_uring Proactor实现
2. `async_io.hpp` - 跨平台接口
3. `proactor_echo_server.cpp` - Echo服务器
4. `notes/month31_proactor.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| Proactor理论 | 25小时 |
| IOCP研究 | 25小时 |
| io_uring Proactor实现 | 40小时 |
| 跨平台封装 | 30小时 |
| 测试与文档 | 20小时 |

---

## 下月预告

Month 32将学习**Envoy架构分析**，研究云原生代理的设计。
