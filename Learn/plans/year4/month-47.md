# Month 47: 指标收集与监控——构建可观测性系统

## 本月主题概述

本月学习应用指标收集和监控系统的设计与实现。掌握Prometheus指标格式、时序数据库概念、告警规则配置，以及如何在C++应用中集成指标暴露。

**学习目标**：
- 理解可观测性三大支柱（日志、指标、追踪）
- 掌握Prometheus指标类型和格式
- 学会在C++中实现指标收集
- 能够配置Grafana可视化和告警

---

## 理论学习内容

### 第一周：可观测性基础

**学习目标**：理解监控系统的核心概念

**阅读材料**：
- [ ] Prometheus官方文档
- [ ] 《Site Reliability Engineering》监控章节
- [ ] RED/USE方法论

**核心概念**：

```cpp
// ==========================================
// 可观测性三大支柱
// ==========================================

// 1. 日志 (Logs)
//    - 离散事件记录
//    - 适合调试和审计
//    - 高基数数据

// 2. 指标 (Metrics)
//    - 数值型时序数据
//    - 适合监控和告警
//    - 聚合后存储，空间效率高

// 3. 追踪 (Traces)
//    - 请求级别的分布式追踪
//    - 适合性能分析和根因定位
//    - 采样存储

// ==========================================
// Prometheus指标类型
// ==========================================

// 1. Counter（计数器）
//    - 只能增加或重置为0
//    - 用于：请求总数、错误总数、处理的字节数

// 2. Gauge（仪表盘）
//    - 可增可减
//    - 用于：温度、内存使用量、队列长度

// 3. Histogram（直方图）
//    - 将观测值分桶统计
//    - 用于：请求延迟、响应大小
//    - 可计算分位数（服务端）

// 4. Summary（摘要）
//    - 客户端计算分位数
//    - 用于：精确的分位数需求
//    - 不能聚合

// ==========================================
// 指标命名规范
// ==========================================

// 格式: namespace_subsystem_name_unit_suffix
// 例如:
//   http_requests_total          (Counter)
//   http_request_duration_seconds (Histogram)
//   process_memory_bytes         (Gauge)
//   node_cpu_seconds_total       (Counter)

// 命名规则:
// - 使用snake_case
// - 以单位为后缀（seconds, bytes, total等）
// - Counter以_total结尾
// - 使用基础单位（秒而不是毫秒）
```

**Prometheus指标格式**：

```text
# HELP http_requests_total Total number of HTTP requests
# TYPE http_requests_total counter
http_requests_total{method="GET",path="/api/users",status="200"} 12345
http_requests_total{method="POST",path="/api/users",status="201"} 1234
http_requests_total{method="GET",path="/api/users",status="500"} 12

# HELP http_request_duration_seconds HTTP request duration in seconds
# TYPE http_request_duration_seconds histogram
http_request_duration_seconds_bucket{method="GET",path="/api/users",le="0.01"} 8000
http_request_duration_seconds_bucket{method="GET",path="/api/users",le="0.05"} 10000
http_request_duration_seconds_bucket{method="GET",path="/api/users",le="0.1"} 11000
http_request_duration_seconds_bucket{method="GET",path="/api/users",le="0.5"} 12000
http_request_duration_seconds_bucket{method="GET",path="/api/users",le="1.0"} 12300
http_request_duration_seconds_bucket{method="GET",path="/api/users",le="+Inf"} 12345
http_request_duration_seconds_sum{method="GET",path="/api/users"} 456.789
http_request_duration_seconds_count{method="GET",path="/api/users"} 12345

# HELP process_memory_bytes Current memory usage
# TYPE process_memory_bytes gauge
process_memory_bytes{type="resident"} 52428800
process_memory_bytes{type="virtual"} 104857600
```

### 第二周：C++指标库实现

**学习目标**：在C++中实现指标收集

**阅读材料**：
- [ ] prometheus-cpp文档
- [ ] OpenTelemetry C++ SDK
- [ ] 时序数据结构设计

```cpp
// ==========================================
// 简单的指标库实现
// ==========================================
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>

namespace metrics {

// 标签集
using Labels = std::map<std::string, std::string>;

// 标签格式化
std::string formatLabels(const Labels& labels) {
    if (labels.empty()) return "";

    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [key, value] : labels) {
        if (!first) oss << ",";
        oss << key << "=\"" << value << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

// ==========================================
// Counter
// ==========================================
class Counter {
public:
    Counter(const std::string& name, const std::string& help)
        : name_(name), help_(help), value_(0)
    {}

    void inc(double delta = 1.0) {
        // 使用原子操作
        double current = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(
            current,
            current + delta,
            std::memory_order_relaxed
        ));
    }

    double value() const {
        return value_.load(std::memory_order_relaxed);
    }

    std::string collect() const {
        std::ostringstream oss;
        oss << "# HELP " << name_ << " " << help_ << "\n";
        oss << "# TYPE " << name_ << " counter\n";
        oss << name_ << " " << value() << "\n";
        return oss.str();
    }

private:
    std::string name_;
    std::string help_;
    std::atomic<double> value_;
};

// 带标签的Counter
class CounterVec {
public:
    CounterVec(const std::string& name, const std::string& help,
               const std::vector<std::string>& label_names)
        : name_(name), help_(help), label_names_(label_names)
    {}

    void inc(const Labels& labels, double delta = 1.0) {
        std::string key = formatLabels(labels);
        std::lock_guard<std::mutex> lock(mutex_);
        values_[key] += delta;
    }

    std::string collect() const {
        std::ostringstream oss;
        oss << "# HELP " << name_ << " " << help_ << "\n";
        oss << "# TYPE " << name_ << " counter\n";

        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [labels, value] : values_) {
            oss << name_ << labels << " " << value << "\n";
        }
        return oss.str();
    }

private:
    std::string name_;
    std::string help_;
    std::vector<std::string> label_names_;
    mutable std::mutex mutex_;
    std::map<std::string, double> values_;
};

// ==========================================
// Gauge
// ==========================================
class Gauge {
public:
    Gauge(const std::string& name, const std::string& help)
        : name_(name), help_(help), value_(0)
    {}

    void set(double value) {
        value_.store(value, std::memory_order_relaxed);
    }

    void inc(double delta = 1.0) {
        double current = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(
            current,
            current + delta,
            std::memory_order_relaxed
        ));
    }

    void dec(double delta = 1.0) {
        inc(-delta);
    }

    double value() const {
        return value_.load(std::memory_order_relaxed);
    }

    std::string collect() const {
        std::ostringstream oss;
        oss << "# HELP " << name_ << " " << help_ << "\n";
        oss << "# TYPE " << name_ << " gauge\n";
        oss << name_ << " " << value() << "\n";
        return oss.str();
    }

private:
    std::string name_;
    std::string help_;
    std::atomic<double> value_;
};

// ==========================================
// Histogram
// ==========================================
class Histogram {
public:
    Histogram(const std::string& name, const std::string& help,
              const std::vector<double>& buckets)
        : name_(name), help_(help), buckets_(buckets)
    {
        bucket_counts_.resize(buckets.size() + 1, 0);  // +1 for +Inf
    }

    void observe(double value) {
        std::lock_guard<std::mutex> lock(mutex_);

        sum_ += value;
        count_++;

        // 找到对应的桶
        for (size_t i = 0; i < buckets_.size(); ++i) {
            if (value <= buckets_[i]) {
                bucket_counts_[i]++;
            }
        }
        bucket_counts_.back()++;  // +Inf
    }

    std::string collect() const {
        std::ostringstream oss;
        oss << "# HELP " << name_ << " " << help_ << "\n";
        oss << "# TYPE " << name_ << " histogram\n";

        std::lock_guard<std::mutex> lock(mutex_);

        uint64_t cumulative = 0;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            cumulative += bucket_counts_[i];
            oss << name_ << "_bucket{le=\"" << buckets_[i] << "\"} "
                << cumulative << "\n";
        }
        oss << name_ << "_bucket{le=\"+Inf\"} " << count_ << "\n";
        oss << name_ << "_sum " << sum_ << "\n";
        oss << name_ << "_count " << count_ << "\n";

        return oss.str();
    }

private:
    std::string name_;
    std::string help_;
    std::vector<double> buckets_;
    mutable std::mutex mutex_;
    std::vector<uint64_t> bucket_counts_;
    double sum_ = 0;
    uint64_t count_ = 0;
};

// ==========================================
// 指标注册表
// ==========================================
class Registry {
public:
    static Registry& instance() {
        static Registry instance;
        return instance;
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> create(Args&&... args) {
        auto metric = std::make_shared<T>(std::forward<Args>(args)...);
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.push_back(metric);
        return metric;
    }

    std::string collect() const {
        std::ostringstream oss;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& metric : metrics_) {
            // 使用虚函数或variant来收集
        }
        return oss.str();
    }

private:
    Registry() = default;
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<void>> metrics_;
};

// ==========================================
// RAII计时器
// ==========================================
class ScopedTimer {
public:
    ScopedTimer(Histogram& histogram)
        : histogram_(histogram)
        , start_(std::chrono::high_resolution_clock::now())
    {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start_).count();
        histogram_.observe(duration);
    }

private:
    Histogram& histogram_;
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace metrics
```

### 第三周：prometheus-cpp使用

**学习目标**：使用官方库实现指标暴露

**阅读材料**：
- [ ] prometheus-cpp GitHub
- [ ] HTTP端点暴露
- [ ] Push Gateway使用

```cpp
// ==========================================
// 使用prometheus-cpp
// ==========================================
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>

// 创建Registry
auto registry = std::make_shared<prometheus::Registry>();

// 定义指标
auto& http_requests = prometheus::BuildCounter()
    .Name("http_requests_total")
    .Help("Total number of HTTP requests")
    .Labels({{"service", "api"}})
    .Register(*registry);

auto& http_request_duration = prometheus::BuildHistogram()
    .Name("http_request_duration_seconds")
    .Help("HTTP request duration in seconds")
    .Labels({{"service", "api"}})
    .Register(*registry);

auto& active_connections = prometheus::BuildGauge()
    .Name("active_connections")
    .Help("Number of active connections")
    .Labels({{"service", "api"}})
    .Register(*registry);

// HTTP处理示例
void handleRequest(const std::string& method, const std::string& path) {
    // 开始计时
    auto start = std::chrono::high_resolution_clock::now();

    // 增加连接计数
    auto& connections = active_connections.Add({});
    connections.Increment();

    // 处理请求...

    // 记录指标
    auto& counter = http_requests.Add({{"method", method}, {"path", path}});
    counter.Increment();

    // 记录延迟
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();

    auto& histogram = http_request_duration.Add(
        {{"method", method}, {"path", path}},
        prometheus::Histogram::BucketBoundaries{0.001, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0}
    );
    histogram.Observe(duration);

    // 减少连接计数
    connections.Decrement();
}

// 暴露指标端点
int main() {
    // 创建HTTP服务器暴露指标
    prometheus::Exposer exposer{"0.0.0.0:9090"};

    // 注册registry
    exposer.RegisterCollectable(registry);

    // 模拟请求处理
    while (true) {
        handleRequest("GET", "/api/users");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
```

**CMake配置**：

```cmake
find_package(prometheus-cpp CONFIG REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp
    PRIVATE
        prometheus-cpp::pull  # 用于exposer
        # prometheus-cpp::push  # 用于push gateway
)
```

### 第四周：监控系统集成

**学习目标**：配置完整的监控栈

**阅读材料**：
- [ ] Prometheus配置
- [ ] Grafana仪表板设计
- [ ] 告警规则编写

```yaml
# ==========================================
# prometheus.yml - Prometheus配置
# ==========================================
global:
  scrape_interval: 15s
  evaluation_interval: 15s

alerting:
  alertmanagers:
    - static_configs:
        - targets:
          - alertmanager:9093

rule_files:
  - "alerts/*.yml"

scrape_configs:
  # Prometheus自身
  - job_name: 'prometheus'
    static_configs:
      - targets: ['localhost:9090']

  # 应用指标
  - job_name: 'myapp'
    static_configs:
      - targets: ['app:9090']
    relabel_configs:
      - source_labels: [__address__]
        target_label: instance
        regex: '([^:]+):\d+'
        replacement: '${1}'

  # 服务发现（Kubernetes示例）
  - job_name: 'kubernetes-pods'
    kubernetes_sd_configs:
      - role: pod
    relabel_configs:
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_scrape]
        action: keep
        regex: true
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_path]
        action: replace
        target_label: __metrics_path__
        regex: (.+)
```

```yaml
# ==========================================
# alerts/app.yml - 告警规则
# ==========================================
groups:
  - name: app_alerts
    rules:
      # 高错误率告警
      - alert: HighErrorRate
        expr: |
          sum(rate(http_requests_total{status=~"5.."}[5m])) /
          sum(rate(http_requests_total[5m])) > 0.05
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High error rate detected"
          description: "Error rate is {{ $value | humanizePercentage }}"

      # 高延迟告警
      - alert: HighLatency
        expr: |
          histogram_quantile(0.95,
            sum(rate(http_request_duration_seconds_bucket[5m])) by (le)
          ) > 1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High latency detected"
          description: "95th percentile latency is {{ $value }}s"

      # 服务不可用
      - alert: ServiceDown
        expr: up == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Service {{ $labels.job }} is down"

      # 内存使用过高
      - alert: HighMemoryUsage
        expr: process_memory_bytes / process_memory_max_bytes > 0.9
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High memory usage"
          description: "Memory usage is {{ $value | humanizePercentage }}"
```

```json
// ==========================================
// Grafana仪表板配置（简化版）
// ==========================================
{
  "dashboard": {
    "title": "Application Dashboard",
    "panels": [
      {
        "title": "Request Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "sum(rate(http_requests_total[5m])) by (method)",
            "legendFormat": "{{ method }}"
          }
        ]
      },
      {
        "title": "Error Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "sum(rate(http_requests_total{status=~\"5..\"}[5m])) / sum(rate(http_requests_total[5m]))",
            "legendFormat": "error rate"
          }
        ]
      },
      {
        "title": "Latency P50/P95/P99",
        "type": "graph",
        "targets": [
          {
            "expr": "histogram_quantile(0.50, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))",
            "legendFormat": "p50"
          },
          {
            "expr": "histogram_quantile(0.95, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))",
            "legendFormat": "p95"
          },
          {
            "expr": "histogram_quantile(0.99, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))",
            "legendFormat": "p99"
          }
        ]
      },
      {
        "title": "Active Connections",
        "type": "gauge",
        "targets": [
          {
            "expr": "sum(active_connections)"
          }
        ]
      }
    ]
  }
}
```

**Docker Compose监控栈**：

```yaml
# docker-compose.monitoring.yml
version: '3.8'

services:
  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9090:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml
      - ./alerts:/etc/prometheus/alerts
      - prometheus_data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--web.enable-lifecycle'

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    volumes:
      - grafana_data:/var/lib/grafana
      - ./grafana/provisioning:/etc/grafana/provisioning
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin

  alertmanager:
    image: prom/alertmanager:latest
    ports:
      - "9093:9093"
    volumes:
      - ./alertmanager.yml:/etc/alertmanager/alertmanager.yml

  app:
    build: .
    ports:
      - "8080:8080"
      - "9091:9090"  # metrics port

volumes:
  prometheus_data:
  grafana_data:
```

---

## 源码阅读任务

### 本月源码阅读

1. **prometheus-cpp源码**
   - 仓库：https://github.com/jupp0r/prometheus-cpp
   - 学习目标：理解指标库的实现

2. **Prometheus源码**
   - 仓库：https://github.com/prometheus/prometheus
   - 重点：`promql/`目录
   - 学习目标：理解PromQL实现

3. **OpenTelemetry C++**
   - 仓库：https://github.com/open-telemetry/opentelemetry-cpp
   - 学习目标：了解新一代可观测性标准

---

## 实践项目

### 项目：完整的监控系统

创建一个集成指标收集的C++服务。

（详细代码参见上文）

---

## 检验标准

- [ ] 理解Prometheus指标类型
- [ ] 能够在C++中实现指标收集
- [ ] 能够配置Prometheus抓取
- [ ] 能够设计Grafana仪表板
- [ ] 能够编写告警规则
- [ ] 理解RED/USE监控方法

### 知识检验问题

1. Counter和Gauge的区别是什么？
2. 为什么Histogram可以计算分位数而Counter不行？
3. 如何选择合适的Histogram桶边界？
4. 什么是PromQL的rate函数？

---

## 输出物清单

1. **指标库**
   - 自定义C++指标库实现

2. **配置文件**
   - Prometheus配置
   - Grafana仪表板

3. **文档**
   - `notes/month47_metrics.md`
   - `notes/promql_cheatsheet.md`

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | 可观测性基础 | 15h | 15h | 5h |
| 第2周 | C++指标库 | 12h | 18h | 5h |
| 第3周 | prometheus-cpp | 10h | 20h | 5h |
| 第4周 | 监控系统集成 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 48将进行**第四年总结与综合项目**，整合全年所学构建一个完整的工程化项目。
