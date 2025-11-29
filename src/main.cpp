#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include "thread_pool/thread_pool.h"
#include "thread_pool/thread_pool_fast.h"

// 性能测试配置
const int NUM_TASKS = 500000;    // 任务数量 (增加数量以凸显差距)
const int WORK_ITERATIONS =
    100;    // 每个任务的计算量 (模拟轻量级任务，凸显锁开销)

// 模拟工作负载
void heavy_work() {
    volatile int x = 0;
    for (int j = 0; j < WORK_ITERATIONS; ++j) {
        x = x + 1;
    }
    (void)x;
}

// 测试普通线程池
void benchmark_normal_pool(size_t num_threads) {
    std::cout << "Testing Normal ThreadPool (" << num_threads
              << " threads)...\n";

    ThreadPool pool(num_threads);
    std::vector<std::future<void>> results;
    results.reserve(NUM_TASKS);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_TASKS; ++i) {
        // AddTask 返回 std::future<void> (如果任务返回 void)
        // 注意：原版 ThreadPool::AddTask 实现可能需要适配 void 返回值，
        // 如果原版 AddTask 返回 future<void> 则没问题。
        // 原版实现中 AddTask 返回 future<invoke_result_t>。
        results.emplace_back(pool.AddTask(heavy_work));
    }

    for (auto& res : results) {
        res.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "  -> Time: " << std::fixed << std::setprecision(4)
              << diff.count() << "s\n";
    std::cout << "  -> Throughput: " << (NUM_TASKS / diff.count())
              << " tasks/s\n";
}

// 测试高性能线程池
void benchmark_fast_pool(size_t num_threads) {
    std::cout << "Testing ThreadPoolFast (" << num_threads << " threads)...\n";

    ThreadPoolFast pool(num_threads);
    std::vector<std::future<void>> results;
    results.reserve(NUM_TASKS);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_TASKS; ++i) {
        results.emplace_back(pool.submit(heavy_work));
    }

    for (auto& res : results) {
        res.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "  -> Time: " << std::fixed << std::setprecision(4)
              << diff.count() << "s\n";
    std::cout << "  -> Throughput: " << (NUM_TASKS / diff.count())
              << " tasks/s\n";
}

int main() {
    size_t num_threads = std::thread::hardware_concurrency();
    std::cout << "========================================\n";
    std::cout << " Thread Pool Performance Benchmark\n";
    std::cout << " Tasks: " << NUM_TASKS << "\n";
    std::cout << " Threads: " << num_threads << "\n";
    std::cout << "========================================\n\n";

    // 1. 测试普通线程池
    benchmark_normal_pool(num_threads);

    std::cout << "\n----------------------------------------\n\n";

    // 2. 测试高性能线程池
    benchmark_fast_pool(num_threads);

    std::cout << "\n========================================\n";
    std::cout << "Analysis:\n";
    std::cout << "ThreadPoolFast should significantly outperform ThreadPool\n";
    std::cout << "due to reduced lock contention (Fine-Grained Locking)\n";
    std::cout << "and better cache locality (Work Stealing + Padding).\n";

    return 0;
}