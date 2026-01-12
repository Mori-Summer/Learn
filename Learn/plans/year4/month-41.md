# Month 41: Docker容器化——C++应用的现代部署方式

## 本月主题概述

本月学习Docker容器化技术，掌握如何将C++应用打包成轻量级、可移植的容器。学习Dockerfile编写、多阶段构建、容器编排，以及如何优化C++应用的容器镜像。

**学习目标**：
- 理解Docker的核心概念和架构
- 掌握C++应用的容器化最佳实践
- 学会多阶段构建优化镜像大小
- 了解Docker Compose和基础容器编排

---

## 理论学习内容

### 第一周：Docker基础概念

**学习目标**：理解容器化技术的原理

**阅读材料**：
- [ ] Docker官方文档：Get Started
- [ ] 《Docker实战》第1-4章
- [ ] Container vs Virtual Machine对比

**核心概念**：

```bash
# ==========================================
# Docker核心概念
# ==========================================

# Image（镜像）: 只读模板，包含运行应用所需的一切
# Container（容器）: 镜像的运行实例
# Dockerfile: 构建镜像的脚本
# Registry: 镜像仓库（Docker Hub, GHCR, etc.）
# Volume: 持久化数据存储
# Network: 容器间通信

# ==========================================
# 基本命令
# ==========================================

# 镜像操作
docker images                    # 列出镜像
docker pull ubuntu:22.04        # 拉取镜像
docker build -t myapp:1.0 .     # 构建镜像
docker push myrepo/myapp:1.0    # 推送镜像
docker rmi myapp:1.0            # 删除镜像

# 容器操作
docker run -it ubuntu:22.04 bash    # 交互式运行
docker run -d --name app myapp:1.0  # 后台运行
docker ps                            # 列出运行中容器
docker ps -a                         # 列出所有容器
docker logs app                      # 查看日志
docker exec -it app bash             # 进入容器
docker stop app                      # 停止容器
docker rm app                        # 删除容器

# 资源清理
docker system prune                  # 清理未使用资源
docker builder prune                 # 清理构建缓存
```

**基本Dockerfile**：

```dockerfile
# ==========================================
# 最简单的C++应用Dockerfile
# ==========================================
FROM ubuntu:22.04

# 安装编译工具
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /app

# 复制源码
COPY . .

# 编译
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build

# 运行
CMD ["./build/myapp"]
```

### 第二周：多阶段构建

**学习目标**：优化C++应用的Docker镜像

**阅读材料**：
- [ ] Docker文档：Multi-stage builds
- [ ] 《Docker实战》第7章
- [ ] Distroless容器镜像

```dockerfile
# ==========================================
# 多阶段构建 - 基础版本
# ==========================================
# 构建阶段
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

# 运行阶段
FROM ubuntu:22.04 AS runtime

# 只安装运行时依赖
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# 创建非root用户
RUN useradd -m -s /bin/bash appuser
USER appuser

WORKDIR /app
COPY --from=builder /src/build/myapp .

ENTRYPOINT ["./myapp"]
```

```dockerfile
# ==========================================
# 多阶段构建 - 使用vcpkg
# ==========================================
FROM ubuntu:22.04 AS vcpkg-builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# 安装vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# 预安装依赖（利用缓存）
WORKDIR /src
COPY vcpkg.json .
RUN vcpkg install --triplet x64-linux

# 构建应用
FROM vcpkg-builder AS app-builder

COPY . .
RUN cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    && cmake --build build --parallel

# 最小运行时镜像
FROM gcr.io/distroless/cc-debian12 AS runtime

COPY --from=app-builder /src/build/myapp /app/myapp
COPY --from=app-builder /src/build/lib/*.so* /lib/

WORKDIR /app
ENTRYPOINT ["/app/myapp"]
```

```dockerfile
# ==========================================
# 多阶段构建 - 静态链接
# ==========================================
FROM alpine:3.18 AS builder

RUN apk add --no-cache \
    build-base \
    cmake \
    linux-headers \
    git

WORKDIR /src
COPY . .

# 静态链接
RUN cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXE_LINKER_FLAGS="-static" \
    -DCMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++" \
    && cmake --build build

# 最小镜像 - scratch
FROM scratch

COPY --from=builder /src/build/myapp /myapp

ENTRYPOINT ["/myapp"]
```

### 第三周：Docker Compose与服务编排

**学习目标**：使用Docker Compose管理多容器应用

**阅读材料**：
- [ ] Docker Compose文档
- [ ] 《Docker实战》第9-10章
- [ ] Compose Specification

```yaml
# ==========================================
# docker-compose.yml - 完整示例
# ==========================================
version: '3.9'

services:
  # C++应用服务
  app:
    build:
      context: .
      dockerfile: Dockerfile
      target: runtime
      args:
        BUILD_TYPE: Release
    image: myapp:latest
    container_name: myapp
    restart: unless-stopped
    ports:
      - "8080:8080"
    environment:
      - LOG_LEVEL=info
      - DB_HOST=postgres
      - REDIS_HOST=redis
    volumes:
      - app-data:/app/data
      - ./config:/app/config:ro
    depends_on:
      postgres:
        condition: service_healthy
      redis:
        condition: service_started
    networks:
      - backend
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 40s
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 1G
        reservations:
          cpus: '0.5'
          memory: 256M

  # PostgreSQL数据库
  postgres:
    image: postgres:15-alpine
    container_name: postgres
    restart: unless-stopped
    environment:
      POSTGRES_USER: ${DB_USER:-app}
      POSTGRES_PASSWORD: ${DB_PASSWORD:-secret}
      POSTGRES_DB: ${DB_NAME:-appdb}
    volumes:
      - postgres-data:/var/lib/postgresql/data
      - ./init.sql:/docker-entrypoint-initdb.d/init.sql:ro
    networks:
      - backend
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ${DB_USER:-app}"]
      interval: 10s
      timeout: 5s
      retries: 5

  # Redis缓存
  redis:
    image: redis:7-alpine
    container_name: redis
    restart: unless-stopped
    command: redis-server --appendonly yes
    volumes:
      - redis-data:/data
    networks:
      - backend

  # Nginx反向代理
  nginx:
    image: nginx:alpine
    container_name: nginx
    restart: unless-stopped
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
      - ./certs:/etc/nginx/certs:ro
    depends_on:
      - app
    networks:
      - frontend
      - backend

networks:
  frontend:
    driver: bridge
  backend:
    driver: bridge
    internal: true

volumes:
  app-data:
  postgres-data:
  redis-data:
```

```yaml
# ==========================================
# docker-compose.override.yml - 开发环境覆盖
# ==========================================
version: '3.9'

services:
  app:
    build:
      target: builder  # 使用构建阶段作为开发环境
    volumes:
      - .:/src  # 挂载源码
      - build-cache:/src/build  # 缓存构建目录
    environment:
      - LOG_LEVEL=debug
    command: ["./build/myapp", "--dev"]

  # 开发工具
  dev-tools:
    image: ubuntu:22.04
    volumes:
      - .:/src
    working_dir: /src
    command: sleep infinity

volumes:
  build-cache:
```

```bash
# ==========================================
# Docker Compose常用命令
# ==========================================

# 启动服务
docker-compose up -d

# 查看状态
docker-compose ps

# 查看日志
docker-compose logs -f app

# 重新构建
docker-compose build --no-cache

# 停止并删除
docker-compose down

# 带数据卷删除
docker-compose down -v

# 扩展服务实例
docker-compose up -d --scale app=3

# 执行命令
docker-compose exec app bash

# 重启单个服务
docker-compose restart app
```

### 第四周：生产环境最佳实践

**学习目标**：掌握生产级Docker部署技巧

**阅读材料**：
- [ ] Docker安全最佳实践
- [ ] 容器镜像优化指南
- [ ] Kubernetes基础概念

```dockerfile
# ==========================================
# 生产级Dockerfile
# ==========================================
# syntax=docker/dockerfile:1.4

# 基础构建镜像
FROM ubuntu:22.04 AS base

# 安装CA证书和时区数据
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    tzdata \
    && rm -rf /var/lib/apt/lists/*

# 构建阶段
FROM base AS builder

# 安装构建依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# 安装vcpkg
ARG VCPKG_COMMIT=a34c873a9717a888f58dc05268dea15592c2f0ff
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && cd /opt/vcpkg \
    && git checkout ${VCPKG_COMMIT} \
    && ./bootstrap-vcpkg.sh -disableMetrics

ENV VCPKG_ROOT=/opt/vcpkg

# 缓存vcpkg依赖
WORKDIR /src
COPY vcpkg.json vcpkg-configuration.json ./
RUN --mount=type=cache,target=/root/.cache/vcpkg \
    ${VCPKG_ROOT}/vcpkg install --triplet x64-linux

# 复制源码并构建
COPY . .

ARG BUILD_TYPE=Release
ARG BUILD_VERSION=dev

RUN --mount=type=cache,target=/src/build \
    cmake -B build -S . -G Ninja \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DVERSION_STRING=${BUILD_VERSION} \
    && cmake --build build --parallel \
    && cmake --install build --prefix /opt/app

# 运行时镜像
FROM base AS runtime

# 安全设置
RUN groupadd -r appgroup && useradd -r -g appgroup appuser

# 复制应用
COPY --from=builder /opt/app /opt/app

# 复制运行时依赖库（如果需要）
# COPY --from=builder /src/build/lib/*.so* /usr/local/lib/
# RUN ldconfig

WORKDIR /opt/app
USER appuser

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD ["/opt/app/bin/healthcheck"]

# 暴露端口
EXPOSE 8080

# 入口点
ENTRYPOINT ["/opt/app/bin/myapp"]
CMD ["--config", "/opt/app/config/default.yaml"]

# 元数据标签
LABEL org.opencontainers.image.title="MyApp" \
      org.opencontainers.image.description="My C++ Application" \
      org.opencontainers.image.version="${BUILD_VERSION}" \
      org.opencontainers.image.source="https://github.com/user/myapp"
```

```dockerfile
# ==========================================
# .dockerignore
# ==========================================
# 版本控制
.git
.gitignore
.gitattributes

# 构建目录
build/
cmake-build-*/
out/

# IDE配置
.idea/
.vscode/
*.swp
*.swo

# 文档
docs/
*.md
LICENSE

# 测试
tests/
*_test.cpp

# CI/CD配置
.github/
.gitlab-ci.yml
Jenkinsfile

# Docker相关（避免递归）
Dockerfile*
docker-compose*
.docker/

# 其他
*.log
*.tmp
.env.local
```

**安全扫描和优化**：

```bash
# ==========================================
# 镜像安全扫描
# ==========================================

# 使用Trivy扫描漏洞
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
    aquasec/trivy image myapp:latest

# 使用Dive分析镜像层
docker run --rm -it \
    -v /var/run/docker.sock:/var/run/docker.sock \
    wagoodman/dive myapp:latest

# 检查镜像大小
docker images myapp

# 查看镜像历史
docker history myapp:latest

# ==========================================
# 镜像优化技巧
# ==========================================

# 1. 使用小基础镜像
# ubuntu:22.04    ~77MB
# debian:slim     ~80MB
# alpine          ~5MB
# distroless      ~2MB
# scratch         0MB

# 2. 合并RUN指令
# 不好的做法
RUN apt-get update
RUN apt-get install -y package1
RUN apt-get install -y package2

# 好的做法
RUN apt-get update && apt-get install -y \
    package1 \
    package2 \
    && rm -rf /var/lib/apt/lists/*

# 3. 使用.dockerignore减少上下文

# 4. 利用构建缓存
# 将变化少的层放在前面

# 5. 使用多阶段构建
# 只复制必要文件到最终镜像
```

---

## 源码阅读任务

### 本月源码阅读

1. **官方示例项目**
   - docker-library/official-images
   - GoogleContainerTools/distroless

2. **C++项目Docker化示例**
   - envoyproxy/envoy 的Dockerfile
   - grpc/grpc 的Docker配置

3. **构建工具**
   - BuildKit源码理解
   - docker/buildx

---

## 实践项目

### 项目：容器化的微服务应用

创建一个容器化的C++微服务应用，包含完整的Docker配置。

**项目结构**：

```
microservice-demo/
├── docker/
│   ├── Dockerfile
│   ├── Dockerfile.dev
│   └── docker-compose.yml
├── src/
│   ├── main.cpp
│   ├── server.hpp
│   ├── server.cpp
│   ├── handler.hpp
│   └── handler.cpp
├── include/
│   └── microservice/
│       └── api.hpp
├── tests/
│   └── test_server.cpp
├── config/
│   ├── default.yaml
│   └── production.yaml
├── scripts/
│   ├── build.sh
│   └── healthcheck.sh
├── CMakeLists.txt
├── vcpkg.json
└── README.md
```

**docker/Dockerfile**：

```dockerfile
# syntax=docker/dockerfile:1.4

###################
# 基础镜像
###################
FROM ubuntu:22.04 AS base

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

###################
# 构建环境
###################
FROM base AS build-env

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# vcpkg
ARG VCPKG_COMMIT
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && cd ${VCPKG_ROOT} \
    && git checkout ${VCPKG_COMMIT:-HEAD} \
    && ./bootstrap-vcpkg.sh -disableMetrics

###################
# 依赖缓存
###################
FROM build-env AS deps

WORKDIR /src
COPY vcpkg.json vcpkg-configuration.json ./

RUN --mount=type=cache,target=/root/.cache/vcpkg \
    ${VCPKG_ROOT}/vcpkg install --triplet x64-linux

###################
# 构建应用
###################
FROM deps AS builder

COPY . .

ARG BUILD_TYPE=Release
ARG VERSION=dev

RUN cmake -B build -S . -G Ninja \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DAPP_VERSION=${VERSION}

RUN cmake --build build --parallel $(nproc)

RUN cmake --install build --prefix /opt/microservice

###################
# 测试
###################
FROM builder AS tester

RUN ctest --test-dir build --output-on-failure

###################
# 生产镜像
###################
FROM base AS production

# 安全配置
RUN groupadd -r microservice && useradd -r -g microservice service
RUN mkdir -p /opt/microservice/data && chown -R service:microservice /opt/microservice

# 复制构建产物
COPY --from=builder --chown=service:microservice /opt/microservice /opt/microservice

# 复制配置
COPY --chown=service:microservice config/production.yaml /opt/microservice/config/config.yaml

WORKDIR /opt/microservice
USER service

EXPOSE 8080 9090

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD ["/opt/microservice/bin/healthcheck"]

ENTRYPOINT ["/opt/microservice/bin/microservice"]
CMD ["--config", "/opt/microservice/config/config.yaml"]

###################
# 开发镜像
###################
FROM build-env AS development

# 安装开发工具
RUN apt-get update && apt-get install -y --no-install-recommends \
    gdb \
    valgrind \
    clang-format \
    clang-tidy \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# 复制vcpkg依赖
COPY --from=deps ${VCPKG_ROOT} ${VCPKG_ROOT}
COPY --from=deps /src/vcpkg_installed /src/vcpkg_installed

CMD ["bash"]
```

**src/server.hpp**：

```cpp
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>

namespace microservice {

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t http_port = 8080;
    uint16_t grpc_port = 9090;
    size_t thread_count = 4;
    std::string log_level = "info";
};

class Server {
public:
    explicit Server(ServerConfig config);
    ~Server();

    // 禁止拷贝
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // 启动服务
    void start();

    // 停止服务
    void stop();

    // 等待停止
    void wait();

    // 检查是否运行中
    bool is_running() const;

    // 健康检查
    struct HealthStatus {
        bool healthy;
        std::string version;
        int64_t uptime_seconds;
        size_t active_connections;
    };
    HealthStatus health_check() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace microservice
```

**src/server.cpp**：

```cpp
#include "server.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <thread>
#include <chrono>

namespace microservice {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

class Server::Impl : public std::enable_shared_from_this<Server::Impl> {
public:
    explicit Impl(ServerConfig config)
        : config_(std::move(config))
        , ioc_(config_.thread_count)
        , acceptor_(ioc_)
        , start_time_(std::chrono::steady_clock::now())
    {}

    void start() {
        // 设置日志级别
        spdlog::set_level(spdlog::level::from_str(config_.log_level));

        // 设置acceptor
        tcp::endpoint endpoint(
            asio::ip::make_address(config_.host),
            config_.http_port
        );

        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        spdlog::info("Server starting on {}:{}", config_.host, config_.http_port);

        // 开始接受连接
        do_accept();

        // 启动工作线程
        running_ = true;
        for (size_t i = 0; i < config_.thread_count; ++i) {
            threads_.emplace_back([this] {
                ioc_.run();
            });
        }
    }

    void stop() {
        if (!running_.exchange(false)) return;

        spdlog::info("Server stopping...");

        acceptor_.close();
        ioc_.stop();

        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
        threads_.clear();

        spdlog::info("Server stopped");
    }

    void wait() {
        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }

    bool is_running() const {
        return running_.load();
    }

    HealthStatus health_check() const {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time_).count();

        return {
            .healthy = running_.load(),
            .version = "1.0.0",
            .uptime_seconds = uptime,
            .active_connections = active_connections_.load()
        };
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            asio::make_strand(ioc_),
            [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    self->active_connections_++;
                    self->handle_connection(std::move(socket));
                }
                if (self->running_) {
                    self->do_accept();
                }
            }
        );
    }

    void handle_connection(tcp::socket socket) {
        auto buffer = std::make_shared<beast::flat_buffer>();
        auto request = std::make_shared<http::request<http::string_body>>();

        http::async_read(socket, *buffer, *request,
            [this, socket = std::move(socket), buffer, request]
            (beast::error_code ec, std::size_t) mutable {
                if (ec) {
                    active_connections_--;
                    return;
                }

                handle_request(*request, socket);
                active_connections_--;
            }
        );
    }

    void handle_request(const http::request<http::string_body>& req,
                        tcp::socket& socket) {
        http::response<http::string_body> res;
        res.version(req.version());
        res.keep_alive(req.keep_alive());

        if (req.target() == "/health") {
            auto status = health_check();
            json j = {
                {"healthy", status.healthy},
                {"version", status.version},
                {"uptime_seconds", status.uptime_seconds},
                {"active_connections", status.active_connections}
            };

            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = j.dump();
        } else if (req.target() == "/api/v1/data") {
            if (req.method() == http::verb::get) {
                json j = {{"message", "Hello from microservice!"}};
                res.result(http::status::ok);
                res.set(http::field::content_type, "application/json");
                res.body() = j.dump();
            } else {
                res.result(http::status::method_not_allowed);
            }
        } else {
            res.result(http::status::not_found);
            res.body() = "Not Found";
        }

        res.prepare_payload();
        http::write(socket, res);
    }

    ServerConfig config_;
    asio::io_context ioc_;
    tcp::acceptor acceptor_;
    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
    std::atomic<size_t> active_connections_{0};
    std::chrono::steady_clock::time_point start_time_;
};

Server::Server(ServerConfig config)
    : impl_(std::make_shared<Impl>(std::move(config))) {}

Server::~Server() {
    stop();
}

void Server::start() { impl_->start(); }
void Server::stop() { impl_->stop(); }
void Server::wait() { impl_->wait(); }
bool Server::is_running() const { return impl_->is_running(); }
Server::HealthStatus Server::health_check() const { return impl_->health_check(); }

} // namespace microservice
```

**src/main.cpp**：

```cpp
#include "server.hpp"

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <csignal>
#include <iostream>
#include <filesystem>

namespace {
    std::unique_ptr<microservice::Server> g_server;

    void signal_handler(int signal) {
        spdlog::info("Received signal {}, shutting down...", signal);
        if (g_server) {
            g_server->stop();
        }
    }
}

microservice::ServerConfig load_config(const std::string& path) {
    microservice::ServerConfig config;

    if (!std::filesystem::exists(path)) {
        spdlog::warn("Config file not found: {}, using defaults", path);
        return config;
    }

    try {
        YAML::Node yaml = YAML::LoadFile(path);

        if (yaml["server"]) {
            auto server = yaml["server"];
            if (server["host"]) config.host = server["host"].as<std::string>();
            if (server["http_port"]) config.http_port = server["http_port"].as<uint16_t>();
            if (server["grpc_port"]) config.grpc_port = server["grpc_port"].as<uint16_t>();
            if (server["thread_count"]) config.thread_count = server["thread_count"].as<size_t>();
        }

        if (yaml["logging"]) {
            auto logging = yaml["logging"];
            if (logging["level"]) config.log_level = logging["level"].as<std::string>();
        }
    } catch (const YAML::Exception& e) {
        spdlog::error("Failed to parse config: {}", e.what());
        throw;
    }

    return config;
}

int main(int argc, char* argv[]) {
    std::string config_path = "/opt/microservice/config/config.yaml";

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [--config <path>]\n";
            return 0;
        }
    }

    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        auto config = load_config(config_path);

        spdlog::info("Starting microservice...");
        spdlog::info("  Host: {}", config.host);
        spdlog::info("  HTTP Port: {}", config.http_port);
        spdlog::info("  Threads: {}", config.thread_count);

        g_server = std::make_unique<microservice::Server>(config);
        g_server->start();
        g_server->wait();

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
```

**scripts/healthcheck.sh**：

```bash
#!/bin/bash
set -e

# 健康检查脚本
curl -sf http://localhost:8080/health | jq -e '.healthy == true' > /dev/null
```

**config/production.yaml**：

```yaml
server:
  host: "0.0.0.0"
  http_port: 8080
  grpc_port: 9090
  thread_count: 4

logging:
  level: "info"

database:
  host: "${DB_HOST:-localhost}"
  port: ${DB_PORT:-5432}
  name: "${DB_NAME:-microservice}"

redis:
  host: "${REDIS_HOST:-localhost}"
  port: ${REDIS_PORT:-6379}
```

---

## 检验标准

- [ ] 理解Docker的核心概念
- [ ] 能够编写多阶段构建的Dockerfile
- [ ] 能够优化镜像大小和安全性
- [ ] 能够使用Docker Compose编排多容器应用
- [ ] 能够实现健康检查和优雅关闭
- [ ] 理解容器安全最佳实践

### 知识检验问题

1. 多阶段构建的优势是什么？
2. 如何减小Docker镜像的大小？
3. ENTRYPOINT和CMD的区别是什么？
4. 如何在容器中处理信号？

---

## 输出物清单

1. **Docker配置**
   - `Dockerfile` - 生产级多阶段构建
   - `docker-compose.yml` - 完整服务编排

2. **项目代码**
   - `microservice-demo/` - 完整的容器化微服务

3. **文档**
   - `notes/month41_docker.md` - 学习笔记
   - `docs/DOCKER_GUIDE.md` - Docker使用指南

4. **脚本**
   - `scripts/build.sh` - 构建脚本
   - `scripts/healthcheck.sh` - 健康检查

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | Docker基础概念 | 15h | 15h | 5h |
| 第2周 | 多阶段构建 | 12h | 18h | 5h |
| 第3周 | Docker Compose | 10h | 20h | 5h |
| 第4周 | 生产最佳实践 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 42将学习**单元测试（Google Test/Catch2）**，掌握C++项目的测试驱动开发和测试框架使用。
