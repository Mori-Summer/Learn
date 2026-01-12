# Month 46: 日志系统设计——构建可观测性基础

## 本月主题概述

本月学习高性能日志系统的设计与实现，掌握结构化日志、异步日志、日志聚合等核心概念。学习spdlog的使用和内部实现，并设计一个支持多种输出目标的日志框架。

**学习目标**：
- 理解日志系统的设计原则
- 掌握spdlog等主流日志库的使用
- 学会设计高性能异步日志系统
- 实现结构化日志和日志聚合

---

## 理论学习内容

### 第一周：日志系统基础

**学习目标**：理解日志的作用和设计原则

**阅读材料**：
- [ ] spdlog官方文档
- [ ] 《日志的艺术》相关文章
- [ ] ELK Stack入门

**核心概念**：

```cpp
// ==========================================
// 日志级别
// ==========================================
enum class LogLevel {
    Trace,    // 最详细的跟踪信息
    Debug,    // 调试信息
    Info,     // 一般信息
    Warning,  // 警告
    Error,    // 错误
    Critical, // 严重错误
    Off       // 关闭日志
};

// 级别选择原则：
// - Trace: 函数进入/退出，变量值变化
// - Debug: 调试时有用的信息
// - Info: 程序正常运行的里程碑
// - Warning: 可能的问题，但不影响运行
// - Error: 错误发生，但可以恢复
// - Critical: 严重错误，程序可能无法继续

// ==========================================
// spdlog基本使用
// ==========================================
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

void basic_example() {
    // 默认logger
    spdlog::info("Welcome to spdlog!");
    spdlog::error("Some error message with arg: {}", 1);

    // 格式化支持
    spdlog::warn("Easy padding in numbers like {:08d}", 12);
    spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}", 42);

    // 设置日志级别
    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("This message should be displayed..");

    // 设置格式
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
}

// ==========================================
// 创建自定义logger
// ==========================================
void create_loggers() {
    // 控制台logger
    auto console = spdlog::stdout_color_mt("console");
    console->info("Console logger created");

    // 文件logger
    auto file_logger = spdlog::basic_logger_mt("file", "logs/basic.log");
    file_logger->info("File logger created");

    // 轮转文件logger (5MB per file, 3 files)
    auto rotating = spdlog::rotating_logger_mt(
        "rotating", "logs/rotating.log", 5 * 1024 * 1024, 3);
    rotating->info("Rotating logger created");

    // 每日轮转logger
    auto daily = spdlog::daily_logger_mt("daily", "logs/daily.log", 0, 0);
    daily->info("Daily logger created");
}

// ==========================================
// 多sink logger
// ==========================================
void multi_sink_example() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::warn);
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/multisink.log", 5 * 1024 * 1024, 3);
    file_sink->set_level(spdlog::level::trace);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);

    spdlog::register_logger(logger);

    logger->trace("Trace to file only");
    logger->info("Info to file only");
    logger->warn("Warn to console and file");
    logger->error("Error to console and file");
}
```

### 第二周：异步日志设计

**学习目标**：理解异步日志的实现原理

**阅读材料**：
- [ ] spdlog异步模式源码
- [ ] 无锁队列设计
- [ ] 线程池实现

```cpp
// ==========================================
// spdlog异步模式
// ==========================================
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>

void async_example() {
    // 初始化线程池
    spdlog::init_thread_pool(8192, 1);  // 队列大小8192，1个线程

    // 创建异步logger
    auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>(
        "async_file", "logs/async.log");

    for (int i = 0; i < 100000; ++i) {
        async_file->info("Async message #{}", i);
    }

    // 确保所有日志都被写入
    spdlog::shutdown();
}

// ==========================================
// 自定义异步日志实现
// ==========================================
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

class AsyncLogger {
public:
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string message;
        std::string logger_name;
        std::thread::id thread_id;
    };

    AsyncLogger(size_t queue_size = 8192)
        : running_(true)
        , queue_size_(queue_size)
    {
        worker_ = std::thread(&AsyncLogger::workerLoop, this);
    }

    ~AsyncLogger() {
        running_ = false;
        cv_.notify_one();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void log(LogLevel level, std::string message) {
        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level = level;
        entry.message = std::move(message);
        entry.thread_id = std::this_thread::get_id();

        {
            std::unique_lock<std::mutex> lock(mutex_);

            // 如果队列满了，根据策略处理
            if (queue_.size() >= queue_size_) {
                // 策略1：阻塞等待
                cv_not_full_.wait(lock, [this] {
                    return queue_.size() < queue_size_ || !running_;
                });

                // 策略2：丢弃（注释掉上面，使用下面）
                // dropped_count_++;
                // return;
            }

            queue_.push(std::move(entry));
        }
        cv_.notify_one();
    }

    void addSink(std::function<void(const LogEntry&)> sink) {
        sinks_.push_back(std::move(sink));
    }

    void flush() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_empty_.wait(lock, [this] { return queue_.empty(); });
    }

private:
    void workerLoop() {
        while (running_ || !queue_.empty()) {
            LogEntry entry;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] {
                    return !queue_.empty() || !running_;
                });

                if (queue_.empty()) continue;

                entry = std::move(queue_.front());
                queue_.pop();

                cv_not_full_.notify_one();

                if (queue_.empty()) {
                    cv_empty_.notify_all();
                }
            }

            // 写入所有sink
            for (auto& sink : sinks_) {
                sink(entry);
            }
        }
    }

    std::queue<LogEntry> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable cv_not_full_;
    std::condition_variable cv_empty_;
    std::thread worker_;
    std::atomic<bool> running_;
    size_t queue_size_;
    std::atomic<size_t> dropped_count_{0};
    std::vector<std::function<void(const LogEntry&)>> sinks_;
};
```

### 第三周：结构化日志

**学习目标**：实现JSON格式的结构化日志

**阅读材料**：
- [ ] 结构化日志最佳实践
- [ ] JSON日志格式标准
- [ ] ELK/Loki日志聚合

```cpp
// ==========================================
// 结构化日志
// ==========================================
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

using json = nlohmann::json;

// 自定义JSON sink
template<typename Mutex>
class JsonSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit JsonSink(const std::string& filename)
        : file_(filename, std::ios::app)
    {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        json log_entry;

        // 基础字段
        log_entry["timestamp"] = fmt::format("{:%Y-%m-%dT%H:%M:%S}.{:03d}Z",
            fmt::localtime(std::chrono::system_clock::to_time_t(msg.time)),
            std::chrono::duration_cast<std::chrono::milliseconds>(
                msg.time.time_since_epoch()).count() % 1000
        );
        log_entry["level"] = spdlog::level::to_string_view(msg.level).data();
        log_entry["logger"] = std::string(msg.logger_name.data(), msg.logger_name.size());
        log_entry["thread_id"] = msg.thread_id;
        log_entry["message"] = std::string(msg.payload.data(), msg.payload.size());

        // 如果有源码位置信息
        if (!msg.source.empty()) {
            log_entry["source"]["file"] = msg.source.filename;
            log_entry["source"]["line"] = msg.source.line;
            log_entry["source"]["function"] = msg.source.funcname;
        }

        file_ << log_entry.dump() << "\n";
    }

    void flush_() override {
        file_.flush();
    }

private:
    std::ofstream file_;
};

using JsonSinkMt = JsonSink<std::mutex>;
using JsonSinkSt = JsonSink<spdlog::details::null_mutex>;

// ==========================================
// 带上下文的结构化日志
// ==========================================
class StructuredLogger {
public:
    StructuredLogger(std::shared_ptr<spdlog::logger> logger)
        : logger_(std::move(logger))
    {}

    // 添加全局上下文
    StructuredLogger& with(const std::string& key, const json& value) {
        context_[key] = value;
        return *this;
    }

    // 记录带额外字段的日志
    void info(const std::string& message, const json& extra = {}) {
        log(spdlog::level::info, message, extra);
    }

    void error(const std::string& message, const json& extra = {}) {
        log(spdlog::level::err, message, extra);
    }

    // 创建子logger（继承上下文）
    StructuredLogger child() const {
        StructuredLogger child(logger_);
        child.context_ = context_;
        return child;
    }

private:
    void log(spdlog::level::level_enum level, const std::string& message, const json& extra) {
        json entry = context_;
        entry["message"] = message;

        for (auto& [key, value] : extra.items()) {
            entry[key] = value;
        }

        logger_->log(level, entry.dump());
    }

    std::shared_ptr<spdlog::logger> logger_;
    json context_;
};

// 使用示例
void structured_logging_example() {
    auto json_sink = std::make_shared<JsonSinkMt>("logs/structured.json");
    auto logger = std::make_shared<spdlog::logger>("json", json_sink);

    StructuredLogger slog(logger);

    // 添加全局上下文
    slog.with("service", "user-api")
        .with("version", "1.0.0")
        .with("environment", "production");

    // 记录带额外字段的日志
    slog.info("User login", {
        {"user_id", 12345},
        {"ip", "192.168.1.100"},
        {"duration_ms", 150}
    });

    slog.error("Database connection failed", {
        {"host", "db.example.com"},
        {"port", 5432},
        {"error_code", "CONN_REFUSED"}
    });
}
```

### 第四周：日志系统最佳实践

**学习目标**：设计生产级日志系统

**阅读材料**：
- [ ] 日志采样策略
- [ ] 日志安全（敏感数据脱敏）
- [ ] 分布式追踪集成

```cpp
// ==========================================
// 完整的日志系统设计
// ==========================================
#include <memory>
#include <string>
#include <unordered_map>
#include <regex>

// 日志配置
struct LogConfig {
    std::string level = "info";
    std::string format = "json";  // text, json
    std::string output = "stdout";  // stdout, file, both
    std::string file_path = "logs/app.log";
    size_t max_file_size = 10 * 1024 * 1024;  // 10MB
    int max_files = 5;
    bool async = true;
    size_t async_queue_size = 8192;
    int async_threads = 1;

    // 采样配置
    bool enable_sampling = false;
    double sample_rate = 1.0;  // 1.0 = 100%

    // 敏感数据脱敏
    std::vector<std::string> sensitive_fields = {
        "password", "token", "secret", "credit_card"
    };
};

// 敏感数据脱敏器
class DataMasker {
public:
    explicit DataMasker(const std::vector<std::string>& sensitive_fields) {
        for (const auto& field : sensitive_fields) {
            patterns_.emplace_back(
                fmt::format(R"("{}"\s*:\s*"[^"]*")", field),
                std::regex::icase
            );
            replacements_.push_back(
                fmt::format(R"("{}":" ***REDACTED***")", field)
            );
        }
    }

    std::string mask(const std::string& input) const {
        std::string result = input;
        for (size_t i = 0; i < patterns_.size(); ++i) {
            result = std::regex_replace(result, patterns_[i], replacements_[i]);
        }
        return result;
    }

private:
    std::vector<std::regex> patterns_;
    std::vector<std::string> replacements_;
};

// 采样器
class LogSampler {
public:
    explicit LogSampler(double rate) : rate_(rate), gen_(std::random_device{}()) {}

    bool shouldLog() {
        if (rate_ >= 1.0) return true;
        if (rate_ <= 0.0) return false;
        return dist_(gen_) < rate_;
    }

private:
    double rate_;
    std::mt19937 gen_;
    std::uniform_real_distribution<> dist_{0.0, 1.0};
};

// 日志管理器
class LogManager {
public:
    static LogManager& instance() {
        static LogManager instance;
        return instance;
    }

    void init(const LogConfig& config) {
        config_ = config;
        masker_ = std::make_unique<DataMasker>(config.sensitive_fields);

        if (config.enable_sampling) {
            sampler_ = std::make_unique<LogSampler>(config.sample_rate);
        }

        setupLogger();
    }

    std::shared_ptr<spdlog::logger> getLogger(const std::string& name = "default") {
        auto it = loggers_.find(name);
        if (it != loggers_.end()) {
            return it->second;
        }
        return default_logger_;
    }

    void flush() {
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> l) {
            l->flush();
        });
    }

    void shutdown() {
        flush();
        spdlog::shutdown();
    }

private:
    LogManager() = default;

    void setupLogger() {
        std::vector<spdlog::sink_ptr> sinks;

        // 控制台输出
        if (config_.output == "stdout" || config_.output == "both") {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            sinks.push_back(console_sink);
        }

        // 文件输出
        if (config_.output == "file" || config_.output == "both") {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config_.file_path,
                config_.max_file_size,
                config_.max_files
            );
            sinks.push_back(file_sink);
        }

        // 创建logger
        if (config_.async) {
            spdlog::init_thread_pool(config_.async_queue_size, config_.async_threads);
            default_logger_ = std::make_shared<spdlog::async_logger>(
                "default",
                sinks.begin(),
                sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block
            );
        } else {
            default_logger_ = std::make_shared<spdlog::logger>(
                "default",
                sinks.begin(),
                sinks.end()
            );
        }

        // 设置级别
        auto level = spdlog::level::from_str(config_.level);
        default_logger_->set_level(level);

        // 设置格式
        if (config_.format == "json") {
            default_logger_->set_pattern(R"({"time":"%Y-%m-%dT%H:%M:%S.%e%z","level":"%l","logger":"%n","message":"%v"})");
        } else {
            default_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
        }

        spdlog::register_logger(default_logger_);
        spdlog::set_default_logger(default_logger_);

        loggers_["default"] = default_logger_;
    }

    LogConfig config_;
    std::shared_ptr<spdlog::logger> default_logger_;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
    std::unique_ptr<DataMasker> masker_;
    std::unique_ptr<LogSampler> sampler_;
};

// 便捷宏
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

// 使用示例
int main() {
    LogConfig config;
    config.level = "debug";
    config.format = "json";
    config.output = "both";
    config.file_path = "logs/app.log";
    config.async = true;

    LogManager::instance().init(config);

    LOG_INFO("Application started");
    LOG_DEBUG("Debug message with args: {} {}", 42, "hello");

    // 结构化日志
    LOG_INFO(R"({{"event":"user_login","user_id":{},"ip":"{}"}})", 12345, "192.168.1.1");

    LogManager::instance().shutdown();
    return 0;
}
```

---

## 源码阅读任务

### 本月源码阅读

1. **spdlog源码**
   - 仓库：https://github.com/gabime/spdlog
   - 重点：`include/spdlog/async.h`、`sinks/`目录
   - 学习目标：理解异步日志和sink设计

2. **glog源码**
   - 仓库：https://github.com/google/glog
   - 重点：`src/logging.cc`
   - 学习目标：理解另一种日志设计

3. **Boost.Log源码**
   - 学习目标：理解更复杂的日志框架设计

---

## 实践项目

### 项目：高性能结构化日志系统

**项目结构**：

```
logger/
├── CMakeLists.txt
├── include/
│   └── logger/
│       ├── logger.hpp
│       ├── config.hpp
│       ├── sink.hpp
│       └── formatter.hpp
├── src/
│   ├── logger.cpp
│   └── sinks/
│       ├── console_sink.cpp
│       ├── file_sink.cpp
│       └── network_sink.cpp
└── tests/
    └── test_logger.cpp
```

（详细代码实现参见上文）

---

## 检验标准

- [ ] 能够配置和使用spdlog
- [ ] 理解同步和异步日志的区别
- [ ] 能够实现结构化JSON日志
- [ ] 能够设计多sink日志系统
- [ ] 理解日志采样和数据脱敏
- [ ] 能够进行日志性能优化

### 知识检验问题

1. 异步日志的队列满了应该如何处理？
2. 结构化日志相比文本日志有什么优势？
3. 如何在不影响性能的情况下记录调试日志？
4. 分布式系统中如何关联日志？

---

## 输出物清单

1. **项目代码**
   - `logger/` - 完整的日志系统

2. **配置模板**
   - 各种场景的日志配置

3. **文档**
   - `notes/month46_logging.md`
   - `notes/logging_best_practices.md`

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | 日志系统基础 | 15h | 15h | 5h |
| 第2周 | 异步日志设计 | 12h | 18h | 5h |
| 第3周 | 结构化日志 | 10h | 20h | 5h |
| 第4周 | 最佳实践 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 47将学习**指标收集与监控**，构建应用的可观测性系统。
