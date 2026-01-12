# Month 45: 性能分析与Profiling——找出程序的性能瓶颈

## 本月主题概述

本月深入学习性能分析工具和技术，掌握如何定位CPU热点、内存瓶颈和I/O问题。学习使用perf、Valgrind、Intel VTune等工具，以及如何编写高性能的C++代码。

**学习目标**：
- 掌握Linux perf工具的使用
- 学会使用Valgrind进行内存和缓存分析
- 理解CPU缓存和分支预测的影响
- 能够进行系统化的性能优化

---

## 理论学习内容

### 第一周：Linux perf基础

**学习目标**：掌握perf工具进行CPU性能分析

**阅读材料**：
- [ ] Linux perf wiki
- [ ] Brendan Gregg的perf教程
- [ ] 《性能之巅》相关章节

**核心概念**：

```bash
# ==========================================
# perf基本命令
# ==========================================

# 安装perf (Ubuntu)
sudo apt-get install linux-tools-common linux-tools-generic

# 统计程序运行信息
perf stat ./myapp

# 详细统计
perf stat -d ./myapp

# 采样分析
perf record -g ./myapp
perf report

# 实时top视图
perf top

# 采样特定事件
perf record -e cache-misses,cache-references ./myapp

# 采样特定进程
perf record -p <pid> sleep 10

# 生成火焰图数据
perf record -g ./myapp
perf script > out.perf
# 然后使用FlameGraph工具

# ==========================================
# 常用perf事件
# ==========================================

# 列出所有可用事件
perf list

# CPU周期
perf stat -e cycles,instructions ./myapp

# 缓存
perf stat -e cache-references,cache-misses ./myapp
perf stat -e L1-dcache-loads,L1-dcache-load-misses ./myapp

# 分支预测
perf stat -e branch-instructions,branch-misses ./myapp

# 页面错误
perf stat -e page-faults,minor-faults,major-faults ./myapp

# 上下文切换
perf stat -e context-switches,cpu-migrations ./myapp
```

**perf输出解读**：

```bash
# perf stat输出示例
$ perf stat ./myapp

 Performance counter stats for './myapp':

         1,234.56 msec task-clock                #    0.998 CPUs utilized
               42 context-switches              #   34.023 /sec
                0 cpu-migrations                #    0.000 /sec
            1,234 page-faults                   # 999.676 /sec
    4,567,890,123 cycles                        #    3.699 GHz
    3,456,789,012 instructions                  #    0.76  insn per cycle
      567,890,123 branches                      #  459.987 M/sec
       12,345,678 branch-misses                 #    2.17% of all branches

       1.236789 seconds time elapsed
       1.234567 seconds user
       0.000123 seconds sys

# 关键指标解读：
# - insn per cycle (IPC): 每周期指令数，越高越好
# - branch-misses: 分支预测失败率，应该<5%
# - cache-misses: 缓存未命中率
# - context-switches: 上下文切换次数
```

**火焰图生成**：

```bash
# ==========================================
# 生成火焰图
# ==========================================

# 克隆FlameGraph仓库
git clone https://github.com/brendangregg/FlameGraph.git

# 记录
perf record -g ./myapp

# 转换格式
perf script > out.perf

# 折叠栈帧
FlameGraph/stackcollapse-perf.pl out.perf > out.folded

# 生成SVG
FlameGraph/flamegraph.pl out.folded > flamegraph.svg

# 一行命令
perf script | FlameGraph/stackcollapse-perf.pl | FlameGraph/flamegraph.pl > flamegraph.svg

# 差分火焰图（对比两次运行）
FlameGraph/difffolded.pl out1.folded out2.folded | FlameGraph/flamegraph.pl > diff.svg
```

### 第二周：Valgrind工具集

**学习目标**：使用Valgrind进行内存和缓存分析

**阅读材料**：
- [ ] Valgrind官方文档
- [ ] Callgrind和KCachegrind使用指南
- [ ] Massif内存分析

```bash
# ==========================================
# Valgrind工具集
# ==========================================

# 安装
sudo apt-get install valgrind kcachegrind

# Memcheck - 内存错误检测
valgrind --leak-check=full --show-leak-kinds=all ./myapp

# Callgrind - 调用图分析
valgrind --tool=callgrind ./myapp
kcachegrind callgrind.out.*

# 生成调用图
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./myapp

# Cachegrind - 缓存分析
valgrind --tool=cachegrind ./myapp
cg_annotate cachegrind.out.*

# Massif - 堆内存分析
valgrind --tool=massif ./myapp
ms_print massif.out.*

# DHAT - 动态堆分析
valgrind --tool=dhat ./myapp

# Helgrind - 线程错误检测
valgrind --tool=helgrind ./myapp
```

**Cachegrind详解**：

```bash
# ==========================================
# Cachegrind缓存分析
# ==========================================

$ valgrind --tool=cachegrind ./myapp

==12345== Cachegrind, a cache and branch-prediction profiler
==12345==
==12345== I   refs:      1,234,567,890
==12345== I1  misses:           12,345
==12345== LLi misses:            1,234
==12345== I1  miss rate:          0.01%
==12345== LLi miss rate:          0.00%
==12345==
==12345== D   refs:        567,890,123  (345,678,901 rd   + 222,211,222 wr)
==12345== D1  misses:        1,234,567  (    987,654 rd   +     246,913 wr)
==12345== LLd misses:          123,456  (     98,765 rd   +      24,691 wr)
==12345== D1  miss rate:           0.2% (        0.3%     +         0.1%  )
==12345== LLd miss rate:           0.0% (        0.0%     +         0.0%  )
==12345==
==12345== LL refs:           1,246,912  (  1,000,000 rd   +     246,912 wr)
==12345== LL misses:           124,690  (     99,999 rd   +      24,691 wr)
==12345== LL miss rate:            0.0% (        0.0%     +         0.0%  )

# 指标解释：
# I refs: 指令引用
# D refs: 数据引用
# D1 misses: L1缓存未命中
# LL misses: 最后一级缓存（L3）未命中

# 按源码行注释
cg_annotate --auto=yes cachegrind.out.* source.cpp
```

**Massif内存分析**：

```bash
# ==========================================
# Massif堆内存分析
# ==========================================

valgrind --tool=massif --pages-as-heap=yes ./myapp

# 可视化
ms_print massif.out.* | head -100

# 输出示例（ASCII图）：
#     MB
# 12.00^                                                       #
#      |                                                      @#
#      |                                                     @@#
#      |                                                    @@@#
#      |                                                   @@@@#
#      |                                                  @@@@@#
#      |                                                 @@@@@@#
#      |                                                @@@@@@@#
#      |                                               @@@@@@@@#
#      |                                              @@@@@@@@@#
#      |                                             @@@@@@@@@@#
#      |                                            @@@@@@@@@@@#
#      |                                           @@@@@@@@@@@@#
#      |                                          @@@@@@@@@@@@@#
#      |                                         @@@@@@@@@@@@@@#
#   0 +--------------------------------------------------------------->Mi
#      0                                                           100

# 使用massif-visualizer（GUI）
sudo apt-get install massif-visualizer
massif-visualizer massif.out.*
```

### 第三周：Google Benchmark与微基准测试

**学习目标**：编写科学的微基准测试

**阅读材料**：
- [ ] Google Benchmark文档
- [ ] 微基准测试陷阱
- [ ] CPU流水线和优化器影响

```cpp
// ==========================================
// Google Benchmark基础
// ==========================================
#include <benchmark/benchmark.h>
#include <vector>
#include <algorithm>
#include <random>

// 基本基准测试
static void BM_VectorPushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        for (int i = 0; i < state.range(0); ++i) {
            v.push_back(i);
        }
        benchmark::DoNotOptimize(v.data());
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_VectorPushBack)->Range(8, 8<<10)->Complexity();

// 带预留空间
static void BM_VectorPushBackReserved(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        v.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            v.push_back(i);
        }
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_VectorPushBackReserved)->Range(8, 8<<10);

// 比较不同排序算法
static void BM_StdSort(benchmark::State& state) {
    std::vector<int> v(state.range(0));
    std::iota(v.begin(), v.end(), 0);

    for (auto _ : state) {
        state.PauseTiming();
        std::shuffle(v.begin(), v.end(), std::mt19937{42});
        state.ResumeTiming();

        std::sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_StdSort)->Range(8, 8<<16);

static void BM_StableSort(benchmark::State& state) {
    std::vector<int> v(state.range(0));
    std::iota(v.begin(), v.end(), 0);

    for (auto _ : state) {
        state.PauseTiming();
        std::shuffle(v.begin(), v.end(), std::mt19937{42});
        state.ResumeTiming();

        std::stable_sort(v.begin(), v.end());
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_StableSort)->Range(8, 8<<16);

// 内存访问模式比较
static void BM_SequentialAccess(benchmark::State& state) {
    std::vector<int> v(1024 * 1024, 1);

    for (auto _ : state) {
        int sum = 0;
        for (int i = 0; i < v.size(); ++i) {
            sum += v[i];
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * v.size() * sizeof(int));
}
BENCHMARK(BM_SequentialAccess);

static void BM_RandomAccess(benchmark::State& state) {
    std::vector<int> v(1024 * 1024, 1);
    std::vector<int> indices(v.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), std::mt19937{42});

    for (auto _ : state) {
        int sum = 0;
        for (int idx : indices) {
            sum += v[idx];
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * v.size() * sizeof(int));
}
BENCHMARK(BM_RandomAccess);

// 缓存行效应
static void BM_StrideAccess(benchmark::State& state) {
    std::vector<int> v(1024 * 1024, 1);
    const int stride = state.range(0);

    for (auto _ : state) {
        int sum = 0;
        for (int i = 0; i < v.size(); i += stride) {
            sum += v[i];
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_StrideAccess)->RangeMultiplier(2)->Range(1, 64);

// 分支预测
static void BM_PredictableBranch(benchmark::State& state) {
    std::vector<int> v(10000);
    std::iota(v.begin(), v.end(), 0);  // 有序

    for (auto _ : state) {
        int sum = 0;
        for (int x : v) {
            if (x < 5000) {  // 可预测：前半部分都是true
                sum += x;
            }
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_PredictableBranch);

static void BM_UnpredictableBranch(benchmark::State& state) {
    std::vector<int> v(10000);
    std::iota(v.begin(), v.end(), 0);
    std::shuffle(v.begin(), v.end(), std::mt19937{42});  // 随机

    for (auto _ : state) {
        int sum = 0;
        for (int x : v) {
            if (x < 5000) {  // 不可预测
                sum += x;
            }
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_UnpredictableBranch);

BENCHMARK_MAIN();
```

```cmake
# CMakeLists.txt
find_package(benchmark REQUIRED)

add_executable(benchmarks
    benchmarks.cpp
)

target_link_libraries(benchmarks
    PRIVATE
        benchmark::benchmark
)
```

### 第四周：实际性能优化案例

**学习目标**：综合运用工具进行性能优化

**阅读材料**：
- [ ] 《C++性能优化指南》
- [ ] SIMD指令入门
- [ ] 数据局部性优化

```cpp
// ==========================================
// 性能优化案例：矩阵乘法
// ==========================================
#include <vector>
#include <chrono>
#include <iostream>

// 朴素实现
void matrix_multiply_naive(
    const std::vector<std::vector<double>>& A,
    const std::vector<std::vector<double>>& B,
    std::vector<std::vector<double>>& C,
    int N
) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            C[i][j] = 0;
            for (int k = 0; k < N; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// 优化1：改变循环顺序（更好的缓存局部性）
void matrix_multiply_reordered(
    const std::vector<std::vector<double>>& A,
    const std::vector<std::vector<double>>& B,
    std::vector<std::vector<double>>& C,
    int N
) {
    // 先清零
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            C[i][j] = 0;
        }
    }

    // i-k-j顺序
    for (int i = 0; i < N; ++i) {
        for (int k = 0; k < N; ++k) {
            for (int j = 0; j < N; ++j) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// 优化2：分块（tiling）
void matrix_multiply_blocked(
    const std::vector<std::vector<double>>& A,
    const std::vector<std::vector<double>>& B,
    std::vector<std::vector<double>>& C,
    int N
) {
    constexpr int BLOCK_SIZE = 64;

    // 清零
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            C[i][j] = 0;
        }
    }

    for (int ii = 0; ii < N; ii += BLOCK_SIZE) {
        for (int kk = 0; kk < N; kk += BLOCK_SIZE) {
            for (int jj = 0; jj < N; jj += BLOCK_SIZE) {
                // 处理块
                for (int i = ii; i < std::min(ii + BLOCK_SIZE, N); ++i) {
                    for (int k = kk; k < std::min(kk + BLOCK_SIZE, N); ++k) {
                        for (int j = jj; j < std::min(jj + BLOCK_SIZE, N); ++j) {
                            C[i][j] += A[i][k] * B[k][j];
                        }
                    }
                }
            }
        }
    }
}

// 优化3：使用连续内存布局
class Matrix {
public:
    Matrix(int rows, int cols) : rows_(rows), cols_(cols), data_(rows * cols) {}

    double& operator()(int i, int j) { return data_[i * cols_ + j]; }
    double operator()(int i, int j) const { return data_[i * cols_ + j]; }

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    double* data() { return data_.data(); }

private:
    int rows_, cols_;
    std::vector<double> data_;
};

void matrix_multiply_flat(
    const Matrix& A,
    const Matrix& B,
    Matrix& C
) {
    const int N = A.rows();
    constexpr int BLOCK_SIZE = 64;

    // 清零
    std::fill(C.data(), C.data() + N * N, 0.0);

    for (int ii = 0; ii < N; ii += BLOCK_SIZE) {
        for (int kk = 0; kk < N; kk += BLOCK_SIZE) {
            for (int jj = 0; jj < N; jj += BLOCK_SIZE) {
                for (int i = ii; i < std::min(ii + BLOCK_SIZE, N); ++i) {
                    for (int k = kk; k < std::min(kk + BLOCK_SIZE, N); ++k) {
                        const double a_ik = A(i, k);
                        for (int j = jj; j < std::min(jj + BLOCK_SIZE, N); ++j) {
                            C(i, j) += a_ik * B(k, j);
                        }
                    }
                }
            }
        }
    }
}

// 优化4：SIMD（需要编译器支持）
#ifdef __AVX2__
#include <immintrin.h>

void matrix_multiply_simd(
    const Matrix& A,
    const Matrix& B,
    Matrix& C
) {
    const int N = A.rows();

    std::fill(C.data(), C.data() + N * N, 0.0);

    for (int i = 0; i < N; ++i) {
        for (int k = 0; k < N; ++k) {
            __m256d a_ik = _mm256_set1_pd(A(i, k));

            int j = 0;
            for (; j + 4 <= N; j += 4) {
                __m256d b_kj = _mm256_loadu_pd(&B(k, j));
                __m256d c_ij = _mm256_loadu_pd(&C(i, j));
                c_ij = _mm256_fmadd_pd(a_ik, b_kj, c_ij);
                _mm256_storeu_pd(&C(i, j), c_ij);
            }

            // 处理剩余元素
            for (; j < N; ++j) {
                C(i, j) += A(i, k) * B(k, j);
            }
        }
    }
}
#endif
```

---

## 源码阅读任务

### 本月源码阅读

1. **Google Benchmark源码**
   - 仓库：https://github.com/google/benchmark
   - 重点：`src/benchmark.cc`
   - 学习目标：理解时间测量的精确性

2. **perf源码**
   - Linux内核perf子系统
   - 学习目标：理解性能计数器工作原理

3. **高性能库的实现**
   - Eigen矩阵库的SIMD优化
   - folly的性能优化技巧

---

## 实践项目

### 项目：性能分析报告生成器

创建一个自动化的性能分析工具。

**项目结构**：

```
perf-analyzer/
├── CMakeLists.txt
├── include/
│   └── perfanalyzer/
│       ├── profiler.hpp
│       ├── reporter.hpp
│       └── metrics.hpp
├── src/
│   ├── profiler.cpp
│   └── reporter.cpp
├── tools/
│   └── analyze.cpp
└── scripts/
    └── generate_report.py
```

**include/perfanalyzer/profiler.hpp**：

```cpp
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <map>
#include <memory>

namespace perfanalyzer {

/**
 * @brief 性能指标
 */
struct Metrics {
    double cpu_time_seconds = 0;
    double wall_time_seconds = 0;
    uint64_t cycles = 0;
    uint64_t instructions = 0;
    uint64_t cache_references = 0;
    uint64_t cache_misses = 0;
    uint64_t branch_instructions = 0;
    uint64_t branch_misses = 0;
    size_t peak_memory_bytes = 0;

    double ipc() const {
        return cycles > 0 ? static_cast<double>(instructions) / cycles : 0;
    }

    double cache_miss_rate() const {
        return cache_references > 0
            ? static_cast<double>(cache_misses) / cache_references * 100
            : 0;
    }

    double branch_miss_rate() const {
        return branch_instructions > 0
            ? static_cast<double>(branch_misses) / branch_instructions * 100
            : 0;
    }
};

/**
 * @brief RAII计时器
 */
class ScopedTimer {
public:
    explicit ScopedTimer(double& result);
    ~ScopedTimer();

private:
    double& result_;
    std::chrono::high_resolution_clock::time_point start_;
};

/**
 * @brief 性能分析器
 */
class Profiler {
public:
    Profiler();
    ~Profiler();

    // 禁用拷贝
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    /**
     * @brief 开始采样
     */
    void start();

    /**
     * @brief 停止采样
     */
    void stop();

    /**
     * @brief 获取指标
     */
    Metrics getMetrics() const;

    /**
     * @brief 测量函数执行时间
     */
    template<typename Func>
    Metrics measure(Func&& func, int iterations = 1) {
        start();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        stop();
        return getMetrics();
    }

    /**
     * @brief 检查是否支持硬件性能计数器
     */
    static bool isHardwareCountersAvailable();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief 内存追踪器
 */
class MemoryTracker {
public:
    static size_t getCurrentUsage();
    static size_t getPeakUsage();
    static void resetPeak();
};

/**
 * @brief 性能分析会话
 */
class ProfilingSession {
public:
    struct Result {
        std::string name;
        Metrics metrics;
        std::vector<std::pair<std::string, double>> custom_metrics;
    };

    void addResult(const std::string& name, const Metrics& metrics);

    void addCustomMetric(
        const std::string& result_name,
        const std::string& metric_name,
        double value
    );

    const std::vector<Result>& getResults() const { return results_; }

    void clear() { results_.clear(); }

private:
    std::vector<Result> results_;
};

} // namespace perfanalyzer
```

**src/profiler.cpp**：

```cpp
#include "perfanalyzer/profiler.hpp"

#include <sys/resource.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#endif

namespace perfanalyzer {

// ScopedTimer实现
ScopedTimer::ScopedTimer(double& result)
    : result_(result)
    , start_(std::chrono::high_resolution_clock::now())
{}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    result_ = std::chrono::duration<double>(end - start_).count();
}

// Profiler实现
class Profiler::Impl {
public:
    Impl() {
#ifdef __linux__
        initPerfEvents();
#endif
    }

    ~Impl() {
#ifdef __linux__
        closePerfEvents();
#endif
    }

    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        start_cpu_time_ = getCpuTime();

#ifdef __linux__
        resetPerfCounters();
        enablePerfCounters();
#endif
    }

    void stop() {
#ifdef __linux__
        disablePerfCounters();
        readPerfCounters();
#endif

        end_time_ = std::chrono::high_resolution_clock::now();
        end_cpu_time_ = getCpuTime();
    }

    Metrics getMetrics() const {
        Metrics m;
        m.wall_time_seconds = std::chrono::duration<double>(
            end_time_ - start_time_).count();
        m.cpu_time_seconds = end_cpu_time_ - start_cpu_time_;

#ifdef __linux__
        m.cycles = counters_.cycles;
        m.instructions = counters_.instructions;
        m.cache_references = counters_.cache_references;
        m.cache_misses = counters_.cache_misses;
        m.branch_instructions = counters_.branch_instructions;
        m.branch_misses = counters_.branch_misses;
#endif

        return m;
    }

private:
    double getCpuTime() {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6
             + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
    }

#ifdef __linux__
    struct PerfCounters {
        uint64_t cycles = 0;
        uint64_t instructions = 0;
        uint64_t cache_references = 0;
        uint64_t cache_misses = 0;
        uint64_t branch_instructions = 0;
        uint64_t branch_misses = 0;
    };

    void initPerfEvents() {
        // 初始化perf事件
        fd_cycles_ = openPerfEvent(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
        fd_instructions_ = openPerfEvent(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        fd_cache_refs_ = openPerfEvent(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES);
        fd_cache_misses_ = openPerfEvent(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
        fd_branches_ = openPerfEvent(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
        fd_branch_misses_ = openPerfEvent(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);
    }

    int openPerfEvent(uint32_t type, uint64_t config) {
        struct perf_event_attr pe = {};
        pe.type = type;
        pe.size = sizeof(pe);
        pe.config = config;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;

        return syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    }

    void closePerfEvents() {
        if (fd_cycles_ >= 0) close(fd_cycles_);
        if (fd_instructions_ >= 0) close(fd_instructions_);
        if (fd_cache_refs_ >= 0) close(fd_cache_refs_);
        if (fd_cache_misses_ >= 0) close(fd_cache_misses_);
        if (fd_branches_ >= 0) close(fd_branches_);
        if (fd_branch_misses_ >= 0) close(fd_branch_misses_);
    }

    void resetPerfCounters() {
        if (fd_cycles_ >= 0) ioctl(fd_cycles_, PERF_EVENT_IOC_RESET, 0);
        if (fd_instructions_ >= 0) ioctl(fd_instructions_, PERF_EVENT_IOC_RESET, 0);
        if (fd_cache_refs_ >= 0) ioctl(fd_cache_refs_, PERF_EVENT_IOC_RESET, 0);
        if (fd_cache_misses_ >= 0) ioctl(fd_cache_misses_, PERF_EVENT_IOC_RESET, 0);
        if (fd_branches_ >= 0) ioctl(fd_branches_, PERF_EVENT_IOC_RESET, 0);
        if (fd_branch_misses_ >= 0) ioctl(fd_branch_misses_, PERF_EVENT_IOC_RESET, 0);
    }

    void enablePerfCounters() {
        if (fd_cycles_ >= 0) ioctl(fd_cycles_, PERF_EVENT_IOC_ENABLE, 0);
        if (fd_instructions_ >= 0) ioctl(fd_instructions_, PERF_EVENT_IOC_ENABLE, 0);
        if (fd_cache_refs_ >= 0) ioctl(fd_cache_refs_, PERF_EVENT_IOC_ENABLE, 0);
        if (fd_cache_misses_ >= 0) ioctl(fd_cache_misses_, PERF_EVENT_IOC_ENABLE, 0);
        if (fd_branches_ >= 0) ioctl(fd_branches_, PERF_EVENT_IOC_ENABLE, 0);
        if (fd_branch_misses_ >= 0) ioctl(fd_branch_misses_, PERF_EVENT_IOC_ENABLE, 0);
    }

    void disablePerfCounters() {
        if (fd_cycles_ >= 0) ioctl(fd_cycles_, PERF_EVENT_IOC_DISABLE, 0);
        if (fd_instructions_ >= 0) ioctl(fd_instructions_, PERF_EVENT_IOC_DISABLE, 0);
        if (fd_cache_refs_ >= 0) ioctl(fd_cache_refs_, PERF_EVENT_IOC_DISABLE, 0);
        if (fd_cache_misses_ >= 0) ioctl(fd_cache_misses_, PERF_EVENT_IOC_DISABLE, 0);
        if (fd_branches_ >= 0) ioctl(fd_branches_, PERF_EVENT_IOC_DISABLE, 0);
        if (fd_branch_misses_ >= 0) ioctl(fd_branch_misses_, PERF_EVENT_IOC_DISABLE, 0);
    }

    void readPerfCounters() {
        if (fd_cycles_ >= 0) read(fd_cycles_, &counters_.cycles, sizeof(uint64_t));
        if (fd_instructions_ >= 0) read(fd_instructions_, &counters_.instructions, sizeof(uint64_t));
        if (fd_cache_refs_ >= 0) read(fd_cache_refs_, &counters_.cache_references, sizeof(uint64_t));
        if (fd_cache_misses_ >= 0) read(fd_cache_misses_, &counters_.cache_misses, sizeof(uint64_t));
        if (fd_branches_ >= 0) read(fd_branches_, &counters_.branch_instructions, sizeof(uint64_t));
        if (fd_branch_misses_ >= 0) read(fd_branch_misses_, &counters_.branch_misses, sizeof(uint64_t));
    }

    int fd_cycles_ = -1;
    int fd_instructions_ = -1;
    int fd_cache_refs_ = -1;
    int fd_cache_misses_ = -1;
    int fd_branches_ = -1;
    int fd_branch_misses_ = -1;
    PerfCounters counters_;
#endif

    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    double start_cpu_time_ = 0;
    double end_cpu_time_ = 0;
};

Profiler::Profiler() : impl_(std::make_unique<Impl>()) {}
Profiler::~Profiler() = default;

void Profiler::start() { impl_->start(); }
void Profiler::stop() { impl_->stop(); }
Metrics Profiler::getMetrics() const { return impl_->getMetrics(); }

bool Profiler::isHardwareCountersAvailable() {
#ifdef __linux__
    struct perf_event_attr pe = {};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;

    int fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd >= 0) {
        close(fd);
        return true;
    }
#endif
    return false;
}

// MemoryTracker实现
size_t MemoryTracker::getCurrentUsage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * 1024;  // KB to bytes
}

size_t MemoryTracker::getPeakUsage() {
    return getCurrentUsage();  // 简化实现
}

void MemoryTracker::resetPeak() {
    // 无法重置系统跟踪的峰值
}

} // namespace perfanalyzer
```

---

## 检验标准

- [ ] 能够使用perf进行CPU性能分析
- [ ] 能够生成和解读火焰图
- [ ] 能够使用Valgrind分析内存和缓存
- [ ] 能够编写科学的微基准测试
- [ ] 理解CPU缓存和分支预测的影响
- [ ] 能够进行系统化的性能优化

### 知识检验问题

1. 如何解读perf stat的IPC指标？
2. 缓存友好的代码有哪些特征？
3. 为什么随机访问比顺序访问慢？
4. 微基准测试有哪些常见陷阱？

---

## 输出物清单

1. **工具代码**
   - `perf-analyzer/` - 性能分析工具

2. **基准测试**
   - `benchmarks/` - 各种基准测试

3. **文档**
   - `notes/month45_profiling.md` - 学习笔记
   - `notes/optimization_guide.md` - 优化指南

4. **脚本**
   - `scripts/generate_flamegraph.sh`
   - `scripts/run_benchmarks.sh`

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | Linux perf | 15h | 15h | 5h |
| 第2周 | Valgrind工具集 | 12h | 18h | 5h |
| 第3周 | Google Benchmark | 10h | 20h | 5h |
| 第4周 | 优化案例 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 46将学习**日志系统设计**，掌握高性能日志系统的设计与实现。
