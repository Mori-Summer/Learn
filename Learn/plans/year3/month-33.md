# Month 33: 高性能HTTP服务器实现

## 本月主题概述

HTTP是互联网最重要的协议之一。本月深入学习HTTP/1.1协议解析，实现一个完整的高性能HTTP服务器，包括连接管理、Keep-Alive、静态文件服务等功能。

---

## 理论学习内容

### 第一周：HTTP/1.1协议回顾

**学习目标**：深入理解HTTP协议细节

**阅读材料**：
- [ ] RFC 7230-7235 (HTTP/1.1)
- [ ] 《HTTP权威指南》相关章节

**核心概念**：

```
HTTP请求格式：
┌─────────────────────────────────────────┐
│ GET /path HTTP/1.1\r\n                  │ <- 请求行
│ Host: example.com\r\n                   │ <- 头部
│ Content-Length: 13\r\n                  │
│ Connection: keep-alive\r\n              │
│ \r\n                                    │ <- 空行
│ {"key":"val"}                           │ <- 请求体(可选)
└─────────────────────────────────────────┘

HTTP响应格式：
┌─────────────────────────────────────────┐
│ HTTP/1.1 200 OK\r\n                     │ <- 状态行
│ Content-Type: text/html\r\n             │ <- 头部
│ Content-Length: 13\r\n                  │
│ \r\n                                    │ <- 空行
│ <html>...</html>                        │ <- 响应体
└─────────────────────────────────────────┘

关键头部：
- Content-Length: 正文长度
- Transfer-Encoding: chunked 分块传输
- Connection: keep-alive/close
- Content-Type: MIME类型
```

### 第二周：HTTP解析器

```cpp
// http_parser.hpp
#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>

enum class HttpMethod {
    GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH
};

struct HttpRequest {
    HttpMethod method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    std::string get_header(const std::string& key) const {
        auto it = headers.find(key);
        return it != headers.end() ? it->second : "";
    }

    bool keep_alive() const {
        auto conn = get_header("Connection");
        if (version == "HTTP/1.1") {
            return conn != "close";
        }
        return conn == "keep-alive";
    }
};

enum class ParseState {
    REQUEST_LINE,
    HEADERS,
    BODY,
    COMPLETE,
    ERROR
};

class HttpParser {
public:
    ParseState parse(const char* data, size_t len) {
        buffer_.append(data, len);

        while (true) {
            switch (state_) {
                case ParseState::REQUEST_LINE:
                    if (!parse_request_line()) return state_;
                    break;
                case ParseState::HEADERS:
                    if (!parse_headers()) return state_;
                    break;
                case ParseState::BODY:
                    if (!parse_body()) return state_;
                    break;
                case ParseState::COMPLETE:
                case ParseState::ERROR:
                    return state_;
            }
        }
    }

    const HttpRequest& request() const { return request_; }

    void reset() {
        state_ = ParseState::REQUEST_LINE;
        buffer_.clear();
        request_ = HttpRequest{};
        content_length_ = 0;
    }

private:
    bool parse_request_line() {
        size_t pos = buffer_.find("\r\n");
        if (pos == std::string::npos) return false;

        std::string line = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 2);

        // 解析: METHOD PATH VERSION
        size_t sp1 = line.find(' ');
        size_t sp2 = line.find(' ', sp1 + 1);

        if (sp1 == std::string::npos || sp2 == std::string::npos) {
            state_ = ParseState::ERROR;
            return false;
        }

        std::string method = line.substr(0, sp1);
        request_.path = line.substr(sp1 + 1, sp2 - sp1 - 1);
        request_.version = line.substr(sp2 + 1);

        if (method == "GET") request_.method = HttpMethod::GET;
        else if (method == "POST") request_.method = HttpMethod::POST;
        else if (method == "PUT") request_.method = HttpMethod::PUT;
        else if (method == "DELETE") request_.method = HttpMethod::DELETE;
        else if (method == "HEAD") request_.method = HttpMethod::HEAD;
        else {
            state_ = ParseState::ERROR;
            return false;
        }

        state_ = ParseState::HEADERS;
        return true;
    }

    bool parse_headers() {
        while (true) {
            size_t pos = buffer_.find("\r\n");
            if (pos == std::string::npos) return false;

            if (pos == 0) {
                // 空行，头部结束
                buffer_.erase(0, 2);

                auto cl = request_.get_header("Content-Length");
                if (!cl.empty()) {
                    content_length_ = std::stoul(cl);
                }

                if (content_length_ > 0) {
                    state_ = ParseState::BODY;
                } else {
                    state_ = ParseState::COMPLETE;
                }
                return true;
            }

            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);

            size_t colon = line.find(':');
            if (colon == std::string::npos) {
                state_ = ParseState::ERROR;
                return false;
            }

            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // 去除前导空格
            while (!value.empty() && value[0] == ' ') {
                value.erase(0, 1);
            }

            request_.headers[key] = value;
        }
    }

    bool parse_body() {
        if (buffer_.size() >= content_length_) {
            request_.body = buffer_.substr(0, content_length_);
            buffer_.erase(0, content_length_);
            state_ = ParseState::COMPLETE;
            return true;
        }
        return false;
    }

private:
    ParseState state_ = ParseState::REQUEST_LINE;
    std::string buffer_;
    HttpRequest request_;
    size_t content_length_ = 0;
};
```

### 第三周：HTTP响应构建

```cpp
// http_response.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <sstream>

class HttpResponse {
public:
    HttpResponse(int status = 200) : status_(status) {
        reason_ = get_reason(status);
    }

    void set_status(int status) {
        status_ = status;
        reason_ = get_reason(status);
    }

    void set_header(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    void set_body(const std::string& body) {
        body_ = body;
        headers_["Content-Length"] = std::to_string(body.size());
    }

    void set_body(std::string&& body) {
        body_ = std::move(body);
        headers_["Content-Length"] = std::to_string(body_.size());
    }

    std::string to_string() const {
        std::ostringstream oss;

        // 状态行
        oss << "HTTP/1.1 " << status_ << " " << reason_ << "\r\n";

        // 头部
        for (const auto& [key, value] : headers_) {
            oss << key << ": " << value << "\r\n";
        }
        oss << "\r\n";

        // 响应体
        oss << body_;

        return oss.str();
    }

    // 常用响应
    static HttpResponse ok(const std::string& body,
                          const std::string& content_type = "text/plain") {
        HttpResponse resp(200);
        resp.set_header("Content-Type", content_type);
        resp.set_body(body);
        return resp;
    }

    static HttpResponse not_found() {
        HttpResponse resp(404);
        resp.set_header("Content-Type", "text/html");
        resp.set_body("<html><body><h1>404 Not Found</h1></body></html>");
        return resp;
    }

    static HttpResponse server_error(const std::string& msg = "") {
        HttpResponse resp(500);
        resp.set_header("Content-Type", "text/html");
        resp.set_body("<html><body><h1>500 Internal Server Error</h1></body></html>");
        return resp;
    }

private:
    static std::string get_reason(int status) {
        switch (status) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            default: return "Unknown";
        }
    }

private:
    int status_;
    std::string reason_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};
```

### 第四周：完整HTTP服务器

```cpp
// http_server.hpp
#pragma once
#include "http_parser.hpp"
#include "http_response.hpp"
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <functional>
#include <unordered_map>
#include <memory>

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpConnection {
public:
    HttpConnection(int fd) : fd_(fd) {}

    int fd() const { return fd_; }
    HttpParser& parser() { return parser_; }

    bool should_close() const { return should_close_; }
    void set_should_close(bool v) { should_close_ = v; }

private:
    int fd_;
    HttpParser parser_;
    bool should_close_ = false;
};

class HttpServer {
public:
    HttpServer(int port, const std::string& doc_root = "")
        : port_(port), doc_root_(doc_root) {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }

    ~HttpServer() {
        if (listen_fd_ >= 0) close(listen_fd_);
        if (epfd_ >= 0) close(epfd_);
    }

    // 注册路由
    void route(const std::string& path, RequestHandler handler) {
        routes_[path] = std::move(handler);
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

        add_fd(listen_fd_);
        return true;
    }

    void run() {
        running_ = true;
        std::vector<epoll_event> events(1024);

        while (running_) {
            int n = epoll_wait(epfd_, events.data(), events.size(), 1000);

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;

                if (fd == listen_fd_) {
                    handle_accept();
                } else {
                    auto it = connections_.find(fd);
                    if (it == connections_.end()) continue;

                    if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                        close_connection(fd);
                    } else if (events[i].events & EPOLLIN) {
                        handle_read(it->second);
                    }
                }
            }
        }
    }

    void stop() { running_ = false; }

private:
    void add_fd(int fd) {
        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    void close_connection(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        connections_.erase(fd);
    }

    void handle_accept() {
        while (true) {
            int client = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK);
            if (client < 0) break;

            connections_[client] = std::make_unique<HttpConnection>(client);
            add_fd(client);
        }
    }

    void handle_read(std::unique_ptr<HttpConnection>& conn) {
        char buf[4096];

        while (true) {
            ssize_t n = read(conn->fd(), buf, sizeof(buf));

            if (n > 0) {
                auto state = conn->parser().parse(buf, n);

                if (state == ParseState::COMPLETE) {
                    handle_request(conn);

                    if (conn->should_close()) {
                        close_connection(conn->fd());
                        return;
                    }

                    conn->parser().reset();
                } else if (state == ParseState::ERROR) {
                    close_connection(conn->fd());
                    return;
                }
            } else if (n == 0) {
                close_connection(conn->fd());
                return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                close_connection(conn->fd());
                return;
            }
        }
    }

    void handle_request(std::unique_ptr<HttpConnection>& conn) {
        const auto& req = conn->parser().request();
        HttpResponse resp;

        // 检查路由
        auto it = routes_.find(req.path);
        if (it != routes_.end()) {
            resp = it->second(req);
        } else if (!doc_root_.empty()) {
            // 静态文件服务
            resp = serve_file(req.path);
        } else {
            resp = HttpResponse::not_found();
        }

        // 设置Connection头
        if (!req.keep_alive()) {
            resp.set_header("Connection", "close");
            conn->set_should_close(true);
        } else {
            resp.set_header("Connection", "keep-alive");
        }

        // 发送响应
        std::string response = resp.to_string();
        send_all(conn->fd(), response.data(), response.size());
    }

    HttpResponse serve_file(const std::string& path) {
        std::string file_path = doc_root_ + path;
        if (path == "/") file_path += "index.html";

        struct stat st;
        if (stat(file_path.c_str(), &st) < 0 || S_ISDIR(st.st_mode)) {
            return HttpResponse::not_found();
        }

        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd < 0) return HttpResponse::not_found();

        std::string content(st.st_size, '\0');
        read(fd, content.data(), st.st_size);
        close(fd);

        HttpResponse resp(200);
        resp.set_header("Content-Type", get_mime_type(file_path));
        resp.set_body(std::move(content));
        return resp;
    }

    std::string get_mime_type(const std::string& path) {
        if (path.ends_with(".html")) return "text/html";
        if (path.ends_with(".css")) return "text/css";
        if (path.ends_with(".js")) return "application/javascript";
        if (path.ends_with(".json")) return "application/json";
        if (path.ends_with(".png")) return "image/png";
        if (path.ends_with(".jpg")) return "image/jpeg";
        return "application/octet-stream";
    }

    void send_all(int fd, const char* data, size_t len) {
        size_t sent = 0;
        while (sent < len) {
            ssize_t n = write(fd, data + sent, len - sent);
            if (n > 0) sent += n;
            else if (errno == EINTR) continue;
            else break;
        }
    }

private:
    int port_;
    std::string doc_root_;
    int epfd_ = -1;
    int listen_fd_ = -1;
    bool running_ = false;
    std::unordered_map<int, std::unique_ptr<HttpConnection>> connections_;
    std::unordered_map<std::string, RequestHandler> routes_;
};
```

---

## 检验标准

- [ ] 理解HTTP/1.1协议格式
- [ ] 实现增量式HTTP解析器
- [ ] 实现HTTP响应构建器
- [ ] 实现Keep-Alive连接复用
- [ ] 实现静态文件服务

### 输出物
1. `http_parser.hpp` - HTTP请求解析器
2. `http_response.hpp` - HTTP响应构建器
3. `http_server.hpp` - 完整HTTP服务器
4. `benchmark.cpp` - 性能测试
5. `notes/month33_http.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| HTTP协议学习 | 20小时 |
| 解析器实现 | 35小时 |
| 响应构建器 | 20小时 |
| 服务器实现 | 45小时 |
| 测试与优化 | 20小时 |

---

## 下月预告

Month 34将学习**RPC框架基础**，实现远程过程调用。
