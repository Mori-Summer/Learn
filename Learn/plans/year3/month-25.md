# Month 25: Socket编程基础——网络通信的基石

## 本月主题概述

进入第三年的网络编程学习。本月从Socket API开始，掌握TCP/UDP编程的基础知识，理解网络协议栈的工作原理。

---

## 理论学习内容

### 第一周：网络基础回顾

**学习目标**：理解OSI模型和TCP/IP协议栈

**阅读材料**：
- [ ] 《UNIX网络编程》卷1 第1-3章
- [ ] 《TCP/IP详解》卷1 相关章节

**核心概念**：
- OSI七层模型与TCP/IP四层模型
- IP地址与端口
- TCP三次握手与四次挥手
- UDP的特点与适用场景

### 第二周：TCP Socket编程

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// 服务器端
int server_fd = socket(AF_INET, SOCK_STREAM, 0);

sockaddr_in addr{};
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = INADDR_ANY;
addr.sin_port = htons(8080);

bind(server_fd, (sockaddr*)&addr, sizeof(addr));
listen(server_fd, SOMAXCONN);

int client_fd = accept(server_fd, nullptr, nullptr);
char buffer[1024];
ssize_t n = read(client_fd, buffer, sizeof(buffer));
write(client_fd, buffer, n);
close(client_fd);

// 客户端
int sock = socket(AF_INET, SOCK_STREAM, 0);
sockaddr_in server_addr{};
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(8080);
inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

connect(sock, (sockaddr*)&server_addr, sizeof(server_addr));
write(sock, "Hello", 5);
char buf[1024];
read(sock, buf, sizeof(buf));
close(sock);
```

### 第三周：UDP Socket编程

```cpp
// UDP服务器
int sock = socket(AF_INET, SOCK_DGRAM, 0);
sockaddr_in addr{};
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = INADDR_ANY;
addr.sin_port = htons(8080);

bind(sock, (sockaddr*)&addr, sizeof(addr));

char buffer[1024];
sockaddr_in client_addr;
socklen_t client_len = sizeof(client_addr);
ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0,
                     (sockaddr*)&client_addr, &client_len);
sendto(sock, buffer, n, 0, (sockaddr*)&client_addr, client_len);

// UDP客户端
int sock = socket(AF_INET, SOCK_DGRAM, 0);
sockaddr_in server_addr{};
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(8080);
inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

sendto(sock, "Hello", 5, 0, (sockaddr*)&server_addr, sizeof(server_addr));
recvfrom(sock, buffer, sizeof(buffer), 0, nullptr, nullptr);
```

### 第四周：Socket选项与错误处理

```cpp
// 常用socket选项
int opt = 1;
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

// TCP选项
setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));  // 禁用Nagle

// 超时设置
timeval tv{5, 0};  // 5秒
setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

// 错误处理
if (connect(sock, ...) < 0) {
    switch (errno) {
        case ECONNREFUSED: // 连接被拒绝
        case ETIMEDOUT:    // 连接超时
        case ENETUNREACH:  // 网络不可达
    }
}
```

---

## 实践项目

### 项目：跨平台Socket封装

```cpp
// socket.hpp
#pragma once
#include <string>
#include <optional>
#include <vector>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
using socket_t = int;
constexpr socket_t INVALID_SOCKET = -1;
#endif

class SocketError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Socket {
protected:
    socket_t fd_ = INVALID_SOCKET;

    Socket(socket_t fd) : fd_(fd) {}

public:
    Socket() = default;
    ~Socket() { close(); }

    Socket(Socket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = INVALID_SOCKET;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = INVALID_SOCKET;
        }
        return *this;
    }

    void close() {
        if (fd_ != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(fd_);
#else
            ::close(fd_);
#endif
            fd_ = INVALID_SOCKET;
        }
    }

    bool valid() const { return fd_ != INVALID_SOCKET; }
    socket_t native() const { return fd_; }

    void set_nonblocking(bool nonblock) {
#ifdef _WIN32
        u_long mode = nonblock ? 1 : 0;
        ioctlsocket(fd_, FIONBIO, &mode);
#else
        int flags = fcntl(fd_, F_GETFL, 0);
        if (nonblock)
            fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        else
            fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
#endif
    }
};

class TcpSocket : public Socket {
public:
    static TcpSocket create() {
        socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == INVALID_SOCKET)
            throw SocketError("Failed to create socket");
        return TcpSocket(fd);
    }

    void connect(const std::string& host, int port) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (::connect(fd_, (sockaddr*)&addr, sizeof(addr)) < 0)
            throw SocketError("Connect failed");
    }

    void bind(const std::string& host, int port) {
        int opt = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (host.empty() || host == "0.0.0.0")
            addr.sin_addr.s_addr = INADDR_ANY;
        else
            inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (::bind(fd_, (sockaddr*)&addr, sizeof(addr)) < 0)
            throw SocketError("Bind failed");
    }

    void listen(int backlog = SOMAXCONN) {
        if (::listen(fd_, backlog) < 0)
            throw SocketError("Listen failed");
    }

    std::optional<TcpSocket> accept() {
        socket_t client = ::accept(fd_, nullptr, nullptr);
        if (client == INVALID_SOCKET)
            return std::nullopt;
        return TcpSocket(client);
    }

    ssize_t send(const void* data, size_t len) {
        return ::send(fd_, (const char*)data, len, 0);
    }

    ssize_t recv(void* buffer, size_t len) {
        return ::recv(fd_, (char*)buffer, len, 0);
    }

private:
    TcpSocket(socket_t fd) : Socket(fd) {}
};
```

---

## 检验标准

- [ ] 理解TCP和UDP的区别
- [ ] 能编写基本的TCP/UDP程序
- [ ] 理解常用的socket选项
- [ ] 实现跨平台的Socket封装

### 输出物
1. `socket.hpp`
2. `tcp_echo_server.cpp`
3. `tcp_client.cpp`
4. `notes/month25_socket.md`

---

## 下月预告

Month 26将学习**阻塞与非阻塞I/O**，理解I/O模型的演进。
