# Month 34: RPC框架基础——远程调用

## 本月主题概述

RPC（远程过程调用）是分布式系统的核心技术。本月学习RPC的基本概念，实现一个简单但完整的RPC框架，包括服务注册、序列化、网络传输等核心组件。

---

## 理论学习内容

### 第一周：RPC概念

**学习目标**：理解RPC的工作原理

**阅读材料**：
- [ ] 《分布式系统原理与范型》RPC章节
- [ ] gRPC官方文档

**核心概念**：

```
RPC调用流程：

Client                                     Server
┌─────────┐                               ┌─────────┐
│ 应用    │ call add(1, 2)                │ 应用    │
└────┬────┘                               └────▲────┘
     │                                         │
┌────▼────┐                               ┌────┴────┐
│ Stub    │ 序列化参数                    │ Skeleton│ 反序列化参数
└────┬────┘                               └────▲────┘
     │                                         │
┌────▼────┐                               ┌────┴────┐
│ 网络层  │ ─────────────────────────>    │ 网络层  │
└─────────┘        发送请求               └─────────┘

RPC核心组件：
1. IDL (Interface Definition Language) - 接口定义
2. Stub/Proxy - 客户端代理
3. Skeleton - 服务端骨架
4. 序列化 - 参数/返回值编码
5. 网络传输 - TCP/UDP/HTTP
6. 服务发现 - 定位服务地址
```

### 第二周：序列化协议

```cpp
// serialization.hpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <type_traits>

class Buffer {
public:
    // 写入基本类型
    template<typename T>
    std::enable_if_t<std::is_arithmetic_v<T>>
    write(T value) {
        size_t pos = data_.size();
        data_.resize(pos + sizeof(T));
        std::memcpy(data_.data() + pos, &value, sizeof(T));
    }

    // 写入字符串
    void write(const std::string& str) {
        write<uint32_t>(str.size());
        data_.insert(data_.end(), str.begin(), str.end());
    }

    // 读取基本类型
    template<typename T>
    std::enable_if_t<std::is_arithmetic_v<T>, T>
    read() {
        T value;
        std::memcpy(&value, data_.data() + read_pos_, sizeof(T));
        read_pos_ += sizeof(T);
        return value;
    }

    // 读取字符串
    std::string read_string() {
        uint32_t len = read<uint32_t>();
        std::string str(data_.data() + read_pos_, len);
        read_pos_ += len;
        return str;
    }

    const char* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    void clear() { data_.clear(); read_pos_ = 0; }

    void set_data(const char* data, size_t len) {
        data_.assign(data, data + len);
        read_pos_ = 0;
    }

private:
    std::vector<char> data_;
    size_t read_pos_ = 0;
};

// RPC消息格式
struct RpcMessage {
    uint32_t msg_id;       // 消息ID
    uint8_t msg_type;      // 请求/响应
    std::string method;    // 方法名
    Buffer payload;        // 参数/返回值

    enum Type : uint8_t {
        REQUEST = 1,
        RESPONSE = 2
    };

    void serialize(Buffer& buf) const {
        buf.write(msg_id);
        buf.write(msg_type);
        buf.write(method);
        buf.write<uint32_t>(payload.size());
        // 追加payload数据
    }

    void deserialize(Buffer& buf) {
        msg_id = buf.read<uint32_t>();
        msg_type = buf.read<uint8_t>();
        method = buf.read_string();
        uint32_t payload_len = buf.read<uint32_t>();
        // 读取payload数据
    }
};
```

### 第三周：RPC客户端

```cpp
// rpc_client.hpp
#pragma once
#include "serialization.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <future>

class RpcClient {
public:
    RpcClient(const std::string& host, int port)
        : host_(host), port_(port) {}

    ~RpcClient() {
        disconnect();
    }

    bool connect() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

        if (::connect(fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd_);
            fd_ = -1;
            return false;
        }

        return true;
    }

    void disconnect() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    // 同步调用
    template<typename R, typename... Args>
    R call(const std::string& method, Args... args) {
        Buffer request;
        serialize_args(request, args...);

        RpcMessage msg;
        msg.msg_id = next_id_++;
        msg.msg_type = RpcMessage::REQUEST;
        msg.method = method;
        msg.payload = std::move(request);

        // 发送请求
        send_message(msg);

        // 接收响应
        RpcMessage response = recv_message();

        // 反序列化返回值
        return response.payload.read<R>();
    }

    // 异步调用
    template<typename R, typename... Args>
    std::future<R> async_call(const std::string& method, Args... args) {
        return std::async(std::launch::async, [=] {
            return call<R>(method, args...);
        });
    }

private:
    template<typename T>
    void serialize_args(Buffer& buf, T arg) {
        buf.write(arg);
    }

    template<typename T, typename... Rest>
    void serialize_args(Buffer& buf, T first, Rest... rest) {
        buf.write(first);
        serialize_args(buf, rest...);
    }

    void serialize_args(Buffer& buf) {}  // 终止条件

    void send_message(const RpcMessage& msg) {
        Buffer buf;
        msg.serialize(buf);

        // 发送长度前缀
        uint32_t len = buf.size();
        send(fd_, &len, sizeof(len), 0);
        send(fd_, buf.data(), buf.size(), 0);
    }

    RpcMessage recv_message() {
        // 读取长度
        uint32_t len;
        recv(fd_, &len, sizeof(len), MSG_WAITALL);

        // 读取消息体
        std::vector<char> data(len);
        recv(fd_, data.data(), len, MSG_WAITALL);

        Buffer buf;
        buf.set_data(data.data(), data.size());

        RpcMessage msg;
        msg.deserialize(buf);
        return msg;
    }

private:
    std::string host_;
    int port_;
    int fd_ = -1;
    std::atomic<uint32_t> next_id_{0};
};
```

### 第四周：RPC服务端

```cpp
// rpc_server.hpp
#pragma once
#include "serialization.hpp"
#include <sys/epoll.h>
#include <netinet/in.h>
#include <functional>
#include <unordered_map>
#include <memory>

class RpcServer {
public:
    using Handler = std::function<Buffer(Buffer&)>;

    RpcServer(int port) : port_(port) {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }

    ~RpcServer() {
        if (listen_fd_ >= 0) close(listen_fd_);
        if (epfd_ >= 0) close(epfd_);
    }

    // 注册服务方法
    template<typename R, typename... Args, typename F>
    void register_method(const std::string& name, F func) {
        handlers_[name] = [func](Buffer& args) -> Buffer {
            // 反序列化参数并调用
            auto result = invoke_with_args<R, Args...>(func, args);

            // 序列化返回值
            Buffer response;
            response.write(result);
            return response;
        };
    }

    bool start() {
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

        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listen_fd_;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, listen_fd_, &ev);

        return true;
    }

    void run() {
        running_ = true;
        std::vector<epoll_event> events(256);

        while (running_) {
            int n = epoll_wait(epfd_, events.data(), events.size(), 1000);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;

                if (fd == listen_fd_) {
                    handle_accept();
                } else {
                    handle_request(fd);
                }
            }
        }
    }

    void stop() { running_ = false; }

private:
    void handle_accept() {
        while (true) {
            int client = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK);
            if (client < 0) break;

            epoll_event ev;
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = client;
            epoll_ctl(epfd_, EPOLL_CTL_ADD, client, &ev);
        }
    }

    void handle_request(int fd) {
        // 读取请求长度
        uint32_t len;
        ssize_t n = recv(fd, &len, sizeof(len), MSG_PEEK);
        if (n <= 0) {
            close_client(fd);
            return;
        }

        recv(fd, &len, sizeof(len), 0);

        // 读取请求体
        std::vector<char> data(len);
        recv(fd, data.data(), len, MSG_WAITALL);

        Buffer buf;
        buf.set_data(data.data(), data.size());

        RpcMessage request;
        request.deserialize(buf);

        // 查找并执行处理器
        RpcMessage response;
        response.msg_id = request.msg_id;
        response.msg_type = RpcMessage::RESPONSE;
        response.method = request.method;

        auto it = handlers_.find(request.method);
        if (it != handlers_.end()) {
            response.payload = it->second(request.payload);
        }

        // 发送响应
        Buffer resp_buf;
        response.serialize(resp_buf);

        uint32_t resp_len = resp_buf.size();
        send(fd, &resp_len, sizeof(resp_len), 0);
        send(fd, resp_buf.data(), resp_buf.size(), 0);
    }

    void close_client(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
    }

    template<typename R, typename... Args, typename F>
    static R invoke_with_args(F func, Buffer& buf) {
        return invoke_impl<R, F, Args...>(func, buf, std::index_sequence_for<Args...>{});
    }

    template<typename R, typename F, typename... Args, size_t... Is>
    static R invoke_impl(F func, Buffer& buf, std::index_sequence<Is...>) {
        std::tuple<Args...> args{buf.read<Args>()...};
        return func(std::get<Is>(args)...);
    }

private:
    int port_;
    int epfd_ = -1;
    int listen_fd_ = -1;
    bool running_ = false;
    std::unordered_map<std::string, Handler> handlers_;
};

// 使用示例
/*
// 服务端
RpcServer server(8080);
server.register_method<int, int, int>("add", [](int a, int b) {
    return a + b;
});
server.start();
server.run();

// 客户端
RpcClient client("127.0.0.1", 8080);
client.connect();
int result = client.call<int>("add", 1, 2);  // result = 3
*/
```

---

## 检验标准

- [ ] 理解RPC的核心概念
- [ ] 实现二进制序列化协议
- [ ] 实现RPC客户端Stub
- [ ] 实现RPC服务端Skeleton
- [ ] 理解服务发现的概念

### 输出物
1. `serialization.hpp` - 序列化库
2. `rpc_client.hpp` - RPC客户端
3. `rpc_server.hpp` - RPC服务端
4. `example_service.cpp` - 示例服务
5. `notes/month34_rpc.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| RPC概念学习 | 20小时 |
| 序列化协议实现 | 30小时 |
| 客户端实现 | 35小时 |
| 服务端实现 | 40小时 |
| 测试与优化 | 15小时 |

---

## 下月预告

Month 35将学习**协议设计与序列化**，包括Protobuf和FlatBuffers。
