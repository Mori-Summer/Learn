# Month 54: SYCL与异构计算入门 (SYCL and Heterogeneous Computing Introduction)

## 本月主题概述

SYCL（发音"sickle"）是Khronos Group制定的跨平台异构计算标准，允许开发者使用纯C++编写在CPU、GPU、FPGA等多种设备上运行的代码。作为OpenCL的高层C++抽象，SYCL提供了现代C++特性支持，同时保持了高性能。本月将学习异构计算基础和SYCL编程模型。

### 学习目标
- 理解异构计算的基本概念
- 掌握SYCL编程模型和核心API
- 理解设备内存层次和数据传输
- 学会编写和优化SYCL内核
- 完成实用的SYCL加速应用

---

## 理论学习内容

### 第一周：异构计算基础

#### 阅读材料
1. 《Programming Massively Parallel Processors》- Chapter 1-3
2. SYCL 2020规范文档
3. Intel oneAPI文档
4. GPU架构白皮书（NVIDIA/AMD/Intel）

#### 核心概念

**异构计算架构**
```
┌─────────────────────────────────────────────────────────┐
│                    异构计算系统                          │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                      Host (CPU)                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │  主程序 │ 内存管理 │ 任务调度 │ 数据准备          │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
        │           │           │           │
        ▼           ▼           ▼           ▼
┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐
│   GPU     │ │   GPU     │ │   FPGA    │ │   DSP     │
│  Device 0 │ │  Device 1 │ │  Device 2 │ │  Device 3 │
└───────────┘ └───────────┘ └───────────┘ └───────────┘
     │              │              │             │
     └──────────────┴──────────────┴─────────────┘
                         │
                    PCIe / 共享内存

CPU vs GPU架构对比：

CPU (少量强核心):
┌────────────────────────────────────────┐
│  Core │ Core │ Core │ Core │ L3 Cache │
│  (大) │ (大) │ (大) │ (大) │  (大)     │
└────────────────────────────────────────┘
- 低延迟优先
- 复杂控制流
- 分支预测优化
- 大缓存

GPU (大量弱核心):
┌────────────────────────────────────────────────────────┐
│ SM │ SM │ SM │ SM │ SM │ SM │ SM │ SM │ ... │ L2     │
│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│     │ Cache  │
│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│ ▪▪▪│     │        │
└────────────────────────────────────────────────────────┘
- 高吞吐量优先
- SIMD/SIMT执行
- 简单控制流
- 高内存带宽
```

**SYCL编程模型**
```cpp
#include <sycl/sycl.hpp>

int main() {
    // 1. 选择设备和创建队列
    sycl::queue q{sycl::gpu_selector_v};

    std::cout << "Running on: "
              << q.get_device().get_info<sycl::info::device::name>()
              << std::endl;

    // 2. 分配内存
    constexpr size_t N = 1024;
    std::vector<float> a(N, 1.0f);
    std::vector<float> b(N, 2.0f);
    std::vector<float> c(N);

    // 3. 创建缓冲区
    {
        sycl::buffer<float> buf_a(a.data(), sycl::range<1>(N));
        sycl::buffer<float> buf_b(b.data(), sycl::range<1>(N));
        sycl::buffer<float> buf_c(c.data(), sycl::range<1>(N));

        // 4. 提交命令
        q.submit([&](sycl::handler& h) {
            // 5. 创建访问器
            auto acc_a = buf_a.get_access<sycl::access::mode::read>(h);
            auto acc_b = buf_b.get_access<sycl::access::mode::read>(h);
            auto acc_c = buf_c.get_access<sycl::access::mode::write>(h);

            // 6. 执行内核
            h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
                acc_c[i] = acc_a[i] + acc_b[i];
            });
        });

        // 7. 等待完成（隐式同步在buffer作用域结束时）
    }

    // 8. 使用结果
    std::cout << "c[0] = " << c[0] << std::endl;  // 3.0

    return 0;
}
```

### 第二周：SYCL内存模型

#### 阅读材料
1. SYCL规范 - Memory Model章节
2. USM (Unified Shared Memory)指南
3. Buffer和Accessor详解

#### 核心概念

**内存类型**
```
┌─────────────────────────────────────────────────────────┐
│                    SYCL内存模型                          │
└─────────────────────────────────────────────────────────┘

1. Buffer/Accessor模型（传统方式）:
┌──────────────┐         ┌──────────────┐
│   Host       │  copy   │   Device     │
│   Memory     │ ◄─────► │   Memory     │
│  std::vector │         │   Buffer     │
└──────────────┘         └──────────────┘
- 自动数据移动
- SYCL运行时管理
- 通过Accessor访问

2. USM (Unified Shared Memory):

   a) Device Memory:
      ┌──────────────┐
      │   Device     │  只能设备访问
      │   malloc_device()
      └──────────────┘

   b) Host Memory:
      ┌──────────────┐
      │   Host       │  主机优化，设备可访问
      │   malloc_host()
      └──────────────┘

   c) Shared Memory:
      ┌──────────────────┐
      │   Shared         │  自动迁移
      │   malloc_shared()│  主机和设备都可访问
      └──────────────────┘
```

**Buffer和Accessor示例**
```cpp
#include <sycl/sycl.hpp>

void bufferExample() {
    sycl::queue q;
    constexpr size_t N = 1024;

    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0);

    {
        // Buffer持有数据所有权
        sycl::buffer<int, 1> buf(data.data(), sycl::range<1>(N));

        q.submit([&](sycl::handler& h) {
            // 不同访问模式
            // read - 只读
            // write - 只写（不从host复制）
            // read_write - 读写
            auto acc = buf.get_access<sycl::access::mode::read_write>(h);

            h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
                acc[i] *= 2;
            });
        });

        // 主机访问器 - 阻塞直到内核完成
        auto host_acc = buf.get_access<sycl::access::mode::read>();
        std::cout << "data[0] = " << host_acc[0] << std::endl;

    } // buffer析构时数据自动写回data向量
}

void usmExample() {
    sycl::queue q;
    constexpr size_t N = 1024;

    // 分配共享内存
    int* shared_data = sycl::malloc_shared<int>(N, q);

    // 初始化
    for (size_t i = 0; i < N; ++i) {
        shared_data[i] = static_cast<int>(i);
    }

    // 内核可以直接使用指针
    q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
        shared_data[i] *= 2;
    }).wait();

    std::cout << "shared_data[0] = " << shared_data[0] << std::endl;

    // 释放内存
    sycl::free(shared_data, q);
}

void deviceMemoryExample() {
    sycl::queue q;
    constexpr size_t N = 1024;

    // Host数据
    std::vector<int> host_data(N);
    std::iota(host_data.begin(), host_data.end(), 0);

    // Device内存
    int* device_data = sycl::malloc_device<int>(N, q);

    // 复制到设备
    q.memcpy(device_data, host_data.data(), N * sizeof(int)).wait();

    // 在设备上处理
    q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
        device_data[i] *= 2;
    }).wait();

    // 复制回主机
    q.memcpy(host_data.data(), device_data, N * sizeof(int)).wait();

    std::cout << "host_data[0] = " << host_data[0] << std::endl;

    sycl::free(device_data, q);
}
```

### 第三周：并行执行模型

#### 阅读材料
1. SYCL nd_range和工作组
2. GPU执行模型详解
3. 同步和屏障

#### 核心概念

**执行层次**
```
┌─────────────────────────────────────────────────────────┐
│                    SYCL执行模型                          │
└─────────────────────────────────────────────────────────┘

           ND-Range (全局范围)
┌───────────────────────────────────────────┐
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐         │
│  │ WG  │ │ WG  │ │ WG  │ │ WG  │         │
│  └─────┘ └─────┘ └─────┘ └─────┘         │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐         │
│  │ WG  │ │ WG  │ │ WG  │ │ WG  │         │
│  └─────┘ └─────┘ └─────┘ └─────┘         │
│                                           │
│  WG = Work-Group (工作组)                 │
└───────────────────────────────────────────┘

           Work-Group (工作组)
┌─────────────────────────────────────────┐
│  ┌───┬───┬───┬───┐                      │
│  │WI │WI │WI │WI │  ──── Sub-group      │
│  └───┴───┴───┴───┘                      │
│  ┌───┬───┬───┬───┐                      │
│  │WI │WI │WI │WI │  ──── Sub-group      │
│  └───┴───┴───┴───┘                      │
│                                          │
│  WI = Work-Item (工作项/线程)            │
│  共享 Local Memory                       │
└─────────────────────────────────────────┘

ID空间：
- Global ID: 全局唯一标识
- Local ID: 组内标识
- Group ID: 组标识

global_id = group_id * local_size + local_id
```

**ND-Range内核示例**
```cpp
#include <sycl/sycl.hpp>

void ndRangeExample() {
    sycl::queue q;

    constexpr size_t N = 1024;
    constexpr size_t LOCAL_SIZE = 64;  // 工作组大小

    std::vector<float> data(N, 1.0f);

    {
        sycl::buffer<float> buf(data.data(), sycl::range<1>(N));

        q.submit([&](sycl::handler& h) {
            auto acc = buf.get_access<sycl::access::mode::read_write>(h);

            // Local内存（工作组共享）
            sycl::local_accessor<float, 1> local_mem(
                sycl::range<1>(LOCAL_SIZE), h);

            h.parallel_for(
                sycl::nd_range<1>(
                    sycl::range<1>(N),          // 全局范围
                    sycl::range<1>(LOCAL_SIZE)  // 本地范围
                ),
                [=](sycl::nd_item<1> item) {
                    size_t global_id = item.get_global_id(0);
                    size_t local_id = item.get_local_id(0);
                    size_t group_id = item.get_group(0);

                    // 加载到local memory
                    local_mem[local_id] = acc[global_id];

                    // 组内同步屏障
                    item.barrier(sycl::access::fence_space::local_space);

                    // 使用local memory做计算
                    // （示例：组内归约求和）
                    for (size_t stride = LOCAL_SIZE / 2; stride > 0; stride /= 2) {
                        if (local_id < stride) {
                            local_mem[local_id] += local_mem[local_id + stride];
                        }
                        item.barrier(sycl::access::fence_space::local_space);
                    }

                    // 第一个线程写回结果
                    if (local_id == 0) {
                        acc[group_id] = local_mem[0];
                    }
                }
            );
        });
    }

    // 前N/LOCAL_SIZE个元素现在包含部分和
    float total = 0;
    for (size_t i = 0; i < N / LOCAL_SIZE; ++i) {
        total += data[i];
    }
    std::cout << "Sum: " << total << std::endl;  // 1024.0
}
```

### 第四周：优化技术

#### 阅读材料
1. GPU性能优化指南
2. 内存合并访问
3. 占用率优化
4. Profiling工具使用

#### 核心概念

**性能优化策略**
```cpp
// 1. 内存合并访问（Coalesced Access）

// 不好：跳跃访问
h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
    // 每个线程访问不连续的内存
    result[i] = data[i * stride];  // 非合并
});

// 好：连续访问
h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
    // 相邻线程访问相邻内存
    result[i] = data[i];  // 合并访问
});


// 2. 避免分支分歧

// 不好：同一warp/wavefront内的线程走不同分支
h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
    if (i % 2 == 0) {
        // 一半线程执行这里
        expensive_operation_a();
    } else {
        // 另一半执行这里
        expensive_operation_b();
    }
});

// 好：基于工作组的分支
h.parallel_for(
    sycl::nd_range<1>(N, LOCAL_SIZE),
    [=](sycl::nd_item<1> item) {
        if (item.get_group(0) % 2 == 0) {
            // 整个组执行同一分支
            expensive_operation_a();
        } else {
            expensive_operation_b();
        }
    }
);


// 3. 使用Local Memory减少全局内存访问

// 不好：重复访问全局内存
h.parallel_for(sycl::range<2>(M, N), [=](sycl::id<2> id) {
    float sum = 0;
    for (int k = 0; k < K; ++k) {
        sum += A[id[0]][k] * B[k][id[1]];  // 大量全局内存访问
    }
    C[id[0]][id[1]] = sum;
});

// 好：分块并使用local memory
constexpr int TILE = 16;
h.parallel_for(
    sycl::nd_range<2>({M, N}, {TILE, TILE}),
    [=](sycl::nd_item<2> item) {
        sycl::local_accessor<float, 2> tileA({TILE, TILE}, h);
        sycl::local_accessor<float, 2> tileB({TILE, TILE}, h);

        int row = item.get_global_id(0);
        int col = item.get_global_id(1);
        int localRow = item.get_local_id(0);
        int localCol = item.get_local_id(1);

        float sum = 0;

        for (int t = 0; t < (K + TILE - 1) / TILE; ++t) {
            // 协作加载到local memory
            tileA[localRow][localCol] = A[row][t * TILE + localCol];
            tileB[localRow][localCol] = B[t * TILE + localRow][col];

            item.barrier(sycl::access::fence_space::local_space);

            // 从local memory计算
            for (int k = 0; k < TILE; ++k) {
                sum += tileA[localRow][k] * tileB[k][localCol];
            }

            item.barrier(sycl::access::fence_space::local_space);
        }

        C[row][col] = sum;
    }
);
```

---

## 源码阅读任务

### 必读项目

1. **SYCL标准库实现** (hipSYCL/AdaptiveCpp)
   - https://github.com/AdaptiveCpp/AdaptiveCpp
   - 重点：runtime和编译器实现
   - 阅读时间：12小时

2. **oneDNN** (https://github.com/oneapi-src/oneDNN)
   - 重点：SYCL后端实现
   - 学习目标：理解高性能计算库设计
   - 阅读时间：8小时

3. **SYCL-BLAS** (https://github.com/codeplaysoftware/sycl-blas)
   - 学习目标：理解BLAS库的SYCL实现
   - 阅读时间：6小时

---

## 实践项目：SYCL矩阵运算库

### 项目概述
使用SYCL实现高性能矩阵运算库，包括向量运算、矩阵乘法和基本线性代数操作。

### 完整代码实现

#### 1. SYCL工具库 (sycl_utils/device.hpp)

```cpp
#pragma once

#include <sycl/sycl.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace sycl_utils {

// 设备信息打印
void printDeviceInfo(const sycl::device& device) {
    std::cout << "Device: "
              << device.get_info<sycl::info::device::name>() << "\n";
    std::cout << "  Vendor: "
              << device.get_info<sycl::info::device::vendor>() << "\n";
    std::cout << "  Max compute units: "
              << device.get_info<sycl::info::device::max_compute_units>() << "\n";
    std::cout << "  Max work group size: "
              << device.get_info<sycl::info::device::max_work_group_size>() << "\n";
    std::cout << "  Global memory: "
              << device.get_info<sycl::info::device::global_mem_size>() / (1024*1024)
              << " MB\n";
    std::cout << "  Local memory: "
              << device.get_info<sycl::info::device::local_mem_size>() / 1024
              << " KB\n";
}

// 列出所有设备
void listAllDevices() {
    auto platforms = sycl::platform::get_platforms();

    std::cout << "Available SYCL Devices:\n";
    std::cout << "=======================\n\n";

    for (const auto& platform : platforms) {
        std::cout << "Platform: "
                  << platform.get_info<sycl::info::platform::name>() << "\n";

        for (const auto& device : platform.get_devices()) {
            std::cout << "  ";
            printDeviceInfo(device);
            std::cout << "\n";
        }
    }
}

// 选择最佳设备
sycl::device selectBestDevice() {
    try {
        return sycl::device{sycl::gpu_selector_v};
    } catch (...) {
        std::cout << "No GPU found, falling back to CPU\n";
        return sycl::device{sycl::cpu_selector_v};
    }
}

// 异常处理
void handleException(const sycl::exception& e) {
    std::cerr << "SYCL exception: " << e.what() << "\n";
    if (e.code().value()) {
        std::cerr << "  Error code: " << e.code() << "\n";
    }
}

// 异步错误处理器
auto asyncHandler = [](sycl::exception_list exceptions) {
    for (const auto& e : exceptions) {
        try {
            std::rethrow_exception(e);
        } catch (const sycl::exception& ex) {
            handleException(ex);
        }
    }
};

// 创建带错误处理的队列
sycl::queue createQueue(const sycl::device& device = selectBestDevice()) {
    return sycl::queue{device, asyncHandler};
}

// 计时器
class Timer {
    std::chrono::high_resolution_clock::time_point start_;

public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }
};

} // namespace sycl_utils
```

#### 2. 向量运算 (sycl_math/vector_ops.hpp)

```cpp
#pragma once

#include <sycl/sycl.hpp>
#include <vector>
#include <cmath>

namespace sycl_math {

// 向量加法
template<typename T>
void vectorAdd(sycl::queue& q,
               const T* a, const T* b, T* c,
               size_t n) {
    q.parallel_for(sycl::range<1>(n), [=](sycl::id<1> i) {
        c[i] = a[i] + b[i];
    }).wait();
}

// 向量乘法（逐元素）
template<typename T>
void vectorMul(sycl::queue& q,
               const T* a, const T* b, T* c,
               size_t n) {
    q.parallel_for(sycl::range<1>(n), [=](sycl::id<1> i) {
        c[i] = a[i] * b[i];
    }).wait();
}

// 标量乘法
template<typename T>
void vectorScale(sycl::queue& q,
                 const T* a, T scalar, T* c,
                 size_t n) {
    q.parallel_for(sycl::range<1>(n), [=](sycl::id<1> i) {
        c[i] = a[i] * scalar;
    }).wait();
}

// 点积（使用归约）
template<typename T>
T dotProduct(sycl::queue& q, const T* a, const T* b, size_t n) {
    constexpr size_t LOCAL_SIZE = 256;
    size_t numGroups = (n + LOCAL_SIZE - 1) / LOCAL_SIZE;

    // 部分和
    T* partialSums = sycl::malloc_device<T>(numGroups, q);

    q.submit([&](sycl::handler& h) {
        sycl::local_accessor<T, 1> localMem(sycl::range<1>(LOCAL_SIZE), h);

        h.parallel_for(
            sycl::nd_range<1>(numGroups * LOCAL_SIZE, LOCAL_SIZE),
            [=](sycl::nd_item<1> item) {
                size_t globalId = item.get_global_id(0);
                size_t localId = item.get_local_id(0);
                size_t groupId = item.get_group(0);

                // 加载并计算局部乘积
                T value = (globalId < n) ? a[globalId] * b[globalId] : T(0);
                localMem[localId] = value;

                item.barrier(sycl::access::fence_space::local_space);

                // 归约
                for (size_t stride = LOCAL_SIZE / 2; stride > 0; stride /= 2) {
                    if (localId < stride) {
                        localMem[localId] += localMem[localId + stride];
                    }
                    item.barrier(sycl::access::fence_space::local_space);
                }

                if (localId == 0) {
                    partialSums[groupId] = localMem[0];
                }
            }
        );
    }).wait();

    // 将部分和复制回主机
    std::vector<T> hostPartialSums(numGroups);
    q.memcpy(hostPartialSums.data(), partialSums, numGroups * sizeof(T)).wait();

    sycl::free(partialSums, q);

    // 最终求和
    T result = 0;
    for (size_t i = 0; i < numGroups; ++i) {
        result += hostPartialSums[i];
    }

    return result;
}

// 向量范数
template<typename T>
T vectorNorm(sycl::queue& q, const T* a, size_t n) {
    return std::sqrt(dotProduct(q, a, a, n));
}

// AXPY: y = alpha * x + y
template<typename T>
void axpy(sycl::queue& q,
          T alpha, const T* x, T* y,
          size_t n) {
    q.parallel_for(sycl::range<1>(n), [=](sycl::id<1> i) {
        y[i] = alpha * x[i] + y[i];
    }).wait();
}

// 向量归一化
template<typename T>
void vectorNormalize(sycl::queue& q, T* a, size_t n) {
    T norm = vectorNorm(q, a, n);
    if (norm > 0) {
        vectorScale(q, a, T(1) / norm, a, n);
    }
}

} // namespace sycl_math
```

#### 3. 矩阵运算 (sycl_math/matrix_ops.hpp)

```cpp
#pragma once

#include <sycl/sycl.hpp>
#include <vector>
#include <stdexcept>

namespace sycl_math {

// 矩阵类（行主序存储）
template<typename T>
class Matrix {
private:
    size_t rows_, cols_;
    T* data_;
    sycl::queue* queue_;
    bool ownsData_;

public:
    Matrix(sycl::queue& q, size_t rows, size_t cols)
        : rows_(rows), cols_(cols), queue_(&q), ownsData_(true) {
        data_ = sycl::malloc_shared<T>(rows * cols, q);
    }

    Matrix(sycl::queue& q, size_t rows, size_t cols, T* externalData)
        : rows_(rows), cols_(cols), data_(externalData),
          queue_(&q), ownsData_(false) {}

    ~Matrix() {
        if (ownsData_ && data_) {
            sycl::free(data_, *queue_);
        }
    }

    // 禁止拷贝
    Matrix(const Matrix&) = delete;
    Matrix& operator=(const Matrix&) = delete;

    // 允许移动
    Matrix(Matrix&& other) noexcept
        : rows_(other.rows_), cols_(other.cols_),
          data_(other.data_), queue_(other.queue_),
          ownsData_(other.ownsData_) {
        other.data_ = nullptr;
        other.ownsData_ = false;
    }

    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }
    T* data() { return data_; }
    const T* data() const { return data_; }
    sycl::queue& queue() { return *queue_; }

    // 元素访问（主机端）
    T& operator()(size_t i, size_t j) {
        return data_[i * cols_ + j];
    }

    const T& operator()(size_t i, size_t j) const {
        return data_[i * cols_ + j];
    }

    // 填充
    void fill(T value) {
        queue_->parallel_for(sycl::range<1>(rows_ * cols_), [=, d = data_](sycl::id<1> i) {
            d[i] = value;
        }).wait();
    }

    // 单位矩阵
    void identity() {
        fill(T(0));
        size_t n = std::min(rows_, cols_);
        T* d = data_;
        size_t c = cols_;
        queue_->parallel_for(sycl::range<1>(n), [=](sycl::id<1> i) {
            d[i * c + i] = T(1);
        }).wait();
    }
};

// 朴素矩阵乘法
template<typename T>
void matmulNaive(sycl::queue& q,
                 const Matrix<T>& A,
                 const Matrix<T>& B,
                 Matrix<T>& C) {
    if (A.cols() != B.rows()) {
        throw std::invalid_argument("Matrix dimensions mismatch");
    }

    size_t M = A.rows();
    size_t N = B.cols();
    size_t K = A.cols();

    const T* a = A.data();
    const T* b = B.data();
    T* c = C.data();

    q.parallel_for(sycl::range<2>(M, N), [=](sycl::id<2> id) {
        size_t row = id[0];
        size_t col = id[1];

        T sum = 0;
        for (size_t k = 0; k < K; ++k) {
            sum += a[row * K + k] * b[k * N + col];
        }
        c[row * N + col] = sum;
    }).wait();
}

// 分块矩阵乘法（优化版本）
template<typename T, int TILE_SIZE = 16>
void matmulTiled(sycl::queue& q,
                 const Matrix<T>& A,
                 const Matrix<T>& B,
                 Matrix<T>& C) {
    if (A.cols() != B.rows()) {
        throw std::invalid_argument("Matrix dimensions mismatch");
    }

    size_t M = A.rows();
    size_t N = B.cols();
    size_t K = A.cols();

    const T* a = A.data();
    const T* b = B.data();
    T* c = C.data();

    // 全局范围需要对齐到TILE_SIZE
    size_t globalM = ((M + TILE_SIZE - 1) / TILE_SIZE) * TILE_SIZE;
    size_t globalN = ((N + TILE_SIZE - 1) / TILE_SIZE) * TILE_SIZE;

    q.submit([&](sycl::handler& h) {
        // Local memory for tiles
        sycl::local_accessor<T, 2> tileA(
            sycl::range<2>(TILE_SIZE, TILE_SIZE), h);
        sycl::local_accessor<T, 2> tileB(
            sycl::range<2>(TILE_SIZE, TILE_SIZE), h);

        h.parallel_for(
            sycl::nd_range<2>(
                sycl::range<2>(globalM, globalN),
                sycl::range<2>(TILE_SIZE, TILE_SIZE)
            ),
            [=](sycl::nd_item<2> item) {
                size_t row = item.get_global_id(0);
                size_t col = item.get_global_id(1);
                size_t localRow = item.get_local_id(0);
                size_t localCol = item.get_local_id(1);

                T sum = 0;

                // 遍历所有tile
                size_t numTiles = (K + TILE_SIZE - 1) / TILE_SIZE;

                for (size_t t = 0; t < numTiles; ++t) {
                    // 协作加载A的tile
                    size_t aRow = row;
                    size_t aCol = t * TILE_SIZE + localCol;
                    if (aRow < M && aCol < K) {
                        tileA[localRow][localCol] = a[aRow * K + aCol];
                    } else {
                        tileA[localRow][localCol] = 0;
                    }

                    // 协作加载B的tile
                    size_t bRow = t * TILE_SIZE + localRow;
                    size_t bCol = col;
                    if (bRow < K && bCol < N) {
                        tileB[localRow][localCol] = b[bRow * N + bCol];
                    } else {
                        tileB[localRow][localCol] = 0;
                    }

                    // 等待所有线程完成加载
                    item.barrier(sycl::access::fence_space::local_space);

                    // 计算部分积
                    for (int k = 0; k < TILE_SIZE; ++k) {
                        sum += tileA[localRow][k] * tileB[k][localCol];
                    }

                    // 等待所有计算完成再加载下一个tile
                    item.barrier(sycl::access::fence_space::local_space);
                }

                // 写入结果
                if (row < M && col < N) {
                    c[row * N + col] = sum;
                }
            }
        );
    }).wait();
}

// 矩阵转置
template<typename T>
void transpose(sycl::queue& q,
               const Matrix<T>& A,
               Matrix<T>& B) {
    size_t M = A.rows();
    size_t N = A.cols();

    const T* a = A.data();
    T* b = B.data();

    q.parallel_for(sycl::range<2>(M, N), [=](sycl::id<2> id) {
        size_t i = id[0];
        size_t j = id[1];
        b[j * M + i] = a[i * N + j];
    }).wait();
}

// 矩阵加法
template<typename T>
void matrixAdd(sycl::queue& q,
               const Matrix<T>& A,
               const Matrix<T>& B,
               Matrix<T>& C) {
    size_t M = A.rows();
    size_t N = A.cols();

    const T* a = A.data();
    const T* b = B.data();
    T* c = C.data();

    q.parallel_for(sycl::range<1>(M * N), [=](sycl::id<1> i) {
        c[i] = a[i] + b[i];
    }).wait();
}

// 矩阵标量乘法
template<typename T>
void matrixScale(sycl::queue& q,
                 const Matrix<T>& A,
                 T scalar,
                 Matrix<T>& C) {
    size_t M = A.rows();
    size_t N = A.cols();

    const T* a = A.data();
    T* c = C.data();

    q.parallel_for(sycl::range<1>(M * N), [=](sycl::id<1> i) {
        c[i] = a[i] * scalar;
    }).wait();
}

} // namespace sycl_math
```

#### 4. 高级操作 (sycl_math/advanced_ops.hpp)

```cpp
#pragma once

#include "matrix_ops.hpp"
#include <limits>

namespace sycl_math {

// 归约操作：求和
template<typename T>
T reduce_sum(sycl::queue& q, const T* data, size_t n) {
    constexpr size_t LOCAL_SIZE = 256;
    size_t numGroups = (n + LOCAL_SIZE - 1) / LOCAL_SIZE;

    T* partialSums = sycl::malloc_device<T>(numGroups, q);

    q.submit([&](sycl::handler& h) {
        sycl::local_accessor<T, 1> localMem(LOCAL_SIZE, h);

        h.parallel_for(
            sycl::nd_range<1>(numGroups * LOCAL_SIZE, LOCAL_SIZE),
            [=](sycl::nd_item<1> item) {
                size_t globalId = item.get_global_id(0);
                size_t localId = item.get_local_id(0);
                size_t groupId = item.get_group(0);

                localMem[localId] = (globalId < n) ? data[globalId] : T(0);
                item.barrier(sycl::access::fence_space::local_space);

                for (size_t stride = LOCAL_SIZE / 2; stride > 0; stride /= 2) {
                    if (localId < stride) {
                        localMem[localId] += localMem[localId + stride];
                    }
                    item.barrier(sycl::access::fence_space::local_space);
                }

                if (localId == 0) {
                    partialSums[groupId] = localMem[0];
                }
            }
        );
    }).wait();

    std::vector<T> hostSums(numGroups);
    q.memcpy(hostSums.data(), partialSums, numGroups * sizeof(T)).wait();
    sycl::free(partialSums, q);

    T result = 0;
    for (size_t i = 0; i < numGroups; ++i) {
        result += hostSums[i];
    }
    return result;
}

// 归约操作：最大值
template<typename T>
T reduce_max(sycl::queue& q, const T* data, size_t n) {
    constexpr size_t LOCAL_SIZE = 256;
    size_t numGroups = (n + LOCAL_SIZE - 1) / LOCAL_SIZE;

    T* partialMax = sycl::malloc_device<T>(numGroups, q);

    q.submit([&](sycl::handler& h) {
        sycl::local_accessor<T, 1> localMem(LOCAL_SIZE, h);

        h.parallel_for(
            sycl::nd_range<1>(numGroups * LOCAL_SIZE, LOCAL_SIZE),
            [=](sycl::nd_item<1> item) {
                size_t globalId = item.get_global_id(0);
                size_t localId = item.get_local_id(0);
                size_t groupId = item.get_group(0);

                localMem[localId] = (globalId < n) ?
                    data[globalId] : std::numeric_limits<T>::lowest();
                item.barrier(sycl::access::fence_space::local_space);

                for (size_t stride = LOCAL_SIZE / 2; stride > 0; stride /= 2) {
                    if (localId < stride) {
                        localMem[localId] = sycl::max(
                            localMem[localId], localMem[localId + stride]);
                    }
                    item.barrier(sycl::access::fence_space::local_space);
                }

                if (localId == 0) {
                    partialMax[groupId] = localMem[0];
                }
            }
        );
    }).wait();

    std::vector<T> hostMax(numGroups);
    q.memcpy(hostMax.data(), partialMax, numGroups * sizeof(T)).wait();
    sycl::free(partialMax, q);

    T result = std::numeric_limits<T>::lowest();
    for (size_t i = 0; i < numGroups; ++i) {
        result = std::max(result, hostMax[i]);
    }
    return result;
}

// Softmax (行级别)
template<typename T>
void softmax(sycl::queue& q, Matrix<T>& A) {
    size_t M = A.rows();
    size_t N = A.cols();
    T* data = A.data();

    // 对每行计算softmax
    q.submit([&](sycl::handler& h) {
        h.parallel_for(sycl::range<1>(M), [=](sycl::id<1> row) {
            // 找最大值（数值稳定性）
            T maxVal = data[row * N];
            for (size_t j = 1; j < N; ++j) {
                maxVal = sycl::max(maxVal, data[row * N + j]);
            }

            // 计算exp和sum
            T sum = 0;
            for (size_t j = 0; j < N; ++j) {
                data[row * N + j] = sycl::exp(data[row * N + j] - maxVal);
                sum += data[row * N + j];
            }

            // 归一化
            for (size_t j = 0; j < N; ++j) {
                data[row * N + j] /= sum;
            }
        });
    }).wait();
}

// ReLU激活函数
template<typename T>
void relu(sycl::queue& q, T* data, size_t n) {
    q.parallel_for(sycl::range<1>(n), [=](sycl::id<1> i) {
        data[i] = sycl::max(data[i], T(0));
    }).wait();
}

// Sigmoid激活函数
template<typename T>
void sigmoid(sycl::queue& q, T* data, size_t n) {
    q.parallel_for(sycl::range<1>(n), [=](sycl::id<1> i) {
        data[i] = T(1) / (T(1) + sycl::exp(-data[i]));
    }).wait();
}

// 批量归一化（简化版本）
template<typename T>
void batchNorm(sycl::queue& q,
               T* data, size_t batchSize, size_t features,
               const T* gamma, const T* beta,
               T epsilon = 1e-5) {
    // 计算每个特征的均值和方差
    T* mean = sycl::malloc_shared<T>(features, q);
    T* variance = sycl::malloc_shared<T>(features, q);

    // 计算均值
    q.parallel_for(sycl::range<1>(features), [=](sycl::id<1> f) {
        T sum = 0;
        for (size_t b = 0; b < batchSize; ++b) {
            sum += data[b * features + f];
        }
        mean[f] = sum / batchSize;
    }).wait();

    // 计算方差
    q.parallel_for(sycl::range<1>(features), [=](sycl::id<1> f) {
        T sum = 0;
        for (size_t b = 0; b < batchSize; ++b) {
            T diff = data[b * features + f] - mean[f];
            sum += diff * diff;
        }
        variance[f] = sum / batchSize;
    }).wait();

    // 归一化
    q.parallel_for(sycl::range<2>(batchSize, features), [=](sycl::id<2> id) {
        size_t b = id[0];
        size_t f = id[1];
        T normalized = (data[b * features + f] - mean[f]) /
                       sycl::sqrt(variance[f] + epsilon);
        data[b * features + f] = gamma[f] * normalized + beta[f];
    }).wait();

    sycl::free(mean, q);
    sycl::free(variance, q);
}

} // namespace sycl_math
```

#### 5. 基准测试 (main.cpp)

```cpp
#include "sycl_utils/device.hpp"
#include "sycl_math/matrix_ops.hpp"
#include "sycl_math/vector_ops.hpp"
#include "sycl_math/advanced_ops.hpp"
#include <iostream>
#include <random>

using namespace sycl_utils;
using namespace sycl_math;

void benchmarkMatmul() {
    std::cout << "\n=== Matrix Multiplication Benchmark ===\n\n";

    auto q = createQueue();
    printDeviceInfo(q.get_device());
    std::cout << "\n";

    constexpr size_t SIZES[] = {256, 512, 1024, 2048};

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (size_t N : SIZES) {
        Matrix<float> A(q, N, N);
        Matrix<float> B(q, N, N);
        Matrix<float> C(q, N, N);

        // 初始化
        for (size_t i = 0; i < N * N; ++i) {
            A.data()[i] = dist(rng);
            B.data()[i] = dist(rng);
        }

        // 预热
        matmulTiled<float, 16>(q, A, B, C);

        // 基准测试：朴素实现
        Timer timer;
        for (int i = 0; i < 3; ++i) {
            matmulNaive(q, A, B, C);
        }
        double naiveTime = timer.elapsed() / 3.0;

        // 基准测试：分块实现
        timer.reset();
        for (int i = 0; i < 3; ++i) {
            matmulTiled<float, 16>(q, A, B, C);
        }
        double tiledTime = timer.elapsed() / 3.0;

        // 计算GFLOPS
        double flops = 2.0 * N * N * N;  // 乘加算两次
        double naiveGflops = flops / (naiveTime * 1e6);
        double tiledGflops = flops / (tiledTime * 1e6);

        std::cout << "Matrix size: " << N << "x" << N << "\n";
        std::cout << "  Naive:  " << naiveTime << " ms, "
                  << naiveGflops << " GFLOPS\n";
        std::cout << "  Tiled:  " << tiledTime << " ms, "
                  << tiledGflops << " GFLOPS\n";
        std::cout << "  Speedup: " << naiveTime / tiledTime << "x\n\n";
    }
}

void benchmarkVectorOps() {
    std::cout << "\n=== Vector Operations Benchmark ===\n\n";

    auto q = createQueue();

    constexpr size_t SIZES[] = {1000000, 10000000, 100000000};

    for (size_t N : SIZES) {
        float* a = sycl::malloc_shared<float>(N, q);
        float* b = sycl::malloc_shared<float>(N, q);
        float* c = sycl::malloc_shared<float>(N, q);

        // 初始化
        q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
            a[i] = 1.0f;
            b[i] = 2.0f;
        }).wait();

        // Vector Add
        Timer timer;
        vectorAdd(q, a, b, c, N);
        double addTime = timer.elapsed();

        // Dot Product
        timer.reset();
        float dot = dotProduct(q, a, b, N);
        double dotTime = timer.elapsed();

        // Bandwidth calculation
        double addBandwidth = (3.0 * N * sizeof(float)) / (addTime * 1e6);  // GB/s
        double dotBandwidth = (2.0 * N * sizeof(float)) / (dotTime * 1e6);

        std::cout << "Vector size: " << N << "\n";
        std::cout << "  Add: " << addTime << " ms, "
                  << addBandwidth << " GB/s\n";
        std::cout << "  Dot: " << dotTime << " ms, "
                  << dotBandwidth << " GB/s, result = " << dot << "\n\n";

        sycl::free(a, q);
        sycl::free(b, q);
        sycl::free(c, q);
    }
}

void demoNeuralNetworkLayer() {
    std::cout << "\n=== Neural Network Layer Demo ===\n\n";

    auto q = createQueue();

    constexpr size_t BATCH = 64;
    constexpr size_t INPUT = 784;   // 28x28 MNIST
    constexpr size_t HIDDEN = 256;
    constexpr size_t OUTPUT = 10;

    // 分配权重和偏置
    Matrix<float> W1(q, INPUT, HIDDEN);
    Matrix<float> W2(q, HIDDEN, OUTPUT);
    float* b1 = sycl::malloc_shared<float>(HIDDEN, q);
    float* b2 = sycl::malloc_shared<float>(OUTPUT, q);

    // 输入和中间结果
    Matrix<float> X(q, BATCH, INPUT);
    Matrix<float> H(q, BATCH, HIDDEN);
    Matrix<float> Y(q, BATCH, OUTPUT);

    // 初始化（随机）
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);

    for (size_t i = 0; i < INPUT * HIDDEN; ++i) W1.data()[i] = dist(rng);
    for (size_t i = 0; i < HIDDEN * OUTPUT; ++i) W2.data()[i] = dist(rng);
    for (size_t i = 0; i < HIDDEN; ++i) b1[i] = 0.0f;
    for (size_t i = 0; i < OUTPUT; ++i) b2[i] = 0.0f;

    // 模拟输入
    for (size_t i = 0; i < BATCH * INPUT; ++i) {
        X.data()[i] = dist(rng);
    }

    Timer timer;

    // 前向传播
    // H = X @ W1
    matmulTiled<float, 16>(q, X, W1, H);

    // H += b1 (广播)
    float* h = H.data();
    q.parallel_for(sycl::range<2>(BATCH, HIDDEN), [=](sycl::id<2> id) {
        h[id[0] * HIDDEN + id[1]] += b1[id[1]];
    }).wait();

    // ReLU
    relu(q, H.data(), BATCH * HIDDEN);

    // Y = H @ W2
    matmulTiled<float, 16>(q, H, W2, Y);

    // Y += b2
    float* y = Y.data();
    q.parallel_for(sycl::range<2>(BATCH, OUTPUT), [=](sycl::id<2> id) {
        y[id[0] * OUTPUT + id[1]] += b2[id[1]];
    }).wait();

    // Softmax
    softmax(q, Y);

    double elapsed = timer.elapsed();

    std::cout << "Two-layer neural network forward pass:\n";
    std::cout << "  Batch size: " << BATCH << "\n";
    std::cout << "  Architecture: " << INPUT << " -> " << HIDDEN
              << " -> " << OUTPUT << "\n";
    std::cout << "  Time: " << elapsed << " ms\n";
    std::cout << "  Throughput: " << (BATCH * 1000.0 / elapsed)
              << " samples/sec\n";

    // 打印一些输出
    std::cout << "\n  Sample output (first 3 classes of first sample):\n    ";
    for (int i = 0; i < 3; ++i) {
        std::cout << Y(0, i) << " ";
    }
    std::cout << "...\n";

    sycl::free(b1, q);
    sycl::free(b2, q);
}

int main() {
    try {
        std::cout << "SYCL Math Library Demo\n";
        std::cout << "======================\n";

        listAllDevices();

        benchmarkVectorOps();
        benchmarkMatmul();
        demoNeuralNetworkLayer();

    } catch (const sycl::exception& e) {
        handleException(e);
        return 1;
    }

    return 0;
}
```

---

## 检验标准

### 知识检验
1. [ ] 能够解释CPU和GPU的架构差异
2. [ ] 理解SYCL的执行模型和内存模型
3. [ ] 掌握Buffer/Accessor和USM的使用场景
4. [ ] 理解工作组和工作项的概念
5. [ ] 能够分析SYCL程序的性能瓶颈

### 实践检验
1. [ ] 完成SYCL环境搭建
2. [ ] 实现向量运算库
3. [ ] 实现分块矩阵乘法并获得加速
4. [ ] 归约操作正确实现
5. [ ] 在GPU上运行并获得性能提升

### 代码质量
1. [ ] 正确处理内存分配和释放
2. [ ] 异常处理完善
3. [ ] 代码可在不同SYCL实现上编译
4. [ ] 有完整的基准测试

---

## 输出物清单

1. **学习笔记**
   - [ ] 异构计算基础笔记
   - [ ] SYCL编程模型总结
   - [ ] 优化技术文档

2. **代码产出**
   - [ ] SYCL矩阵运算库
   - [ ] 基准测试套件
   - [ ] 示例应用

3. **文档产出**
   - [ ] API文档
   - [ ] 性能调优指南
   - [ ] 环境搭建指南

---

## 时间分配表

| 周次 | 理论学习 | 源码阅读 | 项目实践 | 总计 |
|------|----------|----------|----------|------|
| Week 1 | 18h | 8h | 9h | 35h |
| Week 2 | 12h | 8h | 15h | 35h |
| Week 3 | 10h | 5h | 20h | 35h |
| Week 4 | 5h | 5h | 25h | 35h |
| **总计** | **45h** | **26h** | **69h** | **140h** |

---

## 下月预告

**Month 55: GPU编程基础**

下个月将深入GPU编程：
- CUDA/HIP编程模型
- GPU内存层次详解
- Warp/Wavefront执行
- 高级优化技术
- 实践项目：实现高性能卷积

建议提前：
1. 如果有NVIDIA GPU，安装CUDA工具包
2. 了解GPU硬件架构（SM、寄存器、共享内存）
3. 复习并行算法基础
