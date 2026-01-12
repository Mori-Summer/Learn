# Month 22: 协程应用——异步I/O与事件循环

## 本月主题概述

将协程与异步I/O结合，实现高性能的网络编程。学习事件循环的设计，以及如何用协程简化异步代码。

---

## 理论学习内容

### 第一周：事件循环设计

```cpp
// 单线程事件循环
class EventLoop {
    std::queue<std::coroutine_handle<>> ready_queue_;
    // + I/O多路复用（epoll/kqueue/IOCP）

public:
    void schedule(std::coroutine_handle<> handle) {
        ready_queue_.push(handle);
    }

    void run() {
        while (!ready_queue_.empty()) {
            auto handle = ready_queue_.front();
            ready_queue_.pop();
            handle.resume();
        }
    }
};
```

### 第二周：异步I/O封装

```cpp
// 将系统I/O操作封装为Awaitable
struct AsyncRead {
    int fd;
    char* buffer;
    size_t size;
    ssize_t result = -1;

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        // 注册到事件循环
        // 当fd可读时，resume协程
    }

    ssize_t await_resume() { return result; }
};
```

### 第三周：协程Socket封装

```cpp
class AsyncSocket {
    int fd_;
    EventLoop& loop_;

public:
    Task<ssize_t> read(char* buf, size_t len);
    Task<ssize_t> write(const char* buf, size_t len);
    Task<void> connect(const std::string& host, int port);
    Task<AsyncSocket> accept();
};
```

### 第四周：实际应用

- 协程HTTP客户端
- 协程Echo服务器

---

## 实践项目

### 项目：协程网络库

简化版的异步网络库，支持：
- TCP连接
- 异步读写
- 超时处理

---

## 检验标准

- [ ] 实现简单的事件循环
- [ ] 将I/O操作封装为协程
- [ ] 完成协程Echo服务器

### 输出物
1. `event_loop.hpp`
2. `async_socket.hpp`
3. `examples/echo_server.cpp`
4. `notes/month22_async_io.md`

---

## 下月预告

Month 23将学习**并发模式与最佳实践**，总结常见的并发设计模式。
