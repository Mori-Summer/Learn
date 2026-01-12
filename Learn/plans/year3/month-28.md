# Month 28: io_uring——新一代异步I/O

## 本月主题概述

io_uring是Linux 5.1引入的全新异步I/O框架，通过共享内存的环形缓冲区实现真正的零拷贝系统调用。本月深入学习io_uring的原理和使用。

---

## 理论学习内容

### 第一周：io_uring基础概念

**学习目标**：理解io_uring的设计动机和核心架构

**阅读材料**：
- [ ] `man io_uring`
- [ ] Jens Axboe的io_uring介绍文章
- [ ] [io_uring官方文档](https://kernel.dk/io_uring.pdf)

**核心概念**：

```
为什么需要io_uring：

传统I/O问题：
┌─────────┐  系统调用  ┌─────────┐
│ 用户态  │ ─────────> │ 内核态  │
│         │ <───────── │         │
└─────────┘  上下文切换 └─────────┘
每次I/O都需要系统调用开销

io_uring方案：
┌─────────────────────────────────────┐
│           共享内存区域              │
│  ┌─────────┐       ┌─────────┐     │
│  │   SQ    │       │   CQ    │     │
│  │提交队列 │       │完成队列 │     │
│  └─────────┘       └─────────┘     │
└─────────────────────────────────────┘
      │                    ▲
      ▼                    │
┌─────────┐          ┌─────────┐
│ 用户态  │          │ 内核态  │
│写入SQE  │          │写入CQE  │
└─────────┘          └─────────┘

核心组件：
1. SQ (Submission Queue) - 提交队列
2. CQ (Completion Queue) - 完成队列
3. SQE (Submission Queue Entry) - 提交队列条目
4. CQE (Completion Queue Entry) - 完成队列条目
```

### 第二周：io_uring API

```cpp
#include <liburing.h>

// 初始化io_uring
struct io_uring ring;
io_uring_queue_init(256, &ring, 0);  // 256个entries

// 获取SQE并准备请求
struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);

// 准备读操作
io_uring_prep_read(sqe, fd, buf, len, offset);
io_uring_sqe_set_data(sqe, user_data);  // 设置用户数据

// 准备写操作
io_uring_prep_write(sqe, fd, buf, len, offset);

// 准备accept
io_uring_prep_accept(sqe, listen_fd, addr, addrlen, 0);

// 准备connect
io_uring_prep_connect(sqe, fd, addr, addrlen);

// 准备recv/send
io_uring_prep_recv(sqe, fd, buf, len, 0);
io_uring_prep_send(sqe, fd, buf, len, 0);

// 提交请求
io_uring_submit(&ring);

// 等待完成
struct io_uring_cqe* cqe;
io_uring_wait_cqe(&ring, &cqe);

// 处理完成事件
int result = cqe->res;
void* user_data = io_uring_cqe_get_data(cqe);

// 标记CQE已处理
io_uring_cqe_seen(&ring, cqe);

// 清理
io_uring_queue_exit(&ring);
```

### 第三周：高级特性

```cpp
// 1. SQPOLL模式 - 内核轮询提交队列
struct io_uring_params params = {};
params.flags = IORING_SETUP_SQPOLL;
params.sq_thread_idle = 2000;  // 2秒空闲后睡眠
io_uring_queue_init_params(256, &ring, &params);

// 2. 注册文件描述符（减少每次操作的开销）
int fds[10] = {...};
io_uring_register_files(&ring, fds, 10);
// 使用时：
sqe->flags |= IOSQE_FIXED_FILE;
sqe->fd = registered_index;  // 使用索引而非fd

// 3. 注册缓冲区
struct iovec iovecs[10];
io_uring_register_buffers(&ring, iovecs, 10);
// 使用固定缓冲区：
io_uring_prep_read_fixed(sqe, fd, buf, len, offset, buf_index);

// 4. 链接操作（按顺序执行）
sqe = io_uring_get_sqe(&ring);
io_uring_prep_read(sqe, fd, buf, len, 0);
sqe->flags |= IOSQE_IO_LINK;  // 与下一个操作链接

sqe = io_uring_get_sqe(&ring);
io_uring_prep_write(sqe, fd2, buf, len, 0);
// 只有第一个成功，第二个才执行

// 5. 超时操作
struct __kernel_timespec ts = {.tv_sec = 5, .tv_nsec = 0};
io_uring_prep_timeout(sqe, &ts, 0, 0);

// 6. 取消操作
io_uring_prep_cancel(sqe, user_data, 0);
```

### 第四周：io_uring与网络编程

```cpp
// 多重等待 - 高效获取多个完成事件
unsigned head;
struct io_uring_cqe* cqe;
int count = 0;

io_uring_for_each_cqe(&ring, head, cqe) {
    process_cqe(cqe);
    count++;
}
io_uring_cq_advance(&ring, count);

// 多shot accept（Linux 5.19+）
sqe = io_uring_get_sqe(&ring);
io_uring_prep_multishot_accept(sqe, listen_fd, nullptr, nullptr, 0);
// 一次提交，多次完成

// 提供缓冲区（Linux 5.7+）
// 让内核选择缓冲区，减少用户态管理
io_uring_prep_recv(sqe, fd, nullptr, buf_len, 0);
sqe->buf_group = buf_group_id;
sqe->flags |= IOSQE_BUFFER_SELECT;
```

---

## 源码阅读任务

1. **liburing源码**
   - 理解用户态封装
   - 查看内存屏障使用

2. **io_uring内核实现**
   - `io_uring/io_uring.c`
   - 理解环形缓冲区设计

---

## 实践项目

### 项目：基于io_uring的高性能Echo服务器

```cpp
// uring_server.hpp
#pragma once
#include <liburing.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <memory>

enum class EventType : uint8_t {
    ACCEPT, READ, WRITE, CLOSE
};

struct Request {
    EventType type;
    int client_fd;
    int buf_idx;
    size_t len;
};

class IoUringServer {
public:
    static constexpr int BUFFER_SIZE = 4096;
    static constexpr int MAX_BUFFERS = 1024;
    static constexpr int QUEUE_DEPTH = 256;

    IoUringServer(int port) : port_(port) {
        buffers_.resize(MAX_BUFFERS * BUFFER_SIZE);
        for (int i = 0; i < MAX_BUFFERS; ++i) {
            free_buffers_.push_back(i);
        }
    }

    ~IoUringServer() {
        if (ring_initialized_) io_uring_queue_exit(&ring_);
        if (listen_fd_ >= 0) close(listen_fd_);
    }

    bool start() {
        if (io_uring_queue_init(QUEUE_DEPTH, &ring_, 0) < 0) return false;
        ring_initialized_ = true;

        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) return false;

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) return false;
        if (listen(listen_fd_, SOMAXCONN) < 0) return false;

        submit_accept();
        return true;
    }

    void run() {
        running_ = true;
        while (running_) {
            io_uring_submit(&ring_);

            struct io_uring_cqe* cqe;
            if (io_uring_wait_cqe(&ring_, &cqe) < 0) continue;

            unsigned head;
            unsigned count = 0;
            io_uring_for_each_cqe(&ring_, head, cqe) {
                handle_cqe(cqe);
                count++;
            }
            io_uring_cq_advance(&ring_, count);
        }
    }

    void stop() { running_ = false; }

private:
    char* get_buffer(int idx) {
        return buffers_.data() + idx * BUFFER_SIZE;
    }

    int alloc_buffer() {
        if (free_buffers_.empty()) return -1;
        int idx = free_buffers_.back();
        free_buffers_.pop_back();
        return idx;
    }

    void free_buffer(int idx) {
        if (idx >= 0) free_buffers_.push_back(idx);
    }

    void submit_accept() {
        auto* sqe = io_uring_get_sqe(&ring_);
        auto* req = new Request{EventType::ACCEPT, -1, -1, 0};
        io_uring_prep_accept(sqe, listen_fd_, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void submit_read(int client_fd) {
        int buf_idx = alloc_buffer();
        if (buf_idx < 0) { close(client_fd); return; }

        auto* sqe = io_uring_get_sqe(&ring_);
        auto* req = new Request{EventType::READ, client_fd, buf_idx, 0};
        io_uring_prep_recv(sqe, client_fd, get_buffer(buf_idx), BUFFER_SIZE, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void submit_write(int client_fd, int buf_idx, size_t len) {
        auto* sqe = io_uring_get_sqe(&ring_);
        auto* req = new Request{EventType::WRITE, client_fd, buf_idx, len};
        io_uring_prep_send(sqe, client_fd, get_buffer(buf_idx), len, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void handle_cqe(struct io_uring_cqe* cqe) {
        auto* req = static_cast<Request*>(io_uring_cqe_get_data(cqe));
        if (!req) return;

        int result = cqe->res;

        switch (req->type) {
            case EventType::ACCEPT:
                if (result >= 0) submit_read(result);
                delete req;
                submit_accept();
                break;

            case EventType::READ:
                if (result <= 0) {
                    close(req->client_fd);
                    free_buffer(req->buf_idx);
                } else {
                    submit_write(req->client_fd, req->buf_idx, result);
                }
                delete req;
                break;

            case EventType::WRITE:
                free_buffer(req->buf_idx);
                if (result > 0) submit_read(req->client_fd);
                else close(req->client_fd);
                delete req;
                break;

            default:
                delete req;
        }
    }

private:
    int port_;
    int listen_fd_ = -1;
    struct io_uring ring_;
    bool ring_initialized_ = false;
    bool running_ = false;
    std::vector<char> buffers_;
    std::vector<int> free_buffers_;
};
```

---

## 检验标准

- [ ] 理解io_uring的设计原理和优势
- [ ] 掌握io_uring核心API使用
- [ ] 理解SQ/CQ环形缓冲区机制
- [ ] 能使用io_uring实现网络服务器
- [ ] 了解io_uring高级特性

### 输出物
1. `uring_server.hpp` - io_uring服务器框架
2. `uring_echo_server.cpp` - Echo服务器实现
3. `uring_vs_epoll_benchmark.cpp` - 性能对比测试
4. `notes/month28_io_uring.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| io_uring概念学习 | 25小时 |
| API实践 | 35小时 |
| 高级特性探索 | 30小时 |
| 服务器实现 | 40小时 |
| 性能对比测试 | 10小时 |

---

## 下月预告

Month 29将学习**零拷贝技术**，包括sendfile、splice、mmap等技术。
