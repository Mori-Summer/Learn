# Day 1 总结：高性能线程池 (High-Performance Thread Pool)

## 1. 核心成就
我们成功从零构建了一个**支持 Work Stealing（任务窃取）** 的高性能线程池，并完成了从基础版 (`ThreadPool`) 到进阶版 (`ThreadPoolFast`) 的迭代。

### 关键特性
*   **Work Stealing (任务窃取)**: 实现了多队列架构，线程优先处理本地任务，空闲时从其他线程队列窃取任务，大幅提升了负载不均场景下的吞吐量。
*   **Lock-Free 思想应用**: 虽然使用了 `std::mutex`，但通过**细粒度锁 (Fine-Grained Locking)** 和 **`try_lock`** 机制，避免了线程阻塞，接近无锁性能。
*   **Cache Friendly (缓存友好)**: 使用 `alignas(64)` 解决了 **False Sharing (伪共享)** 问题，这是高性能 C++ 编程的标志性细节。
*   **Modern C++20**: 全程使用 C++20 标准，包括 `Concepts` (`std::invocable`)、`std::atomic` 内存序 (`memory_order_acquire/release`)。

---

## 2. 技术要点与设计思想

### A. 架构演进：从“大锅饭”到“分产到户”
*   **旧版 (ThreadPool)**: 
    *   **模型**: 单一全局队列 (`std::queue`) + 全局互斥锁 (`std::mutex`)。
    *   **瓶颈**: 所有线程争抢一把锁，随着核数增加，锁竞争 (Contention) 指数级上升，CPU 大量时间浪费在上下文切换和等待锁上。
*   **新版 (ThreadPoolFast)**:
    *   **模型**: 每线程独立队列 (`std::deque`) + 独立锁。
    *   **优势**: 90% 的时间线程只访问自己的队列（无竞争），只有 10% 的时间（窃取）才涉及跨线程交互。

### B. 内存与缓存优化 (底层视角)
*   **False Sharing (伪共享)**:
    *   **问题**: 如果线程 A 的队列和线程 B 的队列在内存中紧挨着（在同一个 Cache Line 中），A 修改自己的队列会导致 B 的缓存失效，迫使 B 重新从主存读取。
    *   **解法**: `struct alignas(64) WorkQueue`。强制每个队列独占 64 字节（典型的 L1 Cache Line 大小），实现物理隔离。

### C. 并发控制细节
*   **原子操作与内存序**:
    *   使用 `stop_.store(true, std::memory_order_release)` 和 `stop_.load(std::memory_order_acquire)` 建立 **Synchronizes-with** 关系，确保停止信号的可见性，比默认的 `seq_cst` 更轻量。
*   **非阻塞窃取**:
    *   在窃取任务时使用 `try_lock()`。如果目标队列忙，直接跳过。这体现了**“宁可不做，不可阻塞”**的高性能设计哲学。

---

## 3. 性能对比 (Benchmark)
在 500,000 个轻量级任务的测试下：
*   **ThreadPoolFast**: 吞吐量极高，几乎线性扩展。
*   **ThreadPool**: 随着线程数增加，性能提升受限，甚至可能下降（因为锁竞争加剧）。

## 4. 下一步计划 (Day 2-3)
虽然核心引擎已完成，但为了生产级可用，我们还需要：
1.  **优先级队列**: 目前是 FIFO，需要支持高优先级任务插队。
2.  **动态扩缩容**: 根据负载自动增加或减少线程数。
3.  **异常处理**: 任务抛出异常时如何捕获并传递给 `std::future`。
