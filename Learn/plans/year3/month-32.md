# Month 32: Envoy架构分析——云原生代理

## 本月主题概述

Envoy是云原生时代最重要的网络代理之一。本月深入分析Envoy的架构设计，学习其线程模型、配置热更新机制，以及高性能设计思路。

---

## 理论学习内容

### 第一周：Envoy架构概览

**学习目标**：理解Envoy的整体架构

**阅读材料**：
- [ ] Envoy官方文档架构部分
- [ ] Matt Klein的Envoy设计博客

**核心概念**：

```
Envoy整体架构：

┌─────────────────────────────────────────────────┐
│                    Envoy                         │
│  ┌───────────────────────────────────────────┐  │
│  │              Listener                      │  │
│  │  ┌─────────────────────────────────────┐  │  │
│  │  │          Filter Chain               │  │  │
│  │  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌──────┐  │  │  │
│  │  │  │ TLS │→│HTTP │→│Router│→│Cluster│ │  │  │
│  │  │  └─────┘ └─────┘ └─────┘ └──────┘  │  │  │
│  │  └─────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────┘  │
│                                                  │
│  ┌───────────────────────────────────────────┐  │
│  │           Cluster Manager                  │  │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐   │  │
│  │  │Cluster A│  │Cluster B│  │Cluster C│   │  │
│  │  │ - Host1 │  │ - Host1 │  │ - Host1 │   │  │
│  │  │ - Host2 │  │ - Host2 │  │ - Host2 │   │  │
│  │  └─────────┘  └─────────┘  └─────────┘   │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘

核心概念：
1. Listener - 监听端口，接收连接
2. Filter Chain - 过滤器链，处理连接
3. Cluster - 上游服务集群
4. Endpoint - 集群中的具体节点
5. Route - 路由规则
```

### 第二周：线程模型

```
Envoy线程模型：

┌─────────────────────────────────────────────────┐
│                  Main Thread                     │
│  - 配置加载和管理                                │
│  - xDS API通信                                   │
│  - 统计数据聚合                                  │
│  - 管理API                                       │
└────────────────────┬────────────────────────────┘
                     │ 配置分发
    ┌────────────────┼────────────────┐
    ▼                ▼                ▼
┌────────┐      ┌────────┐      ┌────────┐
│Worker 1│      │Worker 2│      │Worker N│
│        │      │        │      │        │
│ epoll  │      │ epoll  │      │ epoll  │
│ loop   │      │ loop   │      │ loop   │
│        │      │        │      │        │
│连接处理 │      │连接处理 │      │连接处理 │
└────────┘      └────────┘      └────────┘

线程模型特点：
1. Main线程负责控制面
2. Worker线程负责数据面
3. 每个Worker有独立的epoll
4. 使用Thread Local Storage避免锁
5. 配置更新通过TLS槽位传递
```

```cpp
// Envoy线程模型简化示意
class ThreadLocal {
public:
    // 每个线程独立的槽位
    template<typename T>
    T& get() {
        return *static_cast<T*>(tls_[current_slot_]);
    }

    // 主线程更新，传播到所有Worker
    template<typename T>
    void set(std::unique_ptr<T> data) {
        // 异步投递到所有Worker线程
        for (auto& worker : workers_) {
            worker.post([data = data.get()] {
                tls_[slot] = data;
            });
        }
    }
};

class Worker {
public:
    void run() {
        while (running_) {
            // 处理事件
            dispatcher_.run();
        }
    }

private:
    Dispatcher dispatcher_;  // 事件循环
    // 每个Worker独立的连接、路由表等
};
```

### 第三周：Filter机制

```cpp
// Envoy Filter接口
class ReadFilter {
public:
    virtual ~ReadFilter() = default;

    // 新连接建立
    virtual FilterStatus onNewConnection() {
        return FilterStatus::Continue;
    }

    // 读取数据
    virtual FilterStatus onData(Buffer::Instance& data, bool end_stream) = 0;
};

class WriteFilter {
public:
    virtual ~WriteFilter() = default;

    // 写入数据
    virtual FilterStatus onWrite(Buffer::Instance& data, bool end_stream) = 0;
};

// HTTP Filter
class StreamDecoderFilter {
public:
    // 请求头
    virtual FilterHeadersStatus decodeHeaders(RequestHeaderMap& headers,
                                              bool end_stream) = 0;
    // 请求体
    virtual FilterDataStatus decodeData(Buffer::Instance& data,
                                        bool end_stream) = 0;
    // 请求尾
    virtual FilterTrailersStatus decodeTrailers(RequestTrailerMap& trailers) = 0;
};

class StreamEncoderFilter {
public:
    // 响应头
    virtual FilterHeadersStatus encodeHeaders(ResponseHeaderMap& headers,
                                              bool end_stream) = 0;
    // 响应体
    virtual FilterDataStatus encodeData(Buffer::Instance& data,
                                        bool end_stream) = 0;
};

// Filter状态
enum class FilterStatus {
    Continue,        // 继续下一个Filter
    StopIteration,   // 暂停，等待异步操作
};
```

### 第四周：配置热更新

```
xDS协议（动态配置发现）：

┌────────────┐                    ┌─────────────┐
│   Envoy    │◄───── gRPC ───────│ Control     │
│            │                    │ Plane       │
│  Listener  │◄──── LDS ─────────│ (Istio等)   │
│  Route     │◄──── RDS ─────────│             │
│  Cluster   │◄──── CDS ─────────│             │
│  Endpoint  │◄──── EDS ─────────│             │
│  Secret    │◄──── SDS ─────────│             │
└────────────┘                    └─────────────┘

xDS类型：
- LDS (Listener Discovery Service)
- RDS (Route Discovery Service)
- CDS (Cluster Discovery Service)
- EDS (Endpoint Discovery Service)
- SDS (Secret Discovery Service)
- ADS (Aggregated Discovery Service)

配置更新流程：
1. 控制面推送新配置
2. Main线程接收并验证
3. 创建新的配置对象
4. 通过TLS传播到Worker
5. 旧连接使用旧配置
6. 新连接使用新配置
7. 旧配置引用计数归零后释放
```

---

## 源码阅读任务

1. **Envoy核心模块**
   - `source/common/event/dispatcher_impl.cc` - 事件循环
   - `source/server/worker_impl.cc` - Worker线程
   - `source/common/network/connection_impl.cc` - 连接管理

2. **Filter实现**
   - `source/common/http/conn_manager_impl.cc` - HTTP连接管理
   - `source/extensions/filters/http/router/router.cc` - 路由Filter

3. **配置更新**
   - `source/common/config/subscription_factory_impl.cc` - xDS订阅
   - `source/common/thread_local/thread_local_impl.cc` - TLS实现

---

## 实践项目

### 项目：简化版代理服务器

```cpp
// simple_proxy.hpp
#pragma once
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <thread>

// 简化的Filter接口
class Filter {
public:
    virtual ~Filter() = default;
    virtual bool on_read(std::vector<char>& data) { return true; }
    virtual bool on_write(std::vector<char>& data) { return true; }
};

// 连接
class Connection {
public:
    int client_fd;
    int upstream_fd;
    std::vector<char> client_buffer;
    std::vector<char> upstream_buffer;
    std::vector<std::unique_ptr<Filter>> filters;
};

// Worker线程
class Worker {
public:
    Worker() {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }

    void add_connection(std::unique_ptr<Connection> conn) {
        int client_fd = conn->client_fd;
        connections_[client_fd] = std::move(conn);

        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, client_fd, &ev);
    }

    void run() {
        running_ = true;
        std::vector<epoll_event> events(256);

        while (running_) {
            int n = epoll_wait(epfd_, events.data(), events.size(), 100);

            for (int i = 0; i < n; ++i) {
                handle_event(events[i].data.fd, events[i].events);
            }
        }
    }

    void stop() { running_ = false; }

private:
    void handle_event(int fd, uint32_t events);

private:
    int epfd_;
    bool running_ = false;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
};

// 代理服务器
class SimpleProxy {
public:
    SimpleProxy(int port, const std::string& upstream_host, int upstream_port,
                size_t num_workers = 4)
        : port_(port), upstream_host_(upstream_host),
          upstream_port_(upstream_port), workers_(num_workers) {}

    bool start() {
        // 创建监听socket
        listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) return false;
        if (listen(listen_fd_, SOMAXCONN) < 0) return false;

        // 启动Worker线程
        for (auto& worker : workers_) {
            worker_threads_.emplace_back([&worker] { worker.run(); });
        }

        return true;
    }

    void run() {
        running_ = true;
        size_t next_worker = 0;

        while (running_) {
            int client = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK);
            if (client < 0) {
                if (errno == EAGAIN) {
                    usleep(1000);
                    continue;
                }
                continue;
            }

            // 连接上游
            int upstream = connect_upstream();
            if (upstream < 0) {
                close(client);
                continue;
            }

            auto conn = std::make_unique<Connection>();
            conn->client_fd = client;
            conn->upstream_fd = upstream;

            // 轮询分配给Worker
            workers_[next_worker++ % workers_.size()].add_connection(std::move(conn));
        }
    }

    void stop() {
        running_ = false;
        for (auto& worker : workers_) {
            worker.stop();
        }
        for (auto& t : worker_threads_) {
            if (t.joinable()) t.join();
        }
    }

private:
    int connect_upstream() {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(upstream_port_);
        inet_pton(AF_INET, upstream_host_.c_str(), &addr.sin_addr);
        connect(fd, (sockaddr*)&addr, sizeof(addr));
        return fd;
    }

private:
    int port_;
    std::string upstream_host_;
    int upstream_port_;
    int listen_fd_ = -1;
    bool running_ = false;
    std::vector<Worker> workers_;
    std::vector<std::thread> worker_threads_;
};
```

---

## 检验标准

- [ ] 理解Envoy的整体架构
- [ ] 理解Envoy的线程模型和TLS机制
- [ ] 理解Filter Chain的工作原理
- [ ] 理解xDS协议和配置热更新
- [ ] 能实现简化版的代理服务器

### 输出物
1. `envoy_architecture.md` - Envoy架构分析文档
2. `simple_proxy.hpp` - 简化代理实现
3. `filter_example.cpp` - Filter示例
4. `notes/month32_envoy.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| 架构文档阅读 | 25小时 |
| 源码阅读 | 50小时 |
| 线程模型分析 | 25小时 |
| 代理服务器实现 | 30小时 |
| 总结文档 | 10小时 |

---

## 下月预告

Month 33将学习**高性能HTTP服务器实现**，深入HTTP/1.1协议解析。
