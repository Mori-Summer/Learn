# Month 03: STL容器源码深度分析——红黑树与哈希表

## 本月主题概述

本月将系统性地分析STL中两类最重要的关联容器：基于红黑树的`std::map/set`和基于哈希表的`std::unordered_map/set`。通过源码阅读，理解数据结构的工程化实现，培养"从抽象到实现"的思维能力。

---

## 理论学习内容

### 第一周：红黑树理论基础

**学习目标**：彻底理解红黑树的性质和操作

**阅读材料**：
- [ ] 《算法导论》第13章：红黑树
- [ ] 博客：Red-Black Trees Visualized (视觉化理解)
- [ ] 论文：A Dichromatic Framework for Balanced Trees (原始论文，可选)

**核心概念**：
1. 红黑树的五个性质：
   - 每个节点非红即黑
   - 根节点是黑色
   - 叶子节点（NIL）是黑色
   - 红节点的子节点必须是黑色
   - 从任一节点到其叶子的所有路径包含相同数目的黑节点

2. 为什么这些性质保证了平衡？
   - 最长路径（红黑交替）≤ 2 × 最短路径（全黑）
   - 因此高度 h ≤ 2log(n+1)

3. 核心操作复杂度证明：
   - 搜索、插入、删除均为 O(log n)

**手绘练习**：
- [ ] 手绘插入操作的3种情况及其旋转
- [ ] 手绘删除操作的4种情况

### 第二周：std::map/set源码分析

**学习目标**：理解STL红黑树的工程实现

**阅读路径**（GCC libstdc++）：
1. `bits/stl_tree.h` - 红黑树核心实现
2. `bits/stl_map.h` - map包装器
3. `bits/stl_set.h` - set包装器

**源码分析要点**：

#### 节点结构
```cpp
// 分析 _Rb_tree_node_base 和 _Rb_tree_node<_Val>
struct _Rb_tree_node_base {
    _Rb_tree_color _M_color;    // 颜色
    _Base_ptr _M_parent;        // 父节点
    _Base_ptr _M_left;          // 左子节点
    _Base_ptr _M_right;         // 右子节点
};

template<typename _Val>
struct _Rb_tree_node : public _Rb_tree_node_base {
    _Val _M_value_field;        // 存储的值
};
```

**问题思考**：
1. [ ] 为什么节点基类不包含值？（继承vs组合的设计权衡）
2. [ ] header节点的作用是什么？（哨兵节点）
3. [ ] `_M_key_compare` 如何实现自定义比较？

#### 插入操作分析
```cpp
// 追踪 _M_insert_unique 的实现
pair<iterator, bool> insert(const value_type& __x) {
    // 1. 找到插入位置
    // 2. 检查是否已存在（对于unique容器）
    // 3. 插入节点
    // 4. 重新着色和旋转（_Rb_tree_insert_and_rebalance）
}
```

**调试任务**：
```cpp
#include <map>

int main() {
    std::map<int, std::string> m;
    // 在每次插入后观察树的结构
    m[5] = "five";
    m[3] = "three";
    m[7] = "seven";
    m[1] = "one";
    m[4] = "four";
    return 0;
}
```
使用GDB观察每次插入后的树结构变化。

### 第三周：哈希表理论与std::unordered_map

**学习目标**：理解哈希表的实现策略

**阅读材料**：
- [ ] 《算法导论》第11章：散列表
- [ ] CppCon演讲："std::unordered_map: Inside and Out"
- [ ] 博客：Robin Hood Hashing（现代哈希表优化）

**核心概念**：
1. 哈希函数的性质：
   - 确定性
   - 均匀分布
   - 计算效率

2. 冲突解决策略：
   - 链地址法（Chaining）- STL采用
   - 开放寻址法（Open Addressing）
   - Robin Hood Hashing

3. 负载因子（Load Factor）与rehash

**阅读路径**（GCC libstdc++）：
1. `bits/hashtable.h` - 哈希表核心
2. `bits/hashtable_policy.h` - 策略类
3. `bits/unordered_map.h` - 包装器

**源码分析要点**：
```cpp
// _Hashtable 的关键成员
_Bucket_type*  _M_buckets;        // 桶数组
size_type      _M_bucket_count;   // 桶数量
size_type      _M_element_count;  // 元素数量
float          _M_max_load_factor; // 最大负载因子

// 桶的结构
// 每个桶是一个链表头，指向该桶中的第一个节点
```

**问题思考**：
1. [ ] 为什么默认负载因子是1.0？这意味着什么？
2. [ ] rehash时如何重新分配元素？时间复杂度？
3. [ ] `std::hash<T>`如何特化？为什么自定义类型需要特化？

### 第四周：容器性能对比与最佳实践

**学习目标**：建立容器选择的系统性认知

**性能对比实验**：
```cpp
#include <map>
#include <unordered_map>
#include <chrono>
#include <random>
#include <iostream>

template <typename Container>
void benchmark_insert(Container& c, const std::vector<int>& keys) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int k : keys) {
        c[k] = k;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Insert: " << duration.count() << " us\n";
}

template <typename Container>
void benchmark_find(Container& c, const std::vector<int>& keys) {
    auto start = std::chrono::high_resolution_clock::now();
    volatile int sum = 0;
    for (int k : keys) {
        auto it = c.find(k);
        if (it != c.end()) sum += it->second;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Find: " << duration.count() << " us\n";
}

int main() {
    std::vector<int> keys(1000000);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), std::mt19937{42});

    std::cout << "=== std::map ===\n";
    std::map<int, int> m;
    benchmark_insert(m, keys);
    benchmark_find(m, keys);

    std::cout << "=== std::unordered_map ===\n";
    std::unordered_map<int, int> um;
    benchmark_insert(um, keys);
    benchmark_find(um, keys);

    return 0;
}
```

**分析问题**：
1. [ ] 在什么数据规模下map更快？unordered_map更快？
2. [ ] 缓存友好性如何影响实际性能？
3. [ ] 有序遍历的需求如何影响容器选择？

---

## 源码阅读任务

### 深度阅读清单

#### std::map 实现细节
- [ ] `_Rb_tree_insert_and_rebalance` 函数（插入后平衡）
- [ ] `_Rb_tree_rebalance_for_erase` 函数（删除后平衡）
- [ ] iterator 的实现（`_Rb_tree_iterator`）
- [ ] `lower_bound` 和 `upper_bound` 实现

#### std::unordered_map 实现细节
- [ ] 哈希表的桶分配策略
- [ ] rehash 的触发条件和实现
- [ ] local_iterator vs iterator
- [ ] bucket_count 的素数选择

---

## 实践项目

### 项目：实现 mini_map<K, V>

**目标**：实现一个简化版的红黑树map

**要求**：
1. [ ] 基于红黑树实现
2. [ ] 支持插入、删除、查找
3. [ ] 支持迭代器（中序遍历）
4. [ ] 实现 `operator[]`、`at`、`find`
5. [ ] 可视化工具：输出树结构用于调试

**代码框架**：
```cpp
template <typename Key, typename Value, typename Compare = std::less<Key>>
class mini_map {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = std::size_t;

private:
    enum class Color { Red, Black };

    struct Node {
        value_type data;
        Color color;
        Node* parent;
        Node* left;
        Node* right;

        Node(const value_type& d)
            : data(d), color(Color::Red),
              parent(nullptr), left(nullptr), right(nullptr) {}
    };

    Node* root_ = nullptr;
    Node* nil_;  // 哨兵节点
    size_type size_ = 0;
    Compare comp_;

    // 核心操作
    void left_rotate(Node* x);
    void right_rotate(Node* x);
    void insert_fixup(Node* z);
    void delete_fixup(Node* x);
    void transplant(Node* u, Node* v);
    Node* minimum(Node* x) const;

public:
    class iterator {
        // 实现双向迭代器
    };

    mini_map();
    ~mini_map();

    // 元素访问
    mapped_type& operator[](const key_type& key);
    mapped_type& at(const key_type& key);

    // 容量
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }

    // 修改器
    std::pair<iterator, bool> insert(const value_type& value);
    size_type erase(const key_type& key);
    void clear();

    // 查找
    iterator find(const key_type& key);
    iterator lower_bound(const key_type& key);
    iterator upper_bound(const key_type& key);

    // 迭代器
    iterator begin();
    iterator end();

    // 调试
    void print_tree() const;
    bool verify_rb_properties() const;  // 验证红黑树性质
};
```

**测试用例**：
```cpp
void test_mini_map() {
    mini_map<int, std::string> m;

    // 插入测试
    m[1] = "one";
    m[2] = "two";
    m[3] = "three";
    assert(m.size() == 3);

    // 查找测试
    assert(m.find(2)->second == "two");
    assert(m.find(99) == m.end());

    // 有序性测试
    int prev = -1;
    for (const auto& [k, v] : m) {
        assert(k > prev);
        prev = k;
    }

    // 删除测试
    m.erase(2);
    assert(m.find(2) == m.end());

    // 红黑树性质验证
    assert(m.verify_rb_properties());

    std::cout << "All tests passed!\n";
}
```

### 项目：实现 mini_hash_map<K, V>

**目标**：实现一个简化版的链地址哈希表

**要求**：
1. [ ] 链地址法处理冲突
2. [ ] 支持自定义哈希函数
3. [ ] 实现自动rehash
4. [ ] 负载因子可配置

```cpp
template <typename Key, typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class mini_hash_map {
public:
    // 类似接口，但基于哈希表实现
    // ...
};
```

---

## 检验标准

### 知识检验
- [ ] 能够手绘红黑树的插入和删除过程
- [ ] 解释红黑树为什么能保证O(log n)操作
- [ ] 解释哈希表的负载因子对性能的影响
- [ ] 比较std::map和std::unordered_map的使用场景

### 实践检验
- [ ] mini_map通过所有测试，包括红黑树性质验证
- [ ] mini_hash_map通过所有测试，包括rehash正确性
- [ ] 完成性能对比实验并撰写分析报告

### 输出物
1. `mini_map.hpp` 红黑树实现
2. `mini_hash_map.hpp` 哈希表实现
3. `notes/month03_containers.md` 容器源码分析笔记
4. `benchmark_report.md` 性能对比分析报告

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习（数据结构） | 30小时 | 21% |
| 源码阅读分析 | 40小时 | 29% |
| mini_map开发 | 35小时 | 25% |
| mini_hash_map开发 | 25小时 | 18% |
| 性能测试与笔记 | 10小时 | 7% |

---

## 下月预告

Month 04将深入**智能指针与RAII模式**，分析`unique_ptr`、`shared_ptr`、`weak_ptr`的完整实现，理解所有权语义和引用计数的工程实现。
