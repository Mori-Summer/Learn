# Month 16: ABA问题与内存回收——无锁编程的隐秘陷阱

## 本月主题概述

ABA问题是无锁编程中最微妙的Bug来源之一。本月将深入理解ABA问题的本质，学习危险指针（Hazard Pointers）、Epoch-based回收等解决方案，以及它们在实际系统中的应用。

---

## 理论学习内容

### 第一周：ABA问题深度分析

**学习目标**：彻底理解ABA问题的本质

**阅读材料**：
- [ ] 论文："Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects"
- [ ] 博客：Preshing "An Introduction to Lock-Free Programming"
- [ ] 《C++ Concurrency in Action》无锁数据结构章节

**核心概念**：

#### ABA问题示例
```cpp
// 无锁栈的pop操作
template <typename T>
class BuggyStack {
    struct Node {
        T data;
        Node* next;
    };
    std::atomic<Node*> head_;

    std::optional<T> pop() {
        Node* old_head = head_.load();
        while (old_head != nullptr) {
            Node* new_head = old_head->next;  // (1) 读取next
            if (head_.compare_exchange_weak(old_head, new_head)) {
                T result = old_head->data;
                delete old_head;
                return result;
            }
        }
        return std::nullopt;
    }
};

// ABA问题场景：
// 初始状态：head -> A -> B -> C
//
// Thread 1: pop()
//   old_head = A
//   new_head = B (A->next)
//   --- 被中断 ---
//
// Thread 2: pop() 成功，删除A
// Thread 2: pop() 成功，删除B
// Thread 2: push(D)，其中D恰好被分配到A的地址！
// Thread 2: push(A)，重用原来的A地址
//
// 现在：head -> A(new) -> D -> C
// 但A(new)->next = D，不是B！
//
// Thread 1: 恢复执行
//   CAS成功！因为head仍然是A（地址相同）
//   head变成B，但B已经被删除了！
//   --- 内存腐败 ---
```

#### 为什么CAS无法检测ABA？
```cpp
// CAS只比较值，不比较"身份"
// 当一个指针被释放并重新分配到相同地址时
// CAS无法区分这是"同一个对象"还是"不同对象恰好在同一地址"

// 问题的根源：
// 1. CAS是基于值的比较
// 2. 内存分配器可能重用地址
// 3. 没有机制追踪"对象版本"
```

### 第二周：版本计数解决方案

**学习目标**：使用版本号/计数器解决ABA

#### Tagged Pointer（标签指针）
```cpp
// 在指针中嵌入版本号
// 64位系统上，高16位通常不用于地址（取决于架构）
// 或者使用对齐保证的低位

template <typename T>
class TaggedPtr {
    // 使用union或位操作
    // 假设T*是8字节对齐，低3位可用
    uintptr_t data_;

    static constexpr uintptr_t TAG_MASK = 0x7;  // 低3位
    static constexpr uintptr_t PTR_MASK = ~TAG_MASK;

public:
    TaggedPtr() : data_(0) {}
    TaggedPtr(T* ptr, unsigned tag)
        : data_(reinterpret_cast<uintptr_t>(ptr) | (tag & TAG_MASK)) {}

    T* ptr() const {
        return reinterpret_cast<T*>(data_ & PTR_MASK);
    }

    unsigned tag() const {
        return data_ & TAG_MASK;
    }

    bool operator==(const TaggedPtr& other) const {
        return data_ == other.data_;
    }
};

// 使用128位CAS可以存更大的计数器
struct alignas(16) PtrWithCounter {
    void* ptr;
    uint64_t counter;
};
```

#### 使用版本号的无锁栈
```cpp
template <typename T>
class ABASafeStack {
    struct Node {
        T data;
        Node* next;
    };

    struct alignas(16) Head {
        Node* ptr;
        uint64_t counter;  // 版本号，每次修改递增
    };

    std::atomic<Head> head_{{nullptr, 0}};

public:
    void push(T value) {
        Node* new_node = new Node{std::move(value), nullptr};
        Head old_head = head_.load(std::memory_order_relaxed);
        Head new_head;
        do {
            new_node->next = old_head.ptr;
            new_head = {new_node, old_head.counter + 1};
        } while (!head_.compare_exchange_weak(old_head, new_head,
                    std::memory_order_release,
                    std::memory_order_relaxed));
    }

    std::optional<T> pop() {
        Head old_head = head_.load(std::memory_order_relaxed);
        Head new_head;
        do {
            if (old_head.ptr == nullptr) {
                return std::nullopt;
            }
            new_head = {old_head.ptr->next, old_head.counter + 1};
        } while (!head_.compare_exchange_weak(old_head, new_head,
                    std::memory_order_acquire,
                    std::memory_order_relaxed));

        T result = std::move(old_head.ptr->data);
        delete old_head.ptr;  // 仍然有问题！
        return result;
    }
};

// 版本号解决了ABA，但内存回收问题仍然存在
// 在delete之前，其他线程可能正在读取old_head.ptr->next
```

### 第三周：危险指针（Hazard Pointers）

**学习目标**：掌握危险指针的原理和实现

#### 危险指针概念
```cpp
// 核心思想：
// 每个线程声明它正在访问的指针
// 释放内存前，检查是否有线程正在使用

// 每个线程有若干个"危险指针槽"
// 访问共享内存前，将地址放入危险指针
// 完成访问后，清除危险指针
// 释放内存前，检查所有线程的危险指针

class HazardPointer {
    static constexpr int MAX_THREADS = 64;
    static constexpr int HAZARDS_PER_THREAD = 2;

    struct HazardRecord {
        std::atomic<void*> hazards[HAZARDS_PER_THREAD];
        std::atomic<bool> active{false};
    };

    static HazardRecord records_[MAX_THREADS];
    static thread_local int my_index_;

public:
    // 获取一个危险指针槽
    template <typename T>
    class Guard {
        std::atomic<void*>* slot_;
    public:
        Guard() : slot_(nullptr) {}

        T* protect(std::atomic<T*>& src) {
            // 分配槽位
            if (!slot_) {
                for (int i = 0; i < HAZARDS_PER_THREAD; ++i) {
                    void* expected = nullptr;
                    if (records_[my_index_].hazards[i]
                            .compare_exchange_strong(expected, reinterpret_cast<void*>(1))) {
                        slot_ = &records_[my_index_].hazards[i];
                        break;
                    }
                }
            }

            T* ptr;
            do {
                ptr = src.load(std::memory_order_relaxed);
                slot_->store(ptr, std::memory_order_release);
                // 需要重新检查，因为在store之前src可能已经改变
            } while (ptr != src.load(std::memory_order_acquire));

            return ptr;
        }

        void reset() {
            if (slot_) {
                slot_->store(nullptr, std::memory_order_release);
            }
        }

        ~Guard() {
            reset();
        }
    };

    // 检查指针是否被任何线程保护
    static bool is_hazardous(void* ptr) {
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (!records_[i].active.load(std::memory_order_relaxed)) continue;
            for (int j = 0; j < HAZARDS_PER_THREAD; ++j) {
                if (records_[i].hazards[j].load(std::memory_order_acquire) == ptr) {
                    return true;
                }
            }
        }
        return false;
    }

    // 延迟释放
    static void retire(void* ptr, void (*deleter)(void*)) {
        // 添加到待释放列表
        // 定期扫描并释放安全的指针
    }
};
```

#### 使用危险指针的无锁栈
```cpp
template <typename T>
class HazardPointerStack {
    struct Node {
        T data;
        std::atomic<Node*> next;
    };

    std::atomic<Node*> head_{nullptr};

public:
    void push(T value) {
        Node* new_node = new Node{std::move(value)};
        Node* old_head = head_.load(std::memory_order_relaxed);
        do {
            new_node->next.store(old_head, std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(old_head, new_node,
                    std::memory_order_release,
                    std::memory_order_relaxed));
    }

    std::optional<T> pop() {
        HazardPointer::Guard<Node> guard;
        Node* old_head;

        do {
            old_head = guard.protect(head_);  // 保护当前head
            if (old_head == nullptr) {
                return std::nullopt;
            }
        } while (!head_.compare_exchange_weak(old_head, old_head->next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed));

        guard.reset();  // 不再需要保护

        T result = std::move(old_head->data);
        // 安全地延迟释放
        HazardPointer::retire(old_head, [](void* p) { delete static_cast<Node*>(p); });

        return result;
    }
};
```

### 第四周：Epoch-Based Reclamation（EBR）

**学习目标**：理解EBR的原理和实现

#### EBR概念
```cpp
// 核心思想：
// 将时间分为"epoch"（纪元）
// 当所有线程都经历过一个epoch后，该epoch之前的内存可以安全回收

// 优点：
// - 比危险指针更高效（不需要每次访问都设置）
// - 适合读多写少的场景

// 缺点：
// - 如果某个线程长时间不推进epoch，内存无法回收
// - 需要线程协作

class EpochBasedReclamation {
    static constexpr int MAX_THREADS = 64;

    // 全局epoch
    std::atomic<uint64_t> global_epoch_{0};

    // 每个线程的状态
    struct ThreadState {
        std::atomic<uint64_t> local_epoch{0};
        std::atomic<bool> active{false};
        std::vector<void*> retire_lists[3];  // 三个epoch的待回收列表
    };

    static thread_local ThreadState* my_state_;
    ThreadState states_[MAX_THREADS];

public:
    // 进入临界区
    class Guard {
        EpochBasedReclamation& ebr_;
    public:
        Guard(EpochBasedReclamation& ebr) : ebr_(ebr) {
            ebr_.enter();
        }
        ~Guard() {
            ebr_.leave();
        }
    };

    void enter() {
        if (!my_state_) {
            // 分配线程状态
            for (int i = 0; i < MAX_THREADS; ++i) {
                if (!states_[i].active.exchange(true)) {
                    my_state_ = &states_[i];
                    break;
                }
            }
        }

        uint64_t epoch = global_epoch_.load(std::memory_order_relaxed);
        my_state_->local_epoch.store(epoch, std::memory_order_release);
    }

    void leave() {
        my_state_->local_epoch.store(UINT64_MAX, std::memory_order_release);
        try_reclaim();
    }

    void retire(void* ptr) {
        uint64_t epoch = global_epoch_.load(std::memory_order_relaxed);
        my_state_->retire_lists[epoch % 3].push_back(ptr);
    }

private:
    void try_reclaim() {
        uint64_t current = global_epoch_.load(std::memory_order_relaxed);

        // 检查是否可以推进epoch
        bool can_advance = true;
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (!states_[i].active.load(std::memory_order_relaxed)) continue;
            uint64_t local = states_[i].local_epoch.load(std::memory_order_acquire);
            if (local != UINT64_MAX && local != current) {
                can_advance = false;
                break;
            }
        }

        if (can_advance) {
            global_epoch_.compare_exchange_strong(current, current + 1,
                std::memory_order_acq_rel);

            // 回收两个epoch前的内存
            uint64_t safe_epoch = (current + 1) % 3;
            for (void* ptr : my_state_->retire_lists[safe_epoch]) {
                delete ptr;  // 实际使用时需要类型信息
            }
            my_state_->retire_lists[safe_epoch].clear();
        }
    }
};
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] folly的HazardPointer实现
- [ ] crossbeam（Rust）的epoch-based回收
- [ ] libcds的各种回收方案
- [ ] Linux内核的RCU实现

---

## 实践项目

### 项目：实现完整的内存回收方案

#### Part 1: 简化的危险指针
```cpp
// hazard_pointer.hpp
#pragma once
#include <atomic>
#include <vector>
#include <functional>
#include <thread>
#include <algorithm>

class HazardPointerDomain {
    static constexpr size_t MAX_HAZARD_POINTERS = 100;
    static constexpr size_t SCAN_THRESHOLD = 100;

    struct HazardPointerRecord {
        std::atomic<std::thread::id> owner{std::thread::id{}};
        std::atomic<void*> ptr{nullptr};
    };

    HazardPointerRecord hazard_pointers_[MAX_HAZARD_POINTERS];

    struct RetiredNode {
        void* ptr;
        std::function<void(void*)> deleter;
    };

    static thread_local std::vector<RetiredNode> retired_;

public:
    class HazardPointerHolder {
        HazardPointerRecord* record_;

    public:
        HazardPointerHolder() : record_(nullptr) {}

        ~HazardPointerHolder() {
            if (record_) {
                record_->ptr.store(nullptr, std::memory_order_release);
                record_->owner.store(std::thread::id{}, std::memory_order_release);
            }
        }

        HazardPointerHolder(const HazardPointerHolder&) = delete;
        HazardPointerHolder& operator=(const HazardPointerHolder&) = delete;

        HazardPointerHolder(HazardPointerHolder&& other) noexcept
            : record_(other.record_) {
            other.record_ = nullptr;
        }

        template <typename T>
        T* protect(std::atomic<T*>& src, HazardPointerDomain& domain) {
            if (!record_) {
                record_ = domain.acquire_record();
            }

            T* ptr;
            do {
                ptr = src.load(std::memory_order_relaxed);
                record_->ptr.store(ptr, std::memory_order_release);
            } while (ptr != src.load(std::memory_order_acquire));

            return ptr;
        }

        void reset() {
            if (record_) {
                record_->ptr.store(nullptr, std::memory_order_release);
            }
        }
    };

    HazardPointerRecord* acquire_record() {
        auto this_id = std::this_thread::get_id();

        // 尝试获取已有记录
        for (size_t i = 0; i < MAX_HAZARD_POINTERS; ++i) {
            std::thread::id expected{};
            if (hazard_pointers_[i].owner.compare_exchange_strong(
                    expected, this_id, std::memory_order_acq_rel)) {
                return &hazard_pointers_[i];
            }
        }

        throw std::runtime_error("No hazard pointer available");
    }

    template <typename T>
    void retire(T* ptr) {
        retired_.push_back({ptr, [](void* p) { delete static_cast<T*>(p); }});

        if (retired_.size() >= SCAN_THRESHOLD) {
            scan();
        }
    }

    void scan() {
        // 收集所有危险指针
        std::vector<void*> hazardous;
        for (size_t i = 0; i < MAX_HAZARD_POINTERS; ++i) {
            void* p = hazard_pointers_[i].ptr.load(std::memory_order_acquire);
            if (p) {
                hazardous.push_back(p);
            }
        }

        std::sort(hazardous.begin(), hazardous.end());

        // 回收安全的节点
        auto it = std::remove_if(retired_.begin(), retired_.end(),
            [&hazardous](const RetiredNode& node) {
                if (!std::binary_search(hazardous.begin(), hazardous.end(), node.ptr)) {
                    node.deleter(node.ptr);
                    return true;
                }
                return false;
            });

        retired_.erase(it, retired_.end());
    }
};

thread_local std::vector<HazardPointerDomain::RetiredNode>
    HazardPointerDomain::retired_;
```

#### Part 2: 简化的EBR
```cpp
// epoch_reclamation.hpp
#pragma once
#include <atomic>
#include <vector>
#include <functional>
#include <array>

class EpochReclamation {
    static constexpr size_t MAX_THREADS = 64;
    static constexpr uint64_t INACTIVE = UINT64_MAX;

    std::atomic<uint64_t> global_epoch_{0};

    struct ThreadData {
        std::atomic<uint64_t> local_epoch{INACTIVE};
        std::array<std::vector<std::pair<void*, std::function<void(void*)>>>, 3> garbage;
    };

    std::array<ThreadData, MAX_THREADS> thread_data_;
    static thread_local size_t thread_index_;
    static thread_local bool registered_;

    size_t register_thread() {
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            uint64_t expected = INACTIVE;
            if (thread_data_[i].local_epoch.compare_exchange_strong(
                    expected, global_epoch_.load(std::memory_order_relaxed),
                    std::memory_order_acq_rel)) {
                return i;
            }
        }
        throw std::runtime_error("Too many threads");
    }

public:
    class Guard {
        EpochReclamation& er_;
        size_t index_;

    public:
        Guard(EpochReclamation& er) : er_(er) {
            if (!registered_) {
                thread_index_ = er_.register_thread();
                registered_ = true;
            }
            index_ = thread_index_;
            er_.enter(index_);
        }

        ~Guard() {
            er_.leave(index_);
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    };

    void enter(size_t index) {
        thread_data_[index].local_epoch.store(
            global_epoch_.load(std::memory_order_relaxed),
            std::memory_order_release);
    }

    void leave(size_t index) {
        thread_data_[index].local_epoch.store(INACTIVE, std::memory_order_release);
        try_advance();
    }

    template <typename T>
    void retire(T* ptr) {
        size_t epoch = global_epoch_.load(std::memory_order_relaxed) % 3;
        thread_data_[thread_index_].garbage[epoch].emplace_back(
            ptr, [](void* p) { delete static_cast<T*>(p); });
    }

private:
    void try_advance() {
        uint64_t current = global_epoch_.load(std::memory_order_relaxed);

        // 检查所有线程是否都在当前epoch或非活跃
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            uint64_t local = thread_data_[i].local_epoch.load(std::memory_order_acquire);
            if (local != INACTIVE && local != current) {
                return;  // 有线程落后
            }
        }

        // 尝试推进epoch
        if (global_epoch_.compare_exchange_strong(current, current + 1,
                std::memory_order_acq_rel)) {
            // 回收安全的内存（两个epoch前的）
            size_t safe_epoch = (current + 2) % 3;
            auto& garbage = thread_data_[thread_index_].garbage[safe_epoch];
            for (auto& [ptr, deleter] : garbage) {
                deleter(ptr);
            }
            garbage.clear();
        }
    }
};

thread_local size_t EpochReclamation::thread_index_ = 0;
thread_local bool EpochReclamation::registered_ = false;
```

#### Part 3: 使用EBR的无锁链表
```cpp
// lockfree_list.hpp
#pragma once
#include "epoch_reclamation.hpp"
#include <atomic>
#include <optional>

template <typename T>
class LockFreeList {
    struct Node {
        T data;
        std::atomic<Node*> next;

        Node(T d) : data(std::move(d)), next(nullptr) {}
    };

    std::atomic<Node*> head_{nullptr};
    EpochReclamation& er_;

public:
    explicit LockFreeList(EpochReclamation& er) : er_(er) {}

    ~LockFreeList() {
        Node* current = head_.load();
        while (current) {
            Node* next = current->next.load();
            delete current;
            current = next;
        }
    }

    void push_front(T value) {
        EpochReclamation::Guard guard(er_);
        Node* new_node = new Node(std::move(value));
        Node* old_head = head_.load(std::memory_order_relaxed);
        do {
            new_node->next.store(old_head, std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(old_head, new_node,
                    std::memory_order_release,
                    std::memory_order_relaxed));
    }

    std::optional<T> pop_front() {
        EpochReclamation::Guard guard(er_);
        Node* old_head = head_.load(std::memory_order_relaxed);

        while (old_head) {
            Node* next = old_head->next.load(std::memory_order_relaxed);
            if (head_.compare_exchange_weak(old_head, next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                T result = std::move(old_head->data);
                er_.retire(old_head);
                return result;
            }
        }

        return std::nullopt;
    }

    bool contains(const T& value) {
        EpochReclamation::Guard guard(er_);
        Node* current = head_.load(std::memory_order_acquire);
        while (current) {
            if (current->data == value) {
                return true;
            }
            current = current->next.load(std::memory_order_acquire);
        }
        return false;
    }
};
```

---

## 检验标准

### 知识检验
- [ ] 什么是ABA问题？如何触发？
- [ ] 版本计数如何解决ABA？
- [ ] 危险指针的工作原理是什么？
- [ ] EBR的优缺点是什么？
- [ ] RCU与EBR有什么关系？

### 实践检验
- [ ] 实现的危险指针能正确保护内存
- [ ] EBR能正确回收内存
- [ ] 无锁链表在高并发下正确工作

### 输出物
1. `hazard_pointer.hpp`
2. `epoch_reclamation.hpp`
3. `lockfree_list.hpp`
4. `test_memory_reclamation.cpp`
5. `notes/month16_aba_reclamation.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 40小时 | 29% |
| 源码阅读 | 25小时 | 18% |
| 危险指针实现 | 30小时 | 21% |
| EBR实现 | 30小时 | 21% |
| 测试与文档 | 15小时 | 11% |

---

## 下月预告

Month 17将学习**无锁队列**，这是最实用的无锁数据结构之一。我们将学习Michael-Scott队列、MPSC队列等经典算法。
