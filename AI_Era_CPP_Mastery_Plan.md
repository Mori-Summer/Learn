# AI 时代 C++ 架构师一年进阶指南

## 目标
将一位拥有 3 年经验的 C++ 开发者（具备基础语法能力，缺乏并发/架构/AI 经验）转型为**资深 AI 系统架构师**。
**核心重点**：填补**多线程**、**系统架构**和**大型项目设计**的空白，同时深度融合 **AI 工程化**技能。

---

## 第一阶段：突破资深瓶颈 - 并发与架构 (第 1-3 个月)
**目标**：补齐阻碍你驾驭大型项目的关键短板。

### 第 1 个月：高阶并发与并行编程 (核心痛点)
*   **理论学习**：
    *   深入理解 C++ 内存模型 (Memory Model)，`std::atomic` 与内存序 (Acquire/Release)。
    *   互斥锁 (Mutex)、条件变量与死锁避免模式。
    *   无锁编程 (Lock-free Programming) 基础与 CAS 操作。
    *   C++20 协程 (Coroutines) 原理与实战。
*   **推荐资源**：
    *   书籍：《C++ Concurrency in Action, 2nd Edition》 (必读)
*   **实战项目**：
    *   **构建高性能线程池 (Thread Pool)**：支持任务窃取 (Work Stealing) 和优先级队列。
    *   **任务调度器**：基于协程实现一个简单的 M:N 调度器。

### 第 2 个月：设计模式与现代 C++ 惯用法
*   **理论学习**：
    *   GoF 设计模式的现代 C++ 实现 (Factory, Observer, Strategy 等)。
    *   关键惯用法：RAII (资源获取即初始化), Pimpl (指针实现), Type Erasure (类型擦除), CRTP (奇异递归模板模式)。
    *   SOLID 原则在 C++ 中的应用。
*   **实战项目**：
    *   **重构练习**：选取一个开源项目的旧模块，用现代 C++ 风格和整洁架构原则进行重构。

### 第 3 个月：网络编程与异步 I/O
*   **理论学习**：
    *   Socket 编程深入：TCP/IP 状态机，Nagle 算法，Keepalive。
    *   高性能 I/O 模型：Reactor vs Proactor，epoll (Linux) / IOCP (Windows)。
    *   现代 RPC 框架：gRPC 原理与 Protocol Buffers。
*   **实战项目**：
    *   **C10k HTTP 服务器**：从零实现一个基于 epoll/IOCP 的异步 HTTP 服务器，进行压力测试并优化。

---

## 第二阶段：系统架构师视角 - 大规模系统设计 (第 4-6 个月)
**目标**：学习如何组织和构建无法由单人完成的大型系统。

### 第 4 个月：构建系统与工程化工具
*   **理论学习**：
    *   CMake 高级用法 (Target-based design, Generator expressions)。
    *   包管理器实战：Conan 或 vcpkg 的企业级配置。
    *   CI/CD 流水线搭建 (GitHub Actions / Jenkins)。
    *   代码静态分析与 Sanitizers (ASan, TSan) 的使用。
*   **实战项目**：
    *   为你的 HTTP 服务器搭建完整的自动化构建、测试和发布流水线。

### 第 5 个月：分布式系统基础
*   **理论学习**：
    *   微服务 vs 单体架构：权衡与选择。
    *   消息队列原理：Kafka / RabbitMQ 的核心概念。
    *   分布式一致性：CAP 定理，BASE 理论，Raft 共识算法。
*   **实战项目**：
    *   **分布式键值存储 (Distributed KV Store)**：实现一个基于 Raft 算法的简易 KV 存储集群，支持节点故障自动恢复。

### 第 6 个月：数据库内核与存储引擎
*   **理论学习**：
    *   存储模型：B+树 vs LSM-Tree (Log Structured Merge Tree)。
    *   预写式日志 (WAL) 与故障恢复机制。
    *   ACID 事务的实现原理。
*   **实战项目**：
    *   **手写存储引擎**：从零实现一个基于 LSM-Tree 的持久化存储引擎，支持基本的 Put/Get/Scan 操作。

---

## 第三阶段：AI 工程化 - 拥抱新栈 (第 7-9 个月)
**目标**：祛魅 AI，学习如何**工程化** AI 系统，而不仅仅是调用 API。

### 第 7 个月：C++ 开发者的数学与 Python 桥梁
*   **理论学习**：
    *   AI 必备数学：线性代数核心 (矩阵运算)，微积分回顾。
    *   Python/C++ 互操作：深入学习 `pybind11`，理解 Python GIL。
    *   NumPy 内部实现原理分析。
*   **实战项目**：
    *   编写一个 Python 扩展模块，用 C++ 加速 Python 的密集计算任务。

### 第 8 个月：推理引擎架构 (Inference Engine)
*   **理论学习**：
    *   大模型原理：Transformer 架构详解 (Self-Attention, FFN)。
    *   模型量化 (Quantization)：Int8, FP16 及其对精度的影响。
    *   KV-Cache 优化原理。
*   **实战项目**：
    *   **纯 C++ 推理实现**：不依赖重型框架，手动实现一个小规模神经网络的前向传播 (Forward Pass)。

### 第 9 个月：高性能 AI 计算
*   **理论学习**：
    *   SIMD 编程：AVX2 / AVX-512 指令集优化。
    *   GPU 编程入门：CUDA 核心概念 (Kernel, Thread, Block, Grid)。
    *   TensorRT 基础与模型加速。
*   **实战项目**：
    *   **算子优化**：使用 SIMD 指令集优化你的矩阵乘法 (GEMM) 算子，争取达到 OpenBLAS 性能的 50% 以上。

---

## 第四阶段：AI 架构师 - 构建解决方案 (第 10-12 个月)
**目标**：将 C++ 的极致性能与 AI 能力结合，构建端到端解决方案。

### 第 10 个月：RAG 与向量检索
*   **理论学习**：
    *   高维向量检索算法：HNSW (Hierarchical Navigable Small World)。
    *   向量数据库架构 (Milvus / Faiss 源码分析)。
    *   Embedding 流水线设计。
*   **实战项目**：
    *   **高性能向量搜索引擎**：基于 C++ 实现一个支持 HNSW 索引的向量检索引擎。

### 第 11 个月：LLM 基础设施与服务化
*   **理论学习**：
    *   LLM 服务化挑战：Continuous Batching (连续批处理)。
    *   显存管理：PagedAttention (vLLM 核心机制)。
    *   模型服务架构设计。
*   **实战项目**：
    *   **Mini LLM Serving**：构建一个支持简单的连续批处理的 LLM 推理服务。

### 第 12 个月：毕业设计 - 本地 AI Agent 运行时
*   **综合项目**：
    *   设计并实现一个**本地 AI Agent 运行时 (Runtime)**。
    *   **功能要求**：
        1.  管理本地 LLM 的加载与推理 (对接 llama.cpp 或自研引擎)。
        2.  实现“工具调用” (Tool Use) 机制，允许 AI 执行本地 C++ 函数。
        3.  管理 Agent 的短期记忆 (Context) 和长期记忆 (Vector Store)。
    *   这将是你转型架构师的**投名状**。

---

## 学习建议
1.  **坚持代码量**：架构能力是写出来的，不是看出来的。每个月的实战项目务必亲手实现。
2.  **阅读源码**：多阅读优秀开源项目 (如 folly, abseil-cpp, llama.cpp, ClickHouse) 的源码。
3.  **保持耐心**：前三个月的基础夯实最为痛苦，但也是回报最高的阶段。
