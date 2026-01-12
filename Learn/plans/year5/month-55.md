# Month 55: GPU编程基础 (GPU Programming Fundamentals)

## 本月主题概述

在上个月学习SYCL的基础上，本月将深入GPU编程的底层细节。我们将学习CUDA/HIP编程模型，理解GPU硬件架构，掌握内存优化技术，并实现高性能GPU内核。无论是AI训练、科学计算还是图形渲染，GPU编程已成为高性能计算的核心技能。

### 学习目标
- 深入理解GPU硬件架构
- 掌握CUDA/HIP编程模型
- 理解线程执行和调度机制
- 掌握GPU内存优化技术
- 实现高性能计算内核

---

## 理论学习内容

### 第一周：GPU架构深入

#### 阅读材料
1. 《CUDA C Programming Guide》
2. 《Professional CUDA C Programming》- Chapter 1-3
3. NVIDIA GPU架构白皮书（Ampere/Hopper）
4. AMD CDNA架构文档

#### 核心概念

**GPU硬件架构**
```
┌─────────────────────────────────────────────────────────┐
│                    GPU架构 (NVIDIA)                      │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                         GPC                              │
│  (Graphics Processing Cluster)                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │                    TPC                           │   │
│  │  (Texture Processing Cluster)                   │   │
│  │  ┌─────────────┐  ┌─────────────┐              │   │
│  │  │     SM      │  │     SM      │   ...        │   │
│  │  │  (Streaming │  │  (Streaming │              │   │
│  │  │ Multiproc.) │  │ Multiproc.) │              │   │
│  │  └─────────────┘  └─────────────┘              │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘

SM (Streaming Multiprocessor) 详解：
┌─────────────────────────────────────────────────────────┐
│                         SM                               │
│  ┌──────────────────────────────────────────────────┐  │
│  │           Warp Scheduler × 4                      │  │
│  │  (每个调度器可以每周期发射一条指令)                  │  │
│  └──────────────────────────────────────────────────┘  │
│                                                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│  │Processing│ │Processing│ │Processing│ │Processing│  │
│  │Block     │ │Block     │ │Block     │ │Block     │  │
│  │ 32 CUDA  │ │ 32 CUDA  │ │ 32 CUDA  │ │ 32 CUDA  │  │
│  │ Cores    │ │ Cores    │ │ Cores    │ │ Cores    │  │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘  │
│                                                         │
│  ┌──────────────────────────────────────────────────┐  │
│  │           Register File (64KB)                    │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │      Shared Memory / L1 Cache (128KB)            │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │           Tensor Cores (用于矩阵运算)              │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘

内存层次：
┌────────────────────────────────────────────────────────┐
│  Registers      │ ~1 cycle    │ 线程私有，最快        │
├────────────────────────────────────────────────────────┤
│  Shared Memory  │ ~5 cycles   │ 块内共享，可编程      │
├────────────────────────────────────────────────────────┤
│  L1 Cache       │ ~30 cycles  │ SM本地               │
├────────────────────────────────────────────────────────┤
│  L2 Cache       │ ~200 cycles │ 全局共享             │
├────────────────────────────────────────────────────────┤
│  Global Memory  │ ~400 cycles │ HBM/GDDR，最大容量   │
└────────────────────────────────────────────────────────┘
```

**SIMT执行模型**
```cpp
// SIMT: Single Instruction Multiple Threads
// 一个Warp = 32个线程，执行相同指令

// 示例：分支分歧
__global__ void branchDivergence(float* data, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // 分支分歧！同一Warp的线程走不同路径
    if (idx % 2 == 0) {
        // 偶数线程执行这里
        data[idx] = expensive_op_a(data[idx]);  // ~10 cycles
    } else {
        // 奇数线程执行这里（被串行化）
        data[idx] = expensive_op_b(data[idx]);  // ~10 cycles
    }
    // 总时间：~20 cycles（而不是10）
}

// 优化：基于Warp的分支
__global__ void noDivergence(float* data, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int warpId = idx / 32;

    // 整个Warp走同一分支
    if (warpId % 2 == 0) {
        data[idx] = expensive_op_a(data[idx]);
    } else {
        data[idx] = expensive_op_b(data[idx]);
    }
    // 总时间：~10 cycles
}
```

### 第二周：CUDA/HIP编程模型

#### 阅读材料
1. CUDA Best Practices Guide
2. HIP编程指南
3. PTX ISA文档

#### 核心概念

**线程层次**
```cuda
// 线程层次结构
//
// Grid (网格)
//   └── Block (线程块)
//         └── Thread (线程)
//               └── Warp (线程束，硬件调度单位)

// 内置变量
threadIdx.x, threadIdx.y, threadIdx.z  // 块内线程ID
blockIdx.x, blockIdx.y, blockIdx.z     // 块ID
blockDim.x, blockDim.y, blockDim.z     // 块尺寸
gridDim.x, gridDim.y, gridDim.z        // 网格尺寸

// 计算全局ID
int globalIdx = blockIdx.x * blockDim.x + threadIdx.x;
int globalIdy = blockIdx.y * blockDim.y + threadIdx.y;

// 计算线性索引（2D）
int linearIdx = globalIdy * width + globalIdx;

// 内核启动
dim3 gridSize(16, 16);     // 16x16 = 256 blocks
dim3 blockSize(16, 16);    // 16x16 = 256 threads per block
myKernel<<<gridSize, blockSize>>>(args...);

// 共享内存
__shared__ float sharedData[256];

// 同步
__syncthreads();  // 块内同步
```

**CUDA基础示例**
```cuda
#include <cuda_runtime.h>
#include <iostream>

// 错误检查宏
#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "CUDA error at " << __FILE__ << ":" << __LINE__ \
                      << " - " << cudaGetErrorString(err) << std::endl; \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

// 向量加法内核
__global__ void vectorAdd(const float* a, const float* b, float* c, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < n) {
        c[idx] = a[idx] + b[idx];
    }
}

int main() {
    const int N = 1 << 20;  // 1M elements
    const int bytes = N * sizeof(float);

    // 分配主机内存
    float* h_a = new float[N];
    float* h_b = new float[N];
    float* h_c = new float[N];

    // 初始化
    for (int i = 0; i < N; ++i) {
        h_a[i] = 1.0f;
        h_b[i] = 2.0f;
    }

    // 分配设备内存
    float *d_a, *d_b, *d_c;
    CUDA_CHECK(cudaMalloc(&d_a, bytes));
    CUDA_CHECK(cudaMalloc(&d_b, bytes));
    CUDA_CHECK(cudaMalloc(&d_c, bytes));

    // 复制到设备
    CUDA_CHECK(cudaMemcpy(d_a, h_a, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_b, h_b, bytes, cudaMemcpyHostToDevice));

    // 启动内核
    int blockSize = 256;
    int gridSize = (N + blockSize - 1) / blockSize;

    vectorAdd<<<gridSize, blockSize>>>(d_a, d_b, d_c, N);
    CUDA_CHECK(cudaGetLastError());

    // 复制回主机
    CUDA_CHECK(cudaMemcpy(h_c, d_c, bytes, cudaMemcpyDeviceToHost));

    // 验证
    bool correct = true;
    for (int i = 0; i < N; ++i) {
        if (h_c[i] != 3.0f) {
            correct = false;
            break;
        }
    }
    std::cout << "Result: " << (correct ? "PASS" : "FAIL") << std::endl;

    // 清理
    CUDA_CHECK(cudaFree(d_a));
    CUDA_CHECK(cudaFree(d_b));
    CUDA_CHECK(cudaFree(d_c));
    delete[] h_a;
    delete[] h_b;
    delete[] h_c;

    return 0;
}
```

### 第三周：内存优化

#### 阅读材料
1. CUDA Memory Management Guide
2. 内存合并访问模式
3. 共享内存银行冲突
4. 常量内存和纹理内存

#### 核心概念

**内存合并访问**
```cuda
// 全局内存以32、64或128字节事务访问
// 同一Warp的线程应该访问连续内存

// 好：合并访问（连续）
__global__ void coalescedAccess(float* data, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    // Warp内的线程访问连续地址
    // Thread 0: data[0], Thread 1: data[1], ...
    float val = data[idx];
}

// 差：跨步访问
__global__ void stridedAccess(float* data, int n, int stride) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    // Thread 0: data[0], Thread 1: data[stride], Thread 2: data[2*stride]...
    float val = data[idx * stride];  // 多次内存事务
}

// 差：随机访问
__global__ void randomAccess(float* data, int* indices, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    // 每个线程访问随机位置
    float val = data[indices[idx]];  // 最坏情况：32次内存事务
}

// AoS vs SoA
struct ParticleAoS {
    float x, y, z;
    float vx, vy, vz;
};

// 差：AoS布局
__global__ void updateAoS(ParticleAoS* particles, float dt, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        particles[idx].x += particles[idx].vx * dt;  // 非连续
    }
}

// 好：SoA布局
__global__ void updateSoA(float* x, float* vx, float dt, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        x[idx] += vx[idx] * dt;  // 连续访问
    }
}
```

**共享内存使用**
```cuda
// 共享内存有32个bank
// 连续的32位字映射到连续的bank
// Bank冲突会导致串行访问

// 避免Bank冲突
__global__ void matrixTranspose(float* output, const float* input,
                                 int width, int height) {
    // 使用共享内存做分块转置
    __shared__ float tile[32][33];  // 33列避免bank冲突！

    int x = blockIdx.x * 32 + threadIdx.x;
    int y = blockIdx.y * 32 + threadIdx.y;

    // 读取到共享内存
    if (x < width && y < height) {
        tile[threadIdx.y][threadIdx.x] = input[y * width + x];
    }

    __syncthreads();

    // 计算转置后的坐标
    x = blockIdx.y * 32 + threadIdx.x;
    y = blockIdx.x * 32 + threadIdx.y;

    // 写回
    if (x < height && y < width) {
        output[y * height + x] = tile[threadIdx.x][threadIdx.y];
    }
}
```

### 第四周：高级优化技术

#### 阅读材料
1. CUDA Occupancy Calculator
2. 异步内存传输
3. CUDA Streams
4. Profiling工具使用（Nsight）

#### 核心概念

**占用率优化**
```cuda
// 占用率 = 活动Warp数 / 最大Warp数
// 影响因素：寄存器使用、共享内存使用、块大小

// 查询设备属性
cudaDeviceProp prop;
cudaGetDeviceProperties(&prop, 0);

// 计算最大块数
int numBlocks;
cudaOccupancyMaxActiveBlocksPerMultiprocessor(
    &numBlocks,
    myKernel,
    blockSize,
    sharedMemBytes
);

// 建议的块大小
int minGridSize, blockSize;
cudaOccupancyMaxPotentialBlockSize(
    &minGridSize,
    &blockSize,
    myKernel,
    sharedMemBytes,
    maxBlockSize
);

// 限制寄存器使用
__global__ __launch_bounds__(256, 2)  // 最大256线程，每SM最少2个块
void myKernel(...) {
    // ...
}
```

**CUDA Streams和异步操作**
```cuda
// 创建流
cudaStream_t stream1, stream2;
cudaStreamCreate(&stream1);
cudaStreamCreate(&stream2);

// 异步内存传输和内核执行
cudaMemcpyAsync(d_a, h_a, bytes, cudaMemcpyHostToDevice, stream1);
kernel<<<grid, block, 0, stream1>>>(d_a, d_b);
cudaMemcpyAsync(h_b, d_b, bytes, cudaMemcpyDeviceToHost, stream1);

// 同时在另一个流执行不同工作
cudaMemcpyAsync(d_c, h_c, bytes, cudaMemcpyHostToDevice, stream2);
kernel<<<grid, block, 0, stream2>>>(d_c, d_d);
cudaMemcpyAsync(h_d, d_d, bytes, cudaMemcpyDeviceToHost, stream2);

// 等待所有完成
cudaStreamSynchronize(stream1);
cudaStreamSynchronize(stream2);

// 或等待设备空闲
cudaDeviceSynchronize();

// 销毁流
cudaStreamDestroy(stream1);
cudaStreamDestroy(stream2);
```

---

## 源码阅读任务

### 必读项目

1. **CUTLASS** (https://github.com/NVIDIA/cutlass)
   - 重点：GEMM实现
   - 学习目标：理解高性能矩阵运算
   - 阅读时间：12小时

2. **CUB** (https://github.com/NVIDIA/cub)
   - 重点：归约和排序算法
   - 学习目标：理解GPU并行原语
   - 阅读时间：8小时

3. **Thrust** (https://github.com/NVIDIA/thrust)
   - 重点：高级算法接口
   - 学习目标：理解GPU算法库设计
   - 阅读时间：6小时

---

## 实践项目：高性能卷积实现

### 项目概述
实现高性能的2D卷积操作，这是深度学习和图像处理的核心操作。

### 完整代码实现

#### 1. CUDA工具库 (cuda_utils.cuh)

```cuda
#pragma once

#include <cuda_runtime.h>
#include <iostream>
#include <chrono>

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "CUDA error at " << __FILE__ << ":" << __LINE__ \
                      << " - " << cudaGetErrorString(err) << std::endl; \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

// 设备信息
inline void printDeviceInfo() {
    int deviceCount;
    CUDA_CHECK(cudaGetDeviceCount(&deviceCount));

    for (int i = 0; i < deviceCount; ++i) {
        cudaDeviceProp prop;
        CUDA_CHECK(cudaGetDeviceProperties(&prop, i));

        std::cout << "Device " << i << ": " << prop.name << "\n";
        std::cout << "  Compute capability: " << prop.major << "." << prop.minor << "\n";
        std::cout << "  SMs: " << prop.multiProcessorCount << "\n";
        std::cout << "  Global memory: " << prop.totalGlobalMem / (1024*1024) << " MB\n";
        std::cout << "  Shared memory per block: " << prop.sharedMemPerBlock / 1024 << " KB\n";
        std::cout << "  Max threads per block: " << prop.maxThreadsPerBlock << "\n";
        std::cout << "  Warp size: " << prop.warpSize << "\n";
    }
}

// GPU计时器
class GpuTimer {
    cudaEvent_t start_, stop_;

public:
    GpuTimer() {
        CUDA_CHECK(cudaEventCreate(&start_));
        CUDA_CHECK(cudaEventCreate(&stop_));
    }

    ~GpuTimer() {
        cudaEventDestroy(start_);
        cudaEventDestroy(stop_);
    }

    void start(cudaStream_t stream = 0) {
        CUDA_CHECK(cudaEventRecord(start_, stream));
    }

    void stop(cudaStream_t stream = 0) {
        CUDA_CHECK(cudaEventRecord(stop_, stream));
    }

    float elapsed() {
        CUDA_CHECK(cudaEventSynchronize(stop_));
        float ms;
        CUDA_CHECK(cudaEventElapsedTime(&ms, start_, stop_));
        return ms;
    }
};

// 简单内存管理
template<typename T>
class DeviceArray {
    T* data_;
    size_t size_;

public:
    explicit DeviceArray(size_t size) : size_(size) {
        CUDA_CHECK(cudaMalloc(&data_, size * sizeof(T)));
    }

    ~DeviceArray() {
        if (data_) cudaFree(data_);
    }

    DeviceArray(const DeviceArray&) = delete;
    DeviceArray& operator=(const DeviceArray&) = delete;

    DeviceArray(DeviceArray&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
    }

    T* data() { return data_; }
    const T* data() const { return data_; }
    size_t size() const { return size_; }

    void copyFromHost(const T* host) {
        CUDA_CHECK(cudaMemcpy(data_, host, size_ * sizeof(T), cudaMemcpyHostToDevice));
    }

    void copyToHost(T* host) const {
        CUDA_CHECK(cudaMemcpy(host, data_, size_ * sizeof(T), cudaMemcpyDeviceToHost));
    }

    void zero() {
        CUDA_CHECK(cudaMemset(data_, 0, size_ * sizeof(T)));
    }
};
```

#### 2. 朴素卷积实现 (convolution_naive.cuh)

```cuda
#pragma once

#include "cuda_utils.cuh"

// 朴素卷积内核
__global__ void conv2dNaive(
    const float* input,   // [N, C_in, H, W]
    const float* kernel,  // [C_out, C_in, K, K]
    float* output,        // [N, C_out, H_out, W_out]
    int N, int C_in, int H, int W,
    int C_out, int K,
    int H_out, int W_out,
    int pad, int stride
) {
    // 每个线程计算一个输出元素
    int w_out = blockIdx.x * blockDim.x + threadIdx.x;
    int h_out = blockIdx.y * blockDim.y + threadIdx.y;
    int c_out = blockIdx.z % C_out;
    int n = blockIdx.z / C_out;

    if (w_out >= W_out || h_out >= H_out) return;

    float sum = 0.0f;

    // 卷积
    for (int c_in = 0; c_in < C_in; ++c_in) {
        for (int kh = 0; kh < K; ++kh) {
            for (int kw = 0; kw < K; ++kw) {
                int h_in = h_out * stride - pad + kh;
                int w_in = w_out * stride - pad + kw;

                if (h_in >= 0 && h_in < H && w_in >= 0 && w_in < W) {
                    int input_idx = n * (C_in * H * W) +
                                   c_in * (H * W) +
                                   h_in * W + w_in;

                    int kernel_idx = c_out * (C_in * K * K) +
                                    c_in * (K * K) +
                                    kh * K + kw;

                    sum += input[input_idx] * kernel[kernel_idx];
                }
            }
        }
    }

    int output_idx = n * (C_out * H_out * W_out) +
                    c_out * (H_out * W_out) +
                    h_out * W_out + w_out;

    output[output_idx] = sum;
}

void conv2dNaiveLaunch(
    const float* input,
    const float* kernel,
    float* output,
    int N, int C_in, int H, int W,
    int C_out, int K,
    int pad, int stride
) {
    int H_out = (H + 2 * pad - K) / stride + 1;
    int W_out = (W + 2 * pad - K) / stride + 1;

    dim3 blockDim(16, 16);
    dim3 gridDim(
        (W_out + blockDim.x - 1) / blockDim.x,
        (H_out + blockDim.y - 1) / blockDim.y,
        N * C_out
    );

    conv2dNaive<<<gridDim, blockDim>>>(
        input, kernel, output,
        N, C_in, H, W,
        C_out, K,
        H_out, W_out,
        pad, stride
    );

    CUDA_CHECK(cudaGetLastError());
}
```

#### 3. 共享内存优化卷积 (convolution_shared.cuh)

```cuda
#pragma once

#include "cuda_utils.cuh"

// 使用共享内存的卷积
template<int TILE_W, int TILE_H, int K>
__global__ void conv2dShared(
    const float* input,
    const float* kernel,
    float* output,
    int N, int C_in, int H, int W,
    int C_out,
    int H_out, int W_out,
    int pad, int stride
) {
    // 共享内存tile大小需要包含halo区域
    constexpr int SHARED_W = TILE_W + K - 1;
    constexpr int SHARED_H = TILE_H + K - 1;

    __shared__ float sharedInput[SHARED_H][SHARED_W];
    __shared__ float sharedKernel[K][K];

    int tx = threadIdx.x;
    int ty = threadIdx.y;
    int bx = blockIdx.x * TILE_W;
    int by = blockIdx.y * TILE_H;
    int c_out = blockIdx.z % C_out;
    int n = blockIdx.z / C_out;

    int w_out = bx + tx;
    int h_out = by + ty;

    float sum = 0.0f;

    // 遍历输入通道
    for (int c_in = 0; c_in < C_in; ++c_in) {
        // 协作加载卷积核到共享内存
        if (tx < K && ty < K) {
            int kernel_idx = c_out * (C_in * K * K) +
                            c_in * (K * K) +
                            ty * K + tx;
            sharedKernel[ty][tx] = kernel[kernel_idx];
        }

        // 协作加载输入tile（包含halo）
        for (int i = ty; i < SHARED_H; i += blockDim.y) {
            for (int j = tx; j < SHARED_W; j += blockDim.x) {
                int h_in = by * stride - pad + i;
                int w_in = bx * stride - pad + j;

                float val = 0.0f;
                if (h_in >= 0 && h_in < H && w_in >= 0 && w_in < W) {
                    int input_idx = n * (C_in * H * W) +
                                   c_in * (H * W) +
                                   h_in * W + w_in;
                    val = input[input_idx];
                }
                sharedInput[i][j] = val;
            }
        }

        __syncthreads();

        // 计算卷积
        if (w_out < W_out && h_out < H_out) {
            for (int kh = 0; kh < K; ++kh) {
                for (int kw = 0; kw < K; ++kw) {
                    int sh = ty * stride + kh;
                    int sw = tx * stride + kw;
                    sum += sharedInput[sh][sw] * sharedKernel[kh][kw];
                }
            }
        }

        __syncthreads();
    }

    // 写入输出
    if (w_out < W_out && h_out < H_out) {
        int output_idx = n * (C_out * H_out * W_out) +
                        c_out * (H_out * W_out) +
                        h_out * W_out + w_out;
        output[output_idx] = sum;
    }
}

void conv2dSharedLaunch(
    const float* input,
    const float* kernel,
    float* output,
    int N, int C_in, int H, int W,
    int C_out, int K,
    int pad, int stride
) {
    int H_out = (H + 2 * pad - K) / stride + 1;
    int W_out = (W + 2 * pad - K) / stride + 1;

    constexpr int TILE_W = 16;
    constexpr int TILE_H = 16;

    dim3 blockDim(TILE_W, TILE_H);
    dim3 gridDim(
        (W_out + TILE_W - 1) / TILE_W,
        (H_out + TILE_H - 1) / TILE_H,
        N * C_out
    );

    if (K == 3) {
        conv2dShared<TILE_W, TILE_H, 3><<<gridDim, blockDim>>>(
            input, kernel, output,
            N, C_in, H, W, C_out,
            H_out, W_out, pad, stride
        );
    } else if (K == 5) {
        conv2dShared<TILE_W, TILE_H, 5><<<gridDim, blockDim>>>(
            input, kernel, output,
            N, C_in, H, W, C_out,
            H_out, W_out, pad, stride
        );
    }

    CUDA_CHECK(cudaGetLastError());
}
```

#### 4. Im2Col卷积实现 (convolution_im2col.cuh)

```cuda
#pragma once

#include "cuda_utils.cuh"
#include <cublas_v2.h>

// Im2Col变换：将卷积转换为矩阵乘法
__global__ void im2col(
    const float* input,   // [N, C, H, W]
    float* col,           // [N, C*K*K, H_out*W_out]
    int N, int C, int H, int W,
    int K, int H_out, int W_out,
    int pad, int stride
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = N * C * K * K * H_out * W_out;

    if (idx >= total) return;

    // 解析索引
    int w_out = idx % W_out;
    int h_out = (idx / W_out) % H_out;
    int kw = (idx / (W_out * H_out)) % K;
    int kh = (idx / (W_out * H_out * K)) % K;
    int c = (idx / (W_out * H_out * K * K)) % C;
    int n = idx / (W_out * H_out * K * K * C);

    int h_in = h_out * stride - pad + kh;
    int w_in = w_out * stride - pad + kw;

    float val = 0.0f;
    if (h_in >= 0 && h_in < H && w_in >= 0 && w_in < W) {
        val = input[n * (C * H * W) + c * (H * W) + h_in * W + w_in];
    }

    // col layout: [N, C*K*K, H_out*W_out]
    int col_c = c * K * K + kh * K + kw;
    int col_idx = n * (C * K * K * H_out * W_out) +
                  col_c * (H_out * W_out) +
                  h_out * W_out + w_out;

    col[col_idx] = val;
}

// Col2Im（用于反向传播）
__global__ void col2im(
    const float* col,
    float* input,
    int N, int C, int H, int W,
    int K, int H_out, int W_out,
    int pad, int stride
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = N * C * H * W;

    if (idx >= total) return;

    int w = idx % W;
    int h = (idx / W) % H;
    int c = (idx / (W * H)) % C;
    int n = idx / (W * H * C);

    float sum = 0.0f;

    // 找到所有对该位置有贡献的输出
    for (int kh = 0; kh < K; ++kh) {
        for (int kw = 0; kw < K; ++kw) {
            int h_out = (h + pad - kh);
            int w_out = (w + pad - kw);

            if (h_out % stride == 0 && w_out % stride == 0) {
                h_out /= stride;
                w_out /= stride;

                if (h_out >= 0 && h_out < H_out && w_out >= 0 && w_out < W_out) {
                    int col_c = c * K * K + kh * K + kw;
                    int col_idx = n * (C * K * K * H_out * W_out) +
                                  col_c * (H_out * W_out) +
                                  h_out * W_out + w_out;
                    sum += col[col_idx];
                }
            }
        }
    }

    input[idx] = sum;
}

class Conv2dIm2Col {
    cublasHandle_t handle_;
    float* col_buffer_;
    size_t col_buffer_size_;

public:
    Conv2dIm2Col() : col_buffer_(nullptr), col_buffer_size_(0) {
        cublasCreate(&handle_);
    }

    ~Conv2dIm2Col() {
        if (col_buffer_) cudaFree(col_buffer_);
        cublasDestroy(handle_);
    }

    void forward(
        const float* input,   // [N, C_in, H, W]
        const float* weight,  // [C_out, C_in, K, K]
        float* output,        // [N, C_out, H_out, W_out]
        int N, int C_in, int H, int W,
        int C_out, int K,
        int pad, int stride
    ) {
        int H_out = (H + 2 * pad - K) / stride + 1;
        int W_out = (W + 2 * pad - K) / stride + 1;

        // 分配col buffer
        size_t required_size = N * C_in * K * K * H_out * W_out;
        if (required_size > col_buffer_size_) {
            if (col_buffer_) cudaFree(col_buffer_);
            CUDA_CHECK(cudaMalloc(&col_buffer_, required_size * sizeof(float)));
            col_buffer_size_ = required_size;
        }

        // Im2Col变换
        int total_elements = N * C_in * K * K * H_out * W_out;
        int blockSize = 256;
        int gridSize = (total_elements + blockSize - 1) / blockSize;

        im2col<<<gridSize, blockSize>>>(
            input, col_buffer_,
            N, C_in, H, W,
            K, H_out, W_out,
            pad, stride
        );
        CUDA_CHECK(cudaGetLastError());

        // 使用cuBLAS进行矩阵乘法
        // output = weight * col
        // [C_out, H_out*W_out] = [C_out, C_in*K*K] * [C_in*K*K, H_out*W_out]
        float alpha = 1.0f;
        float beta = 0.0f;

        for (int n = 0; n < N; ++n) {
            cublasSgemm(
                handle_,
                CUBLAS_OP_N, CUBLAS_OP_N,
                H_out * W_out,  // M
                C_out,          // N
                C_in * K * K,   // K
                &alpha,
                col_buffer_ + n * C_in * K * K * H_out * W_out,  // A
                H_out * W_out,
                weight,  // B
                C_in * K * K,
                &beta,
                output + n * C_out * H_out * W_out,  // C
                H_out * W_out
            );
        }
    }
};
```

#### 5. Winograd卷积 (convolution_winograd.cuh)

```cuda
#pragma once

#include "cuda_utils.cuh"

// Winograd F(2x2, 3x3)变换矩阵
// 输出tile 2x2，卷积核 3x3
// G = 变换卷积核的矩阵
// B = 变换输入的矩阵
// A = 变换输出的矩阵

// 变换矩阵（存储在常量内存）
__constant__ float G[4][3] = {
    {1.0f, 0.0f, 0.0f},
    {0.5f, 0.5f, 0.5f},
    {0.5f, -0.5f, 0.5f},
    {0.0f, 0.0f, 1.0f}
};

__constant__ float Gt[3][4] = {
    {1.0f, 0.5f, 0.5f, 0.0f},
    {0.0f, 0.5f, -0.5f, 0.0f},
    {0.0f, 0.5f, 0.5f, 1.0f}
};

__constant__ float B[4][4] = {
    {1.0f, 0.0f, -1.0f, 0.0f},
    {0.0f, 1.0f, 1.0f, 0.0f},
    {0.0f, -1.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, -1.0f}
};

__constant__ float Bt[4][4] = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, -1.0f}
};

__constant__ float A[2][4] = {
    {1.0f, 1.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, -1.0f, -1.0f}
};

__constant__ float At[4][2] = {
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {1.0f, -1.0f},
    {0.0f, -1.0f}
};

// 变换卷积核: U = G * g * G^T
__global__ void transformKernel(
    const float* kernel,  // [C_out, C_in, 3, 3]
    float* U,             // [4, 4, C_out, C_in]
    int C_out, int C_in
) {
    int c_out = blockIdx.x * blockDim.x + threadIdx.x;
    int c_in = blockIdx.y * blockDim.y + threadIdx.y;

    if (c_out >= C_out || c_in >= C_in) return;

    // 加载3x3卷积核
    float g[3][3];
    int base = c_out * (C_in * 9) + c_in * 9;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            g[i][j] = kernel[base + i * 3 + j];
        }
    }

    // 计算 temp = G * g
    float temp[4][3];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            temp[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                temp[i][j] += G[i][k] * g[k][j];
            }
        }
    }

    // 计算 U = temp * G^T
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float val = 0.0f;
            for (int k = 0; k < 3; ++k) {
                val += temp[i][k] * Gt[k][j];
            }
            // 存储格式: [4, 4, C_out, C_in]
            int idx = i * (4 * C_out * C_in) + j * (C_out * C_in) +
                     c_out * C_in + c_in;
            U[idx] = val;
        }
    }
}

// Winograd卷积的完整实现较复杂，这里给出框架
class Conv2dWinograd {
    float* U_;  // 变换后的卷积核
    bool kernel_transformed_;

public:
    Conv2dWinograd() : U_(nullptr), kernel_transformed_(false) {}

    ~Conv2dWinograd() {
        if (U_) cudaFree(U_);
    }

    void transformKernels(const float* kernel, int C_out, int C_in) {
        if (U_) cudaFree(U_);
        CUDA_CHECK(cudaMalloc(&U_, 16 * C_out * C_in * sizeof(float)));

        dim3 blockDim(16, 16);
        dim3 gridDim(
            (C_out + 15) / 16,
            (C_in + 15) / 16
        );

        transformKernel<<<gridDim, blockDim>>>(kernel, U_, C_out, C_in);
        CUDA_CHECK(cudaGetLastError());

        kernel_transformed_ = true;
    }

    // forward实现需要:
    // 1. 将输入分块并变换 (B^T * d * B)
    // 2. 逐元素乘法 (U ⊙ V)
    // 3. 反变换得到输出 (A^T * (U ⊙ V) * A)
};
```

#### 6. 基准测试 (main.cu)

```cuda
#include "cuda_utils.cuh"
#include "convolution_naive.cuh"
#include "convolution_shared.cuh"
#include "convolution_im2col.cuh"
#include <iostream>
#include <vector>
#include <random>

void benchmark() {
    printDeviceInfo();
    std::cout << "\n=== Convolution Benchmark ===\n\n";

    // 测试配置
    struct Config {
        int N, C_in, H, W, C_out, K;
    };

    std::vector<Config> configs = {
        {1, 64, 56, 56, 64, 3},     // ResNet conv1
        {1, 128, 28, 28, 128, 3},   // ResNet conv2
        {1, 256, 14, 14, 256, 3},   // ResNet conv3
        {1, 512, 7, 7, 512, 3},     // ResNet conv4
        {32, 64, 56, 56, 64, 3},    // Batch=32
    };

    int pad = 1;
    int stride = 1;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (const auto& cfg : configs) {
        int H_out = (cfg.H + 2 * pad - cfg.K) / stride + 1;
        int W_out = (cfg.W + 2 * pad - cfg.K) / stride + 1;

        size_t input_size = cfg.N * cfg.C_in * cfg.H * cfg.W;
        size_t kernel_size = cfg.C_out * cfg.C_in * cfg.K * cfg.K;
        size_t output_size = cfg.N * cfg.C_out * H_out * W_out;

        // 分配内存
        std::vector<float> h_input(input_size);
        std::vector<float> h_kernel(kernel_size);
        std::vector<float> h_output(output_size);

        for (auto& v : h_input) v = dist(rng);
        for (auto& v : h_kernel) v = dist(rng);

        DeviceArray<float> d_input(input_size);
        DeviceArray<float> d_kernel(kernel_size);
        DeviceArray<float> d_output(output_size);

        d_input.copyFromHost(h_input.data());
        d_kernel.copyFromHost(h_kernel.data());

        GpuTimer timer;
        constexpr int WARMUP = 5;
        constexpr int ITERATIONS = 20;

        std::cout << "Config: N=" << cfg.N << ", C_in=" << cfg.C_in
                  << ", H=" << cfg.H << ", W=" << cfg.W
                  << ", C_out=" << cfg.C_out << ", K=" << cfg.K << "\n";

        // 朴素实现
        for (int i = 0; i < WARMUP; ++i) {
            conv2dNaiveLaunch(
                d_input.data(), d_kernel.data(), d_output.data(),
                cfg.N, cfg.C_in, cfg.H, cfg.W,
                cfg.C_out, cfg.K, pad, stride
            );
        }
        cudaDeviceSynchronize();

        timer.start();
        for (int i = 0; i < ITERATIONS; ++i) {
            conv2dNaiveLaunch(
                d_input.data(), d_kernel.data(), d_output.data(),
                cfg.N, cfg.C_in, cfg.H, cfg.W,
                cfg.C_out, cfg.K, pad, stride
            );
        }
        timer.stop();
        float naiveTime = timer.elapsed() / ITERATIONS;

        // 共享内存实现
        for (int i = 0; i < WARMUP; ++i) {
            conv2dSharedLaunch(
                d_input.data(), d_kernel.data(), d_output.data(),
                cfg.N, cfg.C_in, cfg.H, cfg.W,
                cfg.C_out, cfg.K, pad, stride
            );
        }
        cudaDeviceSynchronize();

        timer.start();
        for (int i = 0; i < ITERATIONS; ++i) {
            conv2dSharedLaunch(
                d_input.data(), d_kernel.data(), d_output.data(),
                cfg.N, cfg.C_in, cfg.H, cfg.W,
                cfg.C_out, cfg.K, pad, stride
            );
        }
        timer.stop();
        float sharedTime = timer.elapsed() / ITERATIONS;

        // Im2Col实现
        Conv2dIm2Col im2col;
        for (int i = 0; i < WARMUP; ++i) {
            im2col.forward(
                d_input.data(), d_kernel.data(), d_output.data(),
                cfg.N, cfg.C_in, cfg.H, cfg.W,
                cfg.C_out, cfg.K, pad, stride
            );
        }
        cudaDeviceSynchronize();

        timer.start();
        for (int i = 0; i < ITERATIONS; ++i) {
            im2col.forward(
                d_input.data(), d_kernel.data(), d_output.data(),
                cfg.N, cfg.C_in, cfg.H, cfg.W,
                cfg.C_out, cfg.K, pad, stride
            );
        }
        timer.stop();
        float im2colTime = timer.elapsed() / ITERATIONS;

        // 计算TFLOPS
        double flops = 2.0 * cfg.N * cfg.C_out * cfg.C_in *
                       cfg.K * cfg.K * H_out * W_out;
        double naiveTflops = flops / (naiveTime * 1e9);
        double sharedTflops = flops / (sharedTime * 1e9);
        double im2colTflops = flops / (im2colTime * 1e9);

        std::cout << "  Naive:  " << naiveTime << " ms, "
                  << naiveTflops << " TFLOPS\n";
        std::cout << "  Shared: " << sharedTime << " ms, "
                  << sharedTflops << " TFLOPS\n";
        std::cout << "  Im2Col: " << im2colTime << " ms, "
                  << im2colTflops << " TFLOPS\n";
        std::cout << "  Speedup (Shared/Naive): "
                  << naiveTime / sharedTime << "x\n";
        std::cout << "  Speedup (Im2Col/Naive): "
                  << naiveTime / im2colTime << "x\n\n";
    }
}

int main() {
    benchmark();
    return 0;
}
```

---

## 检验标准

### 知识检验
1. [ ] 能够解释GPU的SM架构和Warp执行模型
2. [ ] 理解内存层次和访问延迟
3. [ ] 掌握内存合并访问的条件和优化方法
4. [ ] 理解共享内存bank冲突
5. [ ] 能够分析内核的占用率

### 实践检验
1. [ ] 完成CUDA环境搭建
2. [ ] 实现朴素卷积并正确运行
3. [ ] 共享内存优化获得2倍以上加速
4. [ ] Im2Col实现能利用cuBLAS
5. [ ] 使用Nsight进行性能分析

### 代码质量
1. [ ] 正确的错误检查
2. [ ] 无内存泄漏
3. [ ] 代码可配置（块大小等）
4. [ ] 有完整的基准测试

---

## 输出物清单

1. **学习笔记**
   - [ ] GPU架构详解笔记
   - [ ] CUDA编程要点总结
   - [ ] 优化技术文档

2. **代码产出**
   - [ ] 多种卷积实现
   - [ ] 性能基准测试
   - [ ] 可复用的CUDA工具库

3. **分析报告**
   - [ ] 各实现性能对比
   - [ ] Profiling分析报告
   - [ ] 优化建议文档

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

**Month 56: C++23新特性**

下个月将学习C++23带来的新特性：
- std::expected和单子操作
- std::mdspan多维数组视图
- std::generator协程
- std::print格式化输出
- 实践项目：使用C++23重构现有代码

建议提前：
1. 确保编译器支持C++23（GCC 13+, Clang 17+）
2. 复习C++20特性（concepts, ranges, coroutines）
3. 了解函数式编程概念
