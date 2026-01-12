# Month 01: 第一性原理思维与C++抽象机器模型

## 本月主题概述

本月是整个五年学习计划的起点，核心目标是**建立第一性原理的思维方式**，并深入理解C++抽象机器模型与底层硬件之间的关系。这是从"API调用者"向"系统理解者"转变的关键一步。

---

## 理论学习内容

### 第一周：第一性原理思维建立

**学习目标**：理解什么是第一性原理，如何将其应用于系统编程

**阅读材料**：
- [ ] 《Thinking in Systems》第1-3章 - 系统思维入门
- [ ] Elon Musk关于第一性原理的演讲/访谈（YouTube）
- [ ] 博客：https://fs.blog/first-principles/

**核心概念**：
1. 第一性原理 vs 类比推理
2. 分解问题到最基本的物理/数学约束
3. 从基本约束重新构建解决方案

**笔记任务**：
- [ ] 写一篇500字的笔记，解释第一性原理如何应用于软件性能优化

### 第二周：计算机体系结构基础

**学习目标**：理解现代CPU的基本工作原理

**阅读材料**：
- [ ] 《Computer Systems: A Programmer's Perspective (CSAPP)》第1章、第5章
- [ ] 博客：What Every Programmer Should Know About Memory (Ulrich Drepper)

**核心概念**：
1. CPU流水线与指令级并行
2. 缓存层级（L1/L2/L3）与缓存行（64字节）
3. 分支预测与预测失败的代价
4. 虚拟内存与页表

**动手实验**：
```cpp
// 验证缓存行大小对性能的影响
#include <chrono>
#include <vector>

void traverse_sequential(std::vector<int>& v) {
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] *= 2;
    }
}

void traverse_strided(std::vector<int>& v, size_t stride) {
    for (size_t i = 0; i < v.size(); i += stride) {
        v[i] *= 2;
    }
}
// 对比 stride=1, 16, 64, 128 的性能差异
```

### 第三周：C++抽象机器模型

**学习目标**：理解C++标准定义的抽象机器与实际硬件的差距

**阅读材料**：
- [ ] C++标准草案 [intro.abstract] 章节
- [ ] CppCon演讲："The C++ Memory Model" by Herb Sutter
- [ ] 博客：https://preshing.com/20120930/weak-vs-strong-memory-models/

**核心概念**：
1. as-if规则：编译器优化的边界
2. 顺序点（Sequence Points）与求值顺序
3. 未定义行为（UB）的本质：编译器假设UB不会发生
4. volatile的真正含义（它不是用于多线程的！）

**代码分析任务**：
```cpp
// 分析以下代码是否有问题
int x = 0;
int y = 0;

void thread1() {
    x = 1;
    int r1 = y;
}

void thread2() {
    y = 1;
    int r2 = x;
}
// 问题：r1和r2能同时为0吗？
```

### 第四周：编译器优化与Godbolt实践

**学习目标**：学会使用Compiler Explorer分析编译器行为

**工具熟练**：
- [ ] Compiler Explorer (godbolt.org) 基本使用
- [ ] 理解常见的编译器优化：内联、循环展开、死代码消除
- [ ] 对比GCC、Clang、MSVC的优化差异

**实践任务**：在Godbolt上完成以下分析
1. [ ] 对比`-O0`、`-O2`、`-O3`下同一函数的汇编输出
2. [ ] 观察`std::vector<int>::push_back`的实际汇编
3. [ ] 分析虚函数调用的汇编实现

---

## 源码阅读任务

### 本月源码目标：理解`std::vector`的内存布局

**阅读路径**（选择一个实现）：
- GCC libstdc++: `bits/stl_vector.h`
- LLVM libc++: `vector`

**重点关注**：
1. [ ] 三指针设计：`_M_start`, `_M_finish`, `_M_end_of_storage`
2. [ ] 扩容策略的实现（2倍 vs 1.5倍）
3. [ ] `push_back`的异常安全保证

**阅读笔记模板**：
```markdown
## std::vector 源码分析

### 数据成员
- 成员1: 作用是...
- 成员2: 作用是...

### 关键函数分析
#### push_back
- 正常路径: ...
- 扩容路径: ...
- 异常安全: ...

### 设计权衡
- 为什么选择这种扩容策略？
- 与其他实现的对比...
```

---

## 实践项目

### 项目：实现 mini_vector<T>

**目标**：通过造轮子深入理解动态数组的实现

**要求**：
1. [ ] 基本功能：构造、析构、push_back、pop_back、operator[]
2. [ ] 正确处理内存对齐（使用`alignas`）
3. [ ] 实现移动语义（移动构造、移动赋值）
4. [ ] 提供强异常安全保证
5. [ ] 支持自定义分配器（可选）

**代码框架**：
```cpp
template <typename T, typename Allocator = std::allocator<T>>
class mini_vector {
public:
    // 类型别名
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;

    // 构造与析构
    mini_vector() noexcept;
    explicit mini_vector(size_type count);
    mini_vector(const mini_vector& other);
    mini_vector(mini_vector&& other) noexcept;
    ~mini_vector();

    // 赋值
    mini_vector& operator=(const mini_vector& other);
    mini_vector& operator=(mini_vector&& other) noexcept;

    // 元素访问
    reference operator[](size_type pos);
    const_reference operator[](size_type pos) const;
    reference at(size_type pos);

    // 容量
    bool empty() const noexcept;
    size_type size() const noexcept;
    size_type capacity() const noexcept;
    void reserve(size_type new_cap);

    // 修改器
    void push_back(const T& value);
    void push_back(T&& value);
    template <typename... Args>
    reference emplace_back(Args&&... args);
    void pop_back();
    void clear() noexcept;

private:
    T* data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
    Allocator alloc_;

    void reallocate(size_type new_cap);
};
```

**测试用例**（必须通过）：
```cpp
void test_mini_vector() {
    // 基本操作
    mini_vector<int> v;
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
    }
    assert(v.size() == 100);
    assert(v[50] == 50);

    // 移动语义
    mini_vector<int> v2 = std::move(v);
    assert(v.size() == 0);
    assert(v2.size() == 100);

    // 异常安全（使用会抛异常的类型测试）
    // ...
}
```

---

## 检验标准

### 知识检验
能够回答以下问题：
- [ ] 什么是第一性原理？举一个在性能优化中应用的例子
- [ ] CPU缓存行是什么？为什么是64字节？
- [ ] C++的as-if规则是什么？编译器能做哪些优化？
- [ ] `std::vector`扩容时为什么选择2倍而不是1.1倍或10倍？

### 实践检验
- [ ] mini_vector通过所有测试用例
- [ ] 能在Godbolt上分析并解释一段代码的汇编输出
- [ ] 写出一篇源码分析笔记（500字以上）

### 输出物
1. `mini_vector.hpp` 实现文件
2. `test_mini_vector.cpp` 测试文件
3. `notes/month01_vector_analysis.md` 源码分析笔记

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习（阅读、视频） | 50小时 | 36% |
| 源码阅读与分析 | 30小时 | 21% |
| 实践项目开发 | 40小时 | 29% |
| 笔记整理与复习 | 20小时 | 14% |

---

---

## 详细周计划

### Week 1: 第一性原理思维建立（35小时）

**本周目标**：建立第一性原理思维方式，学会从基本原理出发分析问题

#### 每日任务分解

| Day | 时间分配 | 上午任务（2.5h） | 下午任务（2.5h） | 输出物 |
|-----|----------|------------------|------------------|--------|
| **Day 1** | 5h | 阅读《Thinking in Systems》第1章"系统的基本概念" | 整理笔记，绘制系统要素图（存量、流量、反馈回路） | `notes/week1/day1_systems_basics.md` |
| **Day 2** | 5h | 阅读《Thinking in Systems》第2章"系统动力学" | 阅读第3章"系统的弹性与层级" + 案例分析 | `notes/week1/day2_system_dynamics.md` |
| **Day 3** | 5h | 观看Elon Musk第一性原理演讲（搜索YouTube 2-3个视频） | 整理思维导图：第一性原理 vs 类比推理 | `notes/week1/day3_first_principles_mindmap.png` |
| **Day 4** | 5h | 精读fs.blog/first-principles文章 | 收集3个软件工程中的第一性原理案例并分析 | `notes/week1/day4_case_studies.md` |
| **Day 5** | 5h | 分析一个性能问题：用第一性原理拆解（如：为什么vector比list快？） | 从CPU缓存、内存布局等基本约束重新推导 | `notes/week1/day5_performance_analysis.md` |
| **Day 6** | 5h | 撰写"第一性原理与性能优化"笔记初稿 | 修改完善，确保500字以上，逻辑清晰 | `notes/week1/first_principles_optimization.md` |
| **Day 7** | 5h | 本周所有笔记复盘与整理 | 下载CSAPP PDF/电子书，准备Week 2材料 | `notes/week1/week1_summary.md` |

#### 检验标准
- [ ] 能用自己的话解释什么是第一性原理
- [ ] 能区分第一性原理思维与类比思维
- [ ] 完成500字性能优化笔记
- [ ] 整理出至少3个应用第一性原理的案例

---

### Week 2: 计算机体系结构基础（35小时）

**本周目标**：理解CPU缓存、内存层级、流水线等硬件基础，为后续优化打下基础

#### 每日任务分解

| Day | 时间分配 | 上午任务（2.5h） | 下午任务（2.5h） | 输出物 |
|-----|----------|------------------|------------------|--------|
| **Day 1** | 5h | CSAPP第1章：计算机系统漫游（重点：编译系统、存储器层次） | 笔记整理 + 习题1.1-1.5 | `notes/week2/day1_csapp_ch1.md` |
| **Day 2** | 5h | CSAPP第5章5.1-5.7：优化程序性能（循环展开、减少过程调用） | 动手实验：对比不同优化等级的代码运行时间 | `notes/week2/day2_optimization_basics.md` |
| **Day 3** | 5h | CSAPP第5章5.8-5.15：现代处理器（流水线、乱序执行） | 习题5.13-5.19 | `notes/week2/day3_modern_processor.md` |
| **Day 4** | 5h | Ulrich Drepper论文Part 1-2：CPU缓存（组相联、写策略） | 绘制缓存层级图，记录L1/L2/L3典型大小和延迟 | `notes/week2/day4_memory_part1.md` |
| **Day 5** | 5h | Ulrich Drepper论文Part 3-4：虚拟内存、NUMA | 理解TLB、页表、缓存行对齐 | `notes/week2/day5_memory_part2.md` |
| **Day 6** | 5h | 编写缓存行stride访问实验代码 | 测试stride=1,16,64,128的性能差异 | `experiments/cache_stride_test.cpp` |
| **Day 7** | 5h | 分析实验结果，撰写实验报告 | 周复盘，整合所有笔记 | `notes/week2/cache_experiment_report.md` |

#### 实验代码框架
```cpp
// experiments/cache_stride_test.cpp
#include <chrono>
#include <vector>
#include <iostream>

void test_stride(std::vector<int>& v, size_t stride) {
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < v.size(); i += stride) {
        v[i] *= 2;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Stride " << stride << ": " << duration.count() << " us\n";
}

int main() {
    constexpr size_t SIZE = 64 * 1024 * 1024; // 64M integers
    std::vector<int> data(SIZE, 1);

    for (size_t stride : {1, 2, 4, 8, 16, 32, 64, 128}) {
        test_stride(data, stride);
    }
    return 0;
}
```

#### 检验标准
- [ ] 能解释CPU三级缓存的作用和典型大小
- [ ] 能解释缓存行（64字节）对性能的影响
- [ ] 完成stride实验并能解释结果
- [ ] 理解流水线和分支预测的基本概念

---

### Week 3: C++抽象机器模型（35小时）

**本周目标**：理解C++标准定义的抽象机器，掌握as-if规则和UB的本质

#### 每日任务分解

| Day | 时间分配 | 上午任务（2.5h） | 下午任务（2.5h） | 输出物 |
|-----|----------|------------------|------------------|--------|
| **Day 1** | 5h | 阅读C++标准[intro.abstract]章节（可用cppreference辅助） | 整理笔记：抽象机器的定义、可观察行为 | `notes/week3/day1_abstract_machine.md` |
| **Day 2** | 5h | 观看Herb Sutter "C++ Memory Model"演讲（CppCon） | 记录关键概念：sequenced-before、happens-before | `notes/week3/day2_memory_model.md` |
| **Day 3** | 5h | 精读Preshing博客"weak vs strong memory models" | 对比x86（强）和ARM（弱）内存模型 | `notes/week3/day3_memory_ordering.md` |
| **Day 4** | 5h | 深入研究as-if规则：编译器可以做什么优化？ | 用Godbolt验证：死代码消除、常量折叠等 | `notes/week3/day4_as_if_rule.md` |
| **Day 5** | 5h | UB案例研究：收集10个常见UB示例 | 分析每个UB为什么是UB，编译器如何利用 | `notes/week3/day5_ub_examples.md` |
| **Day 6** | 5h | volatile深入分析：为什么不能用于多线程？ | 分析多线程代码示例（r1和r2能同时为0吗？） | `notes/week3/day6_volatile_threading.md` |
| **Day 7** | 5h | 整合本周所有笔记 | 撰写"C++内存模型要点总结" | `notes/week3/week3_summary.md` |

#### UB案例收集模板
```markdown
## UB案例 #1: 有符号整数溢出

### 代码示例
```cpp
int x = INT_MAX;
x = x + 1; // UB!
```

### 为什么是UB
C++标准规定有符号整数溢出是未定义行为...

### 编译器如何利用
编译器假设UB不会发生，因此可以假设 x + 1 > x 永远成立...

### 如何避免
使用无符号整数，或添加溢出检查...
```

#### 检验标准
- [ ] 能解释as-if规则的含义和边界
- [ ] 能列举5个以上常见的UB
- [ ] 能解释volatile为什么不适用于多线程
- [ ] 理解强/弱内存模型的区别

---

### Week 4: 编译器优化与Godbolt + mini_vector项目（35小时）

**本周目标**：掌握Godbolt工具，完成mini_vector实现，撰写源码分析笔记

#### 每日任务分解

| Day | 时间分配 | 上午任务（2.5h） | 下午任务（2.5h） | 输出物 |
|-----|----------|------------------|------------------|--------|
| **Day 1** | 5h | Godbolt基础使用：熟悉界面、编译器选项 | O0/O2/O3对比实验：选3个函数分析汇编差异 | `notes/week4/day1_godbolt_basics.md` + 截图 |
| **Day 2** | 5h | 分析`std::vector::push_back`的汇编实现 | 分析虚函数调用的汇编（vtable查找） | `notes/week4/day2_assembly_analysis.md` |
| **Day 3** | 5h | 阅读GCC libstdc++ `bits/stl_vector.h`源码 | 重点分析：三指针设计、扩容策略 | `notes/week4/day3_vector_source.md` |
| **Day 4** | 5h | mini_vector框架搭建：类定义、类型别名 | 实现构造函数、析构函数、reserve | `src/mini_vector.hpp`（基础版） |
| **Day 5** | 5h | 实现push_back（const T&和T&&两个版本） | 实现emplace_back、扩容逻辑 | `src/mini_vector.hpp`（功能完善） |
| **Day 6** | 5h | 实现移动构造、移动赋值、拷贝操作 | 编写测试用例，确保异常安全 | `src/mini_vector.hpp` + `test/test_mini_vector.cpp` |
| **Day 7** | 5h | 运行所有测试，修复bug | 撰写源码分析笔记 + 月度总结 | `notes/month01_vector_analysis.md` |

#### mini_vector实现检查清单
```
Day 4 完成：
- [ ] 类模板定义
- [ ] 类型别名（value_type, size_type, iterator等）
- [ ] 默认构造函数
- [ ] 析构函数（正确释放内存）
- [ ] reserve函数

Day 5 完成：
- [ ] push_back(const T&)
- [ ] push_back(T&&)
- [ ] emplace_back
- [ ] 扩容逻辑（2倍策略）
- [ ] size(), capacity(), empty()

Day 6 完成：
- [ ] 拷贝构造函数
- [ ] 拷贝赋值运算符
- [ ] 移动构造函数
- [ ] 移动赋值运算符
- [ ] operator[]
- [ ] at() with bounds checking
- [ ] clear(), pop_back()
```

#### 测试用例要求
```cpp
// test/test_mini_vector.cpp
#include "mini_vector.hpp"
#include <cassert>
#include <string>

void test_basic_operations() {
    mini_vector<int> v;
    assert(v.empty());
    assert(v.size() == 0);

    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
    }
    assert(v.size() == 100);
    assert(v[50] == 50);
}

void test_move_semantics() {
    mini_vector<std::string> v1;
    v1.push_back("hello");
    v1.push_back("world");

    mini_vector<std::string> v2 = std::move(v1);
    assert(v1.size() == 0);
    assert(v2.size() == 2);
    assert(v2[0] == "hello");
}

void test_emplace_back() {
    struct Point {
        int x, y;
        Point(int x, int y) : x(x), y(y) {}
    };

    mini_vector<Point> points;
    points.emplace_back(1, 2);
    points.emplace_back(3, 4);

    assert(points[0].x == 1);
    assert(points[1].y == 4);
}

int main() {
    test_basic_operations();
    test_move_semantics();
    test_emplace_back();
    std::cout << "All tests passed!\n";
    return 0;
}
```

#### 检验标准
- [ ] 能在Godbolt上分析任意C++代码的汇编输出
- [ ] 理解std::vector的三指针设计和扩容策略
- [ ] mini_vector通过所有测试用例
- [ ] 完成500字以上的源码分析笔记

---

## 本月输出物清单

### 笔记文件
```
notes/
├── week1/
│   ├── day1_systems_basics.md
│   ├── day2_system_dynamics.md
│   ├── day3_first_principles_mindmap.png
│   ├── day4_case_studies.md
│   ├── day5_performance_analysis.md
│   ├── first_principles_optimization.md
│   └── week1_summary.md
├── week2/
│   ├── day1_csapp_ch1.md
│   ├── day2_optimization_basics.md
│   ├── day3_modern_processor.md
│   ├── day4_memory_part1.md
│   ├── day5_memory_part2.md
│   └── cache_experiment_report.md
├── week3/
│   ├── day1_abstract_machine.md
│   ├── day2_memory_model.md
│   ├── day3_memory_ordering.md
│   ├── day4_as_if_rule.md
│   ├── day5_ub_examples.md
│   ├── day6_volatile_threading.md
│   └── week3_summary.md
├── week4/
│   ├── day1_godbolt_basics.md
│   ├── day2_assembly_analysis.md
│   └── day3_vector_source.md
└── month01_vector_analysis.md
```

### 代码文件
```
src/
└── mini_vector.hpp

test/
└── test_mini_vector.cpp

experiments/
└── cache_stride_test.cpp
```

---

## 下月预告

Month 02将深入**调试器（GDB/LLDB）精通**，学习如何使用调试器作为"显微镜"观察程序运行时状态，包括内存布局、虚函数表、智能指针控制块等。
