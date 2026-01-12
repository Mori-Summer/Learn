# Month 48: 第四年总结与综合项目——现代C++工程化实践

## 本月主题概述

本月是第四年的总结月，将整合全年学习的现代工程化知识，构建一个完整的、生产级的C++微服务项目。项目将包含完整的构建系统、测试框架、CI/CD流水线、容器化部署、日志和监控。

**学习目标**：
- 综合运用全年学习的工程化技能
- 构建一个完整的生产级C++项目
- 掌握项目从零到部署的全流程
- 为第五年的高级主题做准备

---

## 第四年知识回顾

### 知识体系总结

```
第四年：现代工程化与独立开发者全栈能力
├── 构建系统（Month 37-39）
│   ├── Modern CMake深度使用
│   ├── vcpkg包管理器
│   └── Conan包管理器
│
├── 持续集成与部署（Month 40-41）
│   ├── GitHub Actions CI/CD
│   └── Docker容器化
│
├── 代码质量（Month 42-44）
│   ├── 单元测试（Google Test/Catch2）
│   ├── Clang-Tidy静态分析
│   └── Sanitizers运行时检测
│
└── 可观测性（Month 45-47）
    ├── 性能分析与Profiling
    ├── 日志系统设计
    └── 指标收集与监控
```

### 技能检验清单

**构建系统**：
- [ ] 能够编写Modern CMake配置
- [ ] 能够使用vcpkg/Conan管理依赖
- [ ] 理解跨平台构建的最佳实践

**CI/CD**：
- [ ] 能够配置GitHub Actions工作流
- [ ] 能够实现多平台自动化构建
- [ ] 能够配置自动化测试和发布

**容器化**：
- [ ] 能够编写优化的Dockerfile
- [ ] 能够使用Docker Compose编排服务
- [ ] 理解容器安全最佳实践

**测试**：
- [ ] 能够编写单元测试和集成测试
- [ ] 能够使用Mock进行依赖隔离
- [ ] 能够实现测试覆盖率分析

**代码质量**：
- [ ] 能够配置Clang-Tidy检查
- [ ] 能够使用Sanitizers检测问题
- [ ] 能够进行性能分析和优化

**可观测性**：
- [ ] 能够设计结构化日志系统
- [ ] 能够实现Prometheus指标暴露
- [ ] 能够配置监控和告警

---

## 综合项目：高性能任务调度服务

### 项目概述

构建一个分布式任务调度服务，支持：
- RESTful API接口
- 任务队列管理
- 定时任务调度
- 分布式锁
- 完整的可观测性

### 项目架构

```
task-scheduler/
├── .github/
│   └── workflows/
│       ├── ci.yml
│       ├── release.yml
│       └── docker.yml
├── cmake/
│   ├── CompilerWarnings.cmake
│   ├── Sanitizers.cmake
│   └── StaticAnalyzers.cmake
├── docker/
│   ├── Dockerfile
│   ├── Dockerfile.dev
│   └── docker-compose.yml
├── include/
│   └── scheduler/
│       ├── api/
│       │   ├── server.hpp
│       │   ├── routes.hpp
│       │   └── middleware.hpp
│       ├── core/
│       │   ├── scheduler.hpp
│       │   ├── task.hpp
│       │   ├── queue.hpp
│       │   └── executor.hpp
│       ├── storage/
│       │   ├── repository.hpp
│       │   └── redis_client.hpp
│       ├── observability/
│       │   ├── logger.hpp
│       │   ├── metrics.hpp
│       │   └── tracing.hpp
│       └── config/
│           └── config.hpp
├── src/
│   ├── api/
│   ├── core/
│   ├── storage/
│   ├── observability/
│   └── main.cpp
├── tests/
│   ├── unit/
│   ├── integration/
│   └── e2e/
├── config/
│   ├── default.yaml
│   └── production.yaml
├── scripts/
│   ├── build.sh
│   ├── test.sh
│   └── deploy.sh
├── docs/
│   ├── API.md
│   └── DEPLOYMENT.md
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── .clang-tidy
├── .clang-format
└── README.md
```

### 核心代码实现

**CMakeLists.txt**：

```cmake
cmake_minimum_required(VERSION 3.20)

project(TaskScheduler
    VERSION 1.0.0
    DESCRIPTION "A high-performance distributed task scheduler"
    LANGUAGES CXX
)

# 防止in-source构建
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed")
endif()

# C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 选项
option(BUILD_TESTS "Build tests" ON)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# 模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 包含自定义模块
include(CompilerWarnings)
include(Sanitizers)
include(StaticAnalyzers)

# 查找依赖
find_package(Boost REQUIRED COMPONENTS system)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# 主库
add_library(scheduler_lib
    src/core/scheduler.cpp
    src/core/task.cpp
    src/core/queue.cpp
    src/core/executor.cpp
    src/api/server.cpp
    src/api/routes.cpp
    src/storage/redis_client.cpp
    src/observability/logger.cpp
    src/observability/metrics.cpp
    src/config/config.cpp
)

target_include_directories(scheduler_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(scheduler_lib
    PUBLIC
        Boost::system
        fmt::fmt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        yaml-cpp
        prometheus-cpp::pull
        hiredis::hiredis
        OpenSSL::SSL
        OpenSSL::Crypto
)

# 应用编译器警告
set_project_warnings(scheduler_lib)

# 启用Sanitizers
if(ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN)
    enable_sanitizers(scheduler_lib)
endif()

# 主可执行文件
add_executable(scheduler src/main.cpp)
target_link_libraries(scheduler PRIVATE scheduler_lib)

# 测试
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 安装
include(GNUInstallDirs)

install(TARGETS scheduler scheduler_lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY config/
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/scheduler
)
```

**vcpkg.json**：

```json
{
  "name": "task-scheduler",
  "version": "1.0.0",
  "description": "A high-performance distributed task scheduler",
  "dependencies": [
    "boost-system",
    "boost-asio",
    "boost-beast",
    "fmt",
    "spdlog",
    "nlohmann-json",
    "yaml-cpp",
    "prometheus-cpp",
    "hiredis",
    "openssl",
    {
      "name": "gtest",
      "version>=": "1.14.0"
    },
    {
      "name": "catch2",
      "version>=": "3.0.0"
    }
  ],
  "builtin-baseline": "a34c873a9717a888f58dc05268dea15592c2f0ff"
}
```

**include/scheduler/core/task.hpp**：

```cpp
#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace scheduler {

enum class TaskStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
    Retrying
};

enum class TaskPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

struct TaskConfig {
    int max_retries = 3;
    std::chrono::seconds retry_delay{60};
    std::chrono::seconds timeout{300};
    TaskPriority priority = TaskPriority::Normal;
};

class Task {
public:
    using Id = std::string;
    using Timestamp = std::chrono::system_clock::time_point;
    using Payload = nlohmann::json;

    Task(const std::string& type, Payload payload, TaskConfig config = {});

    // Getters
    const Id& id() const { return id_; }
    const std::string& type() const { return type_; }
    const Payload& payload() const { return payload_; }
    TaskStatus status() const { return status_; }
    TaskPriority priority() const { return config_.priority; }
    int retryCount() const { return retry_count_; }
    Timestamp createdAt() const { return created_at_; }
    std::optional<Timestamp> startedAt() const { return started_at_; }
    std::optional<Timestamp> completedAt() const { return completed_at_; }
    std::optional<std::string> error() const { return error_; }
    std::optional<Payload> result() const { return result_; }

    // State transitions
    void start();
    void complete(Payload result);
    void fail(const std::string& error);
    void retry();
    void cancel();

    // Serialization
    nlohmann::json toJson() const;
    static Task fromJson(const nlohmann::json& json);

    // Comparator for priority queue
    bool operator<(const Task& other) const {
        return config_.priority < other.config_.priority;
    }

private:
    Id id_;
    std::string type_;
    Payload payload_;
    TaskStatus status_ = TaskStatus::Pending;
    TaskConfig config_;
    int retry_count_ = 0;

    Timestamp created_at_;
    std::optional<Timestamp> started_at_;
    std::optional<Timestamp> completed_at_;
    std::optional<std::string> error_;
    std::optional<Payload> result_;

    static Id generateId();
};

// Task Handler类型
using TaskHandler = std::function<nlohmann::json(const Task&)>;

} // namespace scheduler
```

**include/scheduler/core/scheduler.hpp**：

```cpp
#pragma once

#include "scheduler/core/task.hpp"
#include "scheduler/core/queue.hpp"
#include "scheduler/core/executor.hpp"
#include "scheduler/storage/repository.hpp"
#include "scheduler/observability/logger.hpp"
#include "scheduler/observability/metrics.hpp"

#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace scheduler {

struct SchedulerConfig {
    size_t worker_threads = 4;
    size_t queue_capacity = 10000;
    std::chrono::seconds polling_interval{1};
    bool enable_persistence = true;
};

class Scheduler {
public:
    explicit Scheduler(SchedulerConfig config);
    ~Scheduler();

    // 禁用拷贝
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // 生命周期
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // 任务管理
    Task::Id submit(Task task);
    Task::Id schedule(Task task, std::chrono::system_clock::time_point when);
    Task::Id scheduleCron(Task task, const std::string& cron_expr);

    std::optional<Task> getTask(const Task::Id& id) const;
    std::vector<Task> listTasks(TaskStatus status, size_t limit = 100) const;
    bool cancelTask(const Task::Id& id);

    // 处理器注册
    void registerHandler(const std::string& task_type, TaskHandler handler);

    // 统计
    struct Stats {
        size_t pending_tasks;
        size_t running_tasks;
        size_t completed_tasks;
        size_t failed_tasks;
        double avg_processing_time_ms;
        size_t workers_active;
    };
    Stats getStats() const;

private:
    void workerLoop();
    void schedulerLoop();
    void processTask(Task& task);

    SchedulerConfig config_;
    std::atomic<bool> running_{false};

    std::unique_ptr<TaskQueue> queue_;
    std::unique_ptr<Executor> executor_;
    std::shared_ptr<TaskRepository> repository_;

    std::unordered_map<std::string, TaskHandler> handlers_;
    mutable std::mutex handlers_mutex_;

    std::vector<std::thread> workers_;
    std::thread scheduler_thread_;

    // 指标
    std::shared_ptr<Metrics> metrics_;
};

} // namespace scheduler
```

**include/scheduler/api/server.hpp**：

```cpp
#pragma once

#include "scheduler/core/scheduler.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <string>

namespace scheduler::api {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    size_t threads = 4;
    std::chrono::seconds request_timeout{30};
};

class HttpServer {
public:
    HttpServer(ServerConfig config, std::shared_ptr<Scheduler> scheduler);
    ~HttpServer();

    void start();
    void stop();

private:
    void acceptConnection();
    void handleRequest(tcp::socket socket);

    http::response<http::string_body> handleRoute(
        const http::request<http::string_body>& req);

    // API路由
    http::response<http::string_body> handleHealth(
        const http::request<http::string_body>& req);
    http::response<http::string_body> handleSubmitTask(
        const http::request<http::string_body>& req);
    http::response<http::string_body> handleGetTask(
        const http::request<http::string_body>& req, const std::string& id);
    http::response<http::string_body> handleListTasks(
        const http::request<http::string_body>& req);
    http::response<http::string_body> handleCancelTask(
        const http::request<http::string_body>& req, const std::string& id);
    http::response<http::string_body> handleStats(
        const http::request<http::string_body>& req);
    http::response<http::string_body> handleMetrics(
        const http::request<http::string_body>& req);

    ServerConfig config_;
    std::shared_ptr<Scheduler> scheduler_;
    net::io_context ioc_;
    tcp::acceptor acceptor_;
    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
};

} // namespace scheduler::api
```

**include/scheduler/observability/metrics.hpp**：

```cpp
#pragma once

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <memory>

namespace scheduler {

class Metrics {
public:
    static std::shared_ptr<Metrics> create();

    // 任务指标
    void taskSubmitted(const std::string& type);
    void taskStarted(const std::string& type);
    void taskCompleted(const std::string& type, double duration_seconds);
    void taskFailed(const std::string& type, const std::string& error);
    void taskRetried(const std::string& type);

    // 队列指标
    void updateQueueSize(size_t size);
    void updateActiveWorkers(size_t count);

    // HTTP指标
    void httpRequest(const std::string& method, const std::string& path,
                     int status, double duration_seconds);

    // 获取registry（用于暴露指标）
    std::shared_ptr<prometheus::Registry> registry() const { return registry_; }

private:
    Metrics();

    std::shared_ptr<prometheus::Registry> registry_;

    // 任务指标
    prometheus::Family<prometheus::Counter>& tasks_total_;
    prometheus::Family<prometheus::Counter>& tasks_failed_total_;
    prometheus::Family<prometheus::Histogram>& task_duration_seconds_;

    // 队列指标
    prometheus::Family<prometheus::Gauge>& queue_size_;
    prometheus::Family<prometheus::Gauge>& active_workers_;

    // HTTP指标
    prometheus::Family<prometheus::Counter>& http_requests_total_;
    prometheus::Family<prometheus::Histogram>& http_request_duration_seconds_;
};

} // namespace scheduler
```

**src/main.cpp**：

```cpp
#include "scheduler/core/scheduler.hpp"
#include "scheduler/api/server.hpp"
#include "scheduler/observability/logger.hpp"
#include "scheduler/observability/metrics.hpp"
#include "scheduler/config/config.hpp"

#include <prometheus/exposer.h>
#include <csignal>
#include <iostream>

namespace {
    std::atomic<bool> g_running{true};

    void signalHandler(int signal) {
        LOG_INFO("Received signal {}, shutting down...", signal);
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    try {
        // 解析配置
        std::string config_path = "config/default.yaml";
        if (argc > 1) {
            config_path = argv[1];
        }

        auto config = scheduler::Config::load(config_path);

        // 初始化日志
        scheduler::Logger::init(config.logging);

        LOG_INFO("Starting Task Scheduler v{}", "1.0.0");

        // 设置信号处理
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        // 创建指标收集器
        auto metrics = scheduler::Metrics::create();

        // 创建调度器
        auto scheduler = std::make_shared<scheduler::Scheduler>(config.scheduler);

        // 注册示例任务处理器
        scheduler->registerHandler("email", [](const scheduler::Task& task) {
            LOG_INFO("Processing email task: {}", task.id());
            // 模拟发送邮件
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return nlohmann::json{{"sent", true}};
        });

        scheduler->registerHandler("report", [](const scheduler::Task& task) {
            LOG_INFO("Generating report: {}", task.id());
            // 模拟生成报告
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return nlohmann::json{{"report_id", "RPT-12345"}};
        });

        // 启动调度器
        scheduler->start();

        // 创建HTTP服务器
        scheduler::api::HttpServer http_server(config.server, scheduler);
        http_server.start();

        // 启动Prometheus指标暴露
        prometheus::Exposer exposer{fmt::format("{}:{}", config.metrics.host, config.metrics.port)};
        exposer.RegisterCollectable(metrics->registry());

        LOG_INFO("Server started on {}:{}", config.server.host, config.server.port);
        LOG_INFO("Metrics available at {}:{}/metrics", config.metrics.host, config.metrics.port);

        // 主循环
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // 定期打印统计
            auto stats = scheduler->getStats();
            LOG_DEBUG("Stats: pending={}, running={}, completed={}, failed={}",
                      stats.pending_tasks, stats.running_tasks,
                      stats.completed_tasks, stats.failed_tasks);
        }

        // 优雅关闭
        LOG_INFO("Shutting down...");
        http_server.stop();
        scheduler->stop();

        LOG_INFO("Shutdown complete");
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
```

**.github/workflows/ci.yml**：

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

env:
  VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        config:
          - name: "Ubuntu GCC"
            os: ubuntu-22.04
            compiler: gcc
          - name: "Ubuntu Clang"
            os: ubuntu-22.04
            compiler: clang
          - name: "macOS"
            os: macos-13
            compiler: clang

    runs-on: ${{ matrix.config.os }}
    name: Build (${{ matrix.config.name }})

    steps:
      - uses: actions/checkout@v4

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11

      - name: Configure
        run: |
          cmake --preset ci-${{ matrix.config.compiler }}

      - name: Build
        run: cmake --build build --parallel

      - name: Test
        run: ctest --test-dir build --output-on-failure

  lint:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4

      - name: Run clang-format
        uses: jidiber/clang-format-action@v4

      - name: Run clang-tidy
        run: |
          cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
          clang-tidy -p build src/*.cpp

  sanitizers:
    runs-on: ubuntu-22.04
    strategy:
      matrix:
        sanitizer: [asan, tsan, ubsan]
    steps:
      - uses: actions/checkout@v4
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
      - name: Build with ${{ matrix.sanitizer }}
        run: |
          cmake --preset ci-${{ matrix.sanitizer }}
          cmake --build build
          ctest --test-dir build --output-on-failure

  docker:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - name: Build Docker image
        run: docker build -t task-scheduler:test .
      - name: Test Docker image
        run: docker run --rm task-scheduler:test --help
```

**docker/Dockerfile**：

```dockerfile
# syntax=docker/dockerfile:1.4

# 构建阶段
FROM ubuntu:22.04 AS builder

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

# 依赖缓存
WORKDIR /src
COPY vcpkg.json vcpkg-configuration.json ./
RUN --mount=type=cache,target=/root/.cache/vcpkg \
    ${VCPKG_ROOT}/vcpkg install --triplet x64-linux

# 构建
COPY . .
RUN cmake -B build -S . -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DBUILD_TESTS=OFF
RUN cmake --build build
RUN cmake --install build --prefix /opt/scheduler

# 运行时
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r scheduler && useradd -r -g scheduler scheduler

COPY --from=builder /opt/scheduler /opt/scheduler

WORKDIR /opt/scheduler
USER scheduler

EXPOSE 8080 9090

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

ENTRYPOINT ["/opt/scheduler/bin/scheduler"]
CMD ["--config", "/opt/scheduler/etc/scheduler/production.yaml"]
```

---

## 检验标准

### 项目完成度检查

- [ ] 项目能够成功构建（Windows/Linux/macOS）
- [ ] 所有单元测试通过
- [ ] 集成测试覆盖主要功能
- [ ] CI/CD流水线正常运行
- [ ] Docker镜像能够正确构建和运行
- [ ] 日志系统正常工作
- [ ] 指标能够正确暴露
- [ ] API文档完整

### 代码质量检查

- [ ] clang-format格式检查通过
- [ ] clang-tidy静态分析无严重问题
- [ ] ASan/TSan/UBSan测试通过
- [ ] 代码覆盖率>80%
- [ ] 无内存泄漏

### 性能指标

- [ ] 能够处理1000+ QPS
- [ ] P99延迟<100ms
- [ ] 内存使用稳定

---

## 第四年学习总结

### 核心收获

1. **工程化思维**：学会了从零开始构建专业级C++项目
2. **自动化**：掌握了CI/CD、自动化测试、自动化部署
3. **可观测性**：理解了日志、指标、追踪的重要性
4. **代码质量**：掌握了静态分析、动态分析等工具

### 能力提升

- 独立搭建完整的C++项目基础设施
- 实现跨平台的自动化构建和测试
- 设计和实现生产级的日志和监控系统
- 进行系统化的性能分析和优化

### 下一步方向

第五年将进入高级主题：
- 分布式系统设计
- 高性能计算
- 系统编程深入
- 领域特定应用

---

## 输出物清单

1. **完整项目**
   - `task-scheduler/` - 完整的任务调度服务

2. **文档**
   - API文档
   - 部署文档
   - 架构设计文档

3. **总结报告**
   - `notes/year4_summary.md` - 年度学习总结

4. **项目模板**
   - 可复用的C++项目模板

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | 项目规划与架构 | 10h | 20h | 5h |
| 第2周 | 核心功能实现 | 5h | 28h | 2h |
| 第3周 | 可观测性集成 | 5h | 28h | 2h |
| 第4周 | 测试与部署 | 5h | 25h | 5h |
| **合计** | | **25h** | **101h** | **14h** |

---

## 第五年预告

恭喜完成第四年的学习！第五年将进入更高级的主题：

- Month 49-51: 分布式系统设计
- Month 52-54: 高性能计算与SIMD
- Month 55-57: 系统编程深入（内核模块、eBPF）
- Month 58-60: 综合项目与开源贡献
