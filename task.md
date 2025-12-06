# Task List

## Day 1: High-Performance Thread Pool [Completed]
- [x] Create basic ThreadPool skeleton (Phase 1)
- [x] Implement `std::future` support (Phase 2)
- [x] Implement Work Stealing & Fine-Grained Locking (Phase 3)
- [x] Implement Cache Alignment to prevent False Sharing
- [x] Benchmark comparison (ThreadPool vs ThreadPoolFast)
- [x] Create Day 1 Summary

## Day 2: Priority & Optimization [Completed]
- [x] Update `ThreadPoolFast` interface to support Priority (Implemented as `ThreadPoolPriority`)
- [x] Implement Multi-Level Queues (High, Normal, Low)
- [x] Update Stealing logic for priority (Optimized: Steal from back)
- [x] Verify Priority Scheduling (Integrated into main.cpp)

## Day 3: Engineering & Coroutines [Pending]
- [ ] Integrate Google Test (vcpkg)
- [ ] Write Unit Tests
- [ ] Create simple Coroutine example
- [ ] Experiment with Coroutines on ThreadPool
