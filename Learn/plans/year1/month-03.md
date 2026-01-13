# Month 03: STL容器源码深度分析——红黑树与哈希表

## 本月主题概述

本月将系统性地分析STL中两类最重要的关联容器：基于红黑树的`std::map/set`和基于哈希表的`std::unordered_map/set`。通过源码阅读，理解数据结构的工程化实现，培养"从抽象到实现"的思维能力。

**本月核心能力培养**：
- 🎯 数据结构的工程化思维：从教科书算法到生产级代码
- 🎯 性能分析直觉：理解常数因子、缓存效应、内存布局的影响
- 🎯 API设计哲学：理解STL设计者的权衡与取舍

---

## 第一周：红黑树理论基础与深度剖析

> **本周主题**：从数学证明到直觉理解，彻底掌握红黑树

### 1.1 学习目标

- [ ] 彻底理解红黑树的五个性质及其数学意义
- [ ] 能够独立推导红黑树的高度上界
- [ ] 掌握旋转操作的几何直觉
- [ ] 理解插入/删除的所有情况及修复策略

### 1.2 阅读材料

**必读**：
- [ ] 《算法导论》第13章：红黑树（精读，做笔记）
- [ ] 博客：Red-Black Trees Visualized (https://www.cs.usfca.edu/~galles/visualization/RedBlack.html)
- [ ] MIT 6.046J Lecture: Red-Black Trees (YouTube)

**选读（深入理解）**：
- [ ] 论文：A Dichromatic Framework for Balanced Trees (Guibas & Sedgewick, 1978)
- [ ] 论文：Left-Leaning Red-Black Trees (Sedgewick, 2008) - 更简洁的变体
- [ ] Chris Okasaki: Red-Black Trees in a Functional Setting

### 1.3 核心概念深度解析

#### 1.3.1 红黑树的五个性质

```
性质1: 每个节点非红即黑
性质2: 根节点是黑色
性质3: 叶子节点（NIL/哨兵）是黑色
性质4: 红节点的子节点必须是黑色（无连续红节点）
性质5: 从任一节点到其后代叶子的所有路径包含相同数目的黑节点（黑高相等）
```

**深度理解**：

| 性质 | 直觉解释 | 工程意义 |
|------|----------|----------|
| 性质1 | 二元标记，用于编码平衡信息 | 只需1bit存储，可与指针合并 |
| 性质2 | 提供固定的平衡起点 | 简化边界条件处理 |
| 性质3 | 统一叶子处理 | 哨兵节点减少空指针检查 |
| 性质4 | 限制任何路径的"膨胀" | 红节点是"借来的"高度 |
| 性质5 | 核心平衡约束 | 保证最长/最短路径比≤2 |

#### 1.3.2 为什么这些性质保证平衡？

**定理**：含有n个内部节点的红黑树高度至多为 2log₂(n+1)

**证明思路**：
```
设 bh(x) = 从节点x到叶子的黑色节点数（不含x本身）

引理1: 以x为根的子树至少包含 2^bh(x) - 1 个内部节点
证明: 归纳法
  - 基础: x是叶子，bh(x)=0，2^0-1=0 ✓
  - 归纳: x的子节点y的黑高至少为bh(x)-1
         子树节点数 ≥ 2×(2^(bh(x)-1) - 1) + 1 = 2^bh(x) - 1

引理2: 树高h的红黑树，根的黑高 bh ≥ h/2
证明: 由性质4，任何路径上红节点数 ≤ 黑节点数

结论: n ≥ 2^(h/2) - 1  →  h ≤ 2log₂(n+1)
```

**直觉理解**：
```
最短路径：全黑节点，长度 = bh(root)
最长路径：红黑交替，长度 = 2×bh(root)
因此：最长 ≤ 2 × 最短
```

#### 1.3.3 旋转操作的几何直觉

```
左旋 (Left Rotate at x):
        x                     y
       / \                   / \
      α   y       →         x   γ
         / \               / \
        β   γ             α   β

右旋 (Right Rotate at y):
        y                     x
       / \                   / \
      x   γ       →         α   y
     / \                       / \
    α   β                     β   γ
```

**关键理解**：
1. 旋转是**局部操作**，只涉及常数个指针修改
2. 旋转**保持BST性质**：中序遍历不变（α < x < β < y < γ）
3. 旋转**改变高度分布**：一边升高，一边降低

**代码实现核心**：
```cpp
void left_rotate(Node* x) {
    Node* y = x->right;        // y是x的右孩子

    // Step 1: 把y的左子树给x作为右子树
    x->right = y->left;
    if (y->left != nil_) {
        y->left->parent = x;
    }

    // Step 2: 更新y的父指针
    y->parent = x->parent;
    if (x->parent == nil_) {
        root_ = y;             // x是根，y成为新根
    } else if (x == x->parent->left) {
        x->parent->left = y;   // x是左孩子
    } else {
        x->parent->right = y;  // x是右孩子
    }

    // Step 3: x成为y的左孩子
    y->left = x;
    x->parent = y;
}
```

### 1.4 插入操作完全解析

#### 1.4.1 插入的基本流程

```
1. 按BST规则找到插入位置
2. 插入新节点，着色为红色（为什么？不破坏性质5！）
3. 修复可能违反的性质（主要是性质4：连续红节点）
```

#### 1.4.2 插入修复的三种情况

设z为新插入节点，p为父节点，g为祖父节点，u为叔节点

```
前提条件：z是红色，p是红色（违反性质4）

Case 1: 叔节点u是红色
        g(B)                    g(R) ← 递归向上处理
       /    \                  /    \
     p(R)   u(R)    →       p(B)   u(B)
     /                       /
   z(R)                    z(R)

   操作：p和u变黑，g变红，z指向g继续修复

Case 2: u是黑色，z是p的右孩子（内侧）
        g(B)                    g(B)
       /    \                  /    \
     p(R)   u(B)    →       z(R)   u(B)   → 转化为Case 3
     \                       /
      z(R)                 p(R)

   操作：对p左旋，转化为Case 3

Case 3: u是黑色，z是p的左孩子（外侧）
        g(B)                    p(B)
       /    \                  /    \
     p(R)   u(B)    →       z(R)   g(R)   ← 完成！
     /                              \
   z(R)                             u(B)

   操作：p变黑，g变红，对g右旋
```

**对称情况**：当p是g的右孩子时，左右对称处理

#### 1.4.3 插入修复的复杂度分析

```
Case 1: 可能递归向上，但每次黑高减1，最多O(log n)次
Case 2: 转化为Case 3，常数时间
Case 3: 终止修复，常数时间

总旋转次数：最多2次！（Case 2一次 + Case 3一次）
总时间复杂度：O(log n)（主要是Case 1的颜色翻转）
```

### 1.5 删除操作完全解析

#### 1.5.1 删除的基本流程

```
1. 按BST规则删除节点（可能需要找后继）
2. 如果删除的是黑色节点，需要修复（破坏了性质5）
3. 修复过程可能需要旋转和重新着色
```

#### 1.5.2 删除修复的四种情况

设x为替代被删节点的节点，w为x的兄弟节点

```
前提：x是"双重黑色"（少了一个黑色）

Case 1: w是红色
        p(B)                    w(B)
       /    \                  /    \
     x(BB)  w(R)    →       p(R)   wr(B)  → 转化为Case 2/3/4
            / \              / \
          wl   wr          x   wl(新w)

   操作：w变黑，p变红，对p左旋，更新w

Case 2: w是黑色，w的两个孩子都是黑色
        p(?)                    p(?) ← x上移
       /    \                  /    \
     x(BB)  w(B)    →       x(B)   w(R)
            / \                    / \
         wl(B) wr(B)            wl(B) wr(B)

   操作：w变红，x上移到p，可能需要继续修复

Case 3: w是黑色，w的左孩子红色，右孩子黑色
        p(?)                    p(?)
       /    \                  /    \
     x(BB)  w(B)    →       x(BB)  wl(B)  → 转化为Case 4
            / \                      \
         wl(R) wr(B)                 w(R)
                                      \
                                      wr(B)

   操作：wl变黑，w变红，对w右旋

Case 4: w是黑色，w的右孩子是红色
        p(?)                    w(p的颜色)
       /    \                  /    \
     x(BB)  w(B)    →       p(B)   wr(B)  ← 完成！
            / \              / \
          wl  wr(R)        x(B) wl

   操作：w取p的颜色，p变黑，wr变黑，对p左旋
```

#### 1.5.3 删除修复的复杂度分析

```
Case 1: 转化为Case 2/3/4，最多1次
Case 2: 可能递归向上，最多O(log n)次
Case 3: 转化为Case 4，最多1次
Case 4: 终止修复，最多1次

总旋转次数：最多3次！
总时间复杂度：O(log n)
```

### 1.6 手绘练习与思考题

**手绘练习**：
- [ ] 画出依次插入 [10, 20, 30, 15, 25, 5] 的红黑树变化过程
- [ ] 画出从上述树中删除 20 的完整过程
- [ ] 画出插入操作的3种情况的状态转换图

**思考题**：
1. [ ] 为什么新插入的节点总是红色？如果插入黑色会怎样？
2. [ ] 红黑树和2-3-4树的对应关系是什么？
3. [ ] 为什么删除比插入复杂？体现在哪里？
4. [ ] AVL树和红黑树的高度上界分别是多少？为什么红黑树更适合频繁修改的场景？

### 1.7 扩展阅读：红黑树的变体

| 变体 | 特点 | 应用场景 |
|------|------|----------|
| Left-Leaning RB Tree | 红链接只能在左边，代码更简洁 | 教学、简化实现 |
| AA Tree | 只有右倾斜的红节点 | 更容易实现 |
| 2-3 Tree | 红黑树的概念原型 | 理论分析 |
| B-Tree | 多路平衡树 | 数据库索引、文件系统 |

### 1.8 本周检验清单

- [ ] 能够不看资料写出红黑树的5个性质
- [ ] 能够证明红黑树的高度上界
- [ ] 能够手绘插入的3种情况
- [ ] 能够手绘删除的4种情况
- [ ] 理解为什么旋转次数是常数级别

---

## 第二周：std::map/set源码深度分析

> **本周主题**：从理论到工程，理解生产级红黑树实现

### 2.1 学习目标

- [ ] 理解STL红黑树的分层设计架构
- [ ] 掌握节点结构的内存布局优化
- [ ] 分析迭代器的实现与失效规则
- [ ] 理解allocator在容器中的角色

### 2.2 源码阅读路径

**GCC libstdc++ 源码结构**：
```
/usr/include/c++/[version]/
├── bits/
│   ├── stl_tree.h          ← 核心！红黑树实现
│   ├── stl_map.h           ← map包装器
│   ├── stl_set.h           ← set包装器
│   ├── stl_multimap.h      ← multimap包装器
│   └── stl_multiset.h      ← multiset包装器
├── map                      ← 头文件入口
└── set                      ← 头文件入口
```

**阅读顺序**：
1. [ ] `stl_tree.h` 的类声明部分（理解架构）
2. [ ] 节点结构：`_Rb_tree_node_base` 和 `_Rb_tree_node`
3. [ ] 迭代器：`_Rb_tree_iterator` 和 `_Rb_tree_const_iterator`
4. [ ] 核心类：`_Rb_tree` 的成员变量
5. [ ] 关键操作：insert、erase、find
6. [ ] `stl_map.h` - 理解包装器如何使用 `_Rb_tree`

### 2.3 源码深度解析

#### 2.3.1 节点结构设计

```cpp
// 基类：不含数据，用于指针操作
struct _Rb_tree_node_base {
    typedef _Rb_tree_node_base* _Base_ptr;
    typedef const _Rb_tree_node_base* _Const_Base_ptr;

    _Rb_tree_color _M_color;    // enum { _S_red = false, _S_black = true }
    _Base_ptr      _M_parent;   // 父节点指针
    _Base_ptr      _M_left;     // 左孩子
    _Base_ptr      _M_right;    // 右孩子

    // 辅助函数
    static _Base_ptr _S_minimum(_Base_ptr __x) noexcept;
    static _Base_ptr _S_maximum(_Base_ptr __x) noexcept;
};

// 派生类：包含实际数据
template<typename _Val>
struct _Rb_tree_node : public _Rb_tree_node_base {
    typedef _Rb_tree_node<_Val>* _Link_type;
    _Val _M_value_field;  // 存储的键值对

    _Val*       _M_valptr()       { return std::addressof(_M_value_field); }
    const _Val* _M_valptr() const { return std::addressof(_M_value_field); }
};
```

**设计分析**：

| 设计决策 | 原因 | 工程影响 |
|----------|------|----------|
| 基类不含值 | 允许用基类指针进行树操作 | 减少模板膨胀，header可以是基类型 |
| 颜色用enum | 类型安全 | 可读性好，编译器可优化为1字节 |
| 存储父指针 | 支持双向迭代器 | 空间换时间，O(1)找父节点 |
| value用对象存储 | 避免额外的指针间接 | 缓存友好，但要求值可构造 |

**内存布局分析**（64位系统）：
```
_Rb_tree_node<pair<const K, V>> 内存布局:
┌─────────────────┬──────────────────────────────────┐
│ _Rb_tree_node_base (32 bytes)                      │
│ ┌───────────┬───────────┬───────────┬───────────┐  │
│ │ color (8B)│ parent(8B)│ left (8B) │ right(8B) │  │
│ └───────────┴───────────┴───────────┴───────────┘  │
├─────────────────────────────────────────────────────┤
│ pair<const K, V> _M_value_field                    │
│ ┌─────────────────┬─────────────────────────────┐  │
│ │ const K (first) │ V (second)                  │  │
│ └─────────────────┴─────────────────────────────┘  │
└─────────────────────────────────────────────────────┘

注意：实际color可能只用1bit，但对齐到8字节
优化：某些实现将color编码到parent指针的最低位！
```

#### 2.3.2 Header节点设计（精妙之处）

```cpp
// _Rb_tree 的核心数据成员
struct _Rb_tree_impl {
    _Key_compare     _M_key_compare;  // 比较函数对象
    _Rb_tree_node_base _M_header;     // header节点（不存储数据）
    size_type        _M_node_count;   // 节点数量

    // header的特殊用法：
    // _M_header._M_parent  → 指向根节点
    // _M_header._M_left    → 指向最小节点（begin()）
    // _M_header._M_right   → 指向最大节点（--end()）
    // root._M_parent       → 指向_M_header
};
```

**Header节点的妙用**：
```
                    _M_header (哨兵)
                   /    |    \
              left    parent   right
             ↓         ↓        ↓
          [最小]    [根]     [最大]
            ↑                   ↑
            └───── 树结构 ──────┘

好处：
1. begin() = O(1)：直接返回 _M_header._M_left
2. end() = O(1)：返回 _M_header 本身的迭代器
3. 空树判断 = O(1)：_M_header._M_parent == &_M_header
4. 根节点的父指针有意义：简化旋转代码
```

#### 2.3.3 迭代器深度分析

```cpp
template<typename _Tp>
struct _Rb_tree_iterator {
    typedef _Tp  value_type;
    typedef _Tp& reference;
    typedef _Tp* pointer;
    typedef bidirectional_iterator_tag iterator_category;
    typedef ptrdiff_t difference_type;

    typedef _Rb_tree_node_base::_Base_ptr _Base_ptr;
    typedef _Rb_tree_node<_Tp>* _Link_type;

    _Base_ptr _M_node;  // 指向当前节点

    // 核心：++操作（找中序后继）
    _Self& operator++() noexcept {
        _M_node = _Rb_tree_increment(_M_node);
        return *this;
    }

    // 核心：--操作（找中序前驱）
    _Self& operator--() noexcept {
        _M_node = _Rb_tree_decrement(_M_node);
        return *this;
    }

    reference operator*() const noexcept {
        return *static_cast<_Link_type>(_M_node)->_M_valptr();
    }
};
```

**中序后继算法（_Rb_tree_increment）**：
```cpp
_Rb_tree_node_base* _Rb_tree_increment(_Rb_tree_node_base* __x) noexcept {
    // Case 1: 有右子树 → 右子树的最左节点
    if (__x->_M_right != 0) {
        __x = __x->_M_right;
        while (__x->_M_left != 0)
            __x = __x->_M_left;
    }
    // Case 2: 无右子树 → 向上找第一个"从左边来"的祖先
    else {
        _Rb_tree_node_base* __y = __x->_M_parent;
        while (__x == __y->_M_right) {
            __x = __y;
            __y = __y->_M_parent;
        }
        // 特殊情况：处理end()
        if (__x->_M_right != __y)
            __x = __y;
    }
    return __x;
}
```

**迭代器失效规则**：
```cpp
std::map<int, int> m = {{1,1}, {2,2}, {3,3}};

// 安全：erase返回下一个有效迭代器
for (auto it = m.begin(); it != m.end(); ) {
    if (it->first == 2)
        it = m.erase(it);  // ✓ C++11起安全
    else
        ++it;
}

// 危险：erase后迭代器失效
for (auto it = m.begin(); it != m.end(); ++it) {
    if (it->first == 2)
        m.erase(it);  // ✗ 未定义行为！
}

// map的特点：只有被删除的迭代器失效，其他保持有效
auto it1 = m.find(1);
auto it2 = m.find(2);
m.erase(it2);
// it1 仍然有效！（红黑树特性）
```

#### 2.3.4 插入操作源码追踪

```cpp
// map::operator[] 的实现
mapped_type& operator[](const key_type& __k) {
    // lower_bound: 找到第一个 >= __k 的位置
    iterator __i = lower_bound(__k);

    // 如果找到的位置key不等于__k，需要插入
    if (__i == end() || key_comp()(__k, (*__i).first))
        __i = _M_t._M_emplace_hint_unique(__i, piecewise_construct,
                                          forward_as_tuple(__k),
                                          tuple<>());
    return (*__i).second;
}

// 核心插入函数
pair<iterator, bool> _M_insert_unique(const value_type& __v) {
    // Step 1: 找插入位置
    pair<_Base_ptr, _Base_ptr> __res = _M_get_insert_unique_pos(_KeyOfValue()(__v));

    if (__res.second) {  // 可以插入（key不存在）
        // Step 2: 分配节点
        _Link_type __z = _M_create_node(__v);

        // Step 3: 插入并重平衡
        _Rb_tree_insert_and_rebalance(__res.first == __res.second,
                                       __z, __res.second, _M_impl._M_header);
        ++_M_impl._M_node_count;
        return pair<iterator, bool>(iterator(__z), true);
    }
    // key已存在
    return pair<iterator, bool>(iterator(__res.first), false);
}
```

**重平衡函数（最核心）**：
```cpp
void _Rb_tree_insert_and_rebalance(
    const bool __insert_left,      // 插入到左边还是右边
    _Rb_tree_node_base* __x,       // 新节点
    _Rb_tree_node_base* __p,       // 父节点
    _Rb_tree_node_base& __header)  // header节点
{
    _Rb_tree_node_base*& __root = __header._M_parent;

    // 初始化新节点
    __x->_M_parent = __p;
    __x->_M_left = 0;
    __x->_M_right = 0;
    __x->_M_color = _S_red;  // 新节点着红色

    // 链接到父节点
    if (__insert_left) {
        __p->_M_left = __x;
        if (__p == &__header) {  // 空树，新节点是根
            __header._M_parent = __x;
            __header._M_right = __x;
        } else if (__p == __header._M_left) {
            __header._M_left = __x;  // 更新最小值
        }
    } else {
        __p->_M_right = __x;
        if (__p == __header._M_right) {
            __header._M_right = __x;  // 更新最大值
        }
    }

    // 重平衡（这就是第一周学的算法！）
    while (__x != __root && __x->_M_parent->_M_color == _S_red) {
        // ... Case 1, 2, 3 的处理
        // （与第一周理论完全对应）
    }
    __root->_M_color = _S_black;  // 根永远是黑色
}
```

### 2.4 map与set的关系

```cpp
// map 本质上是对 _Rb_tree 的薄包装
template<typename _Key, typename _Tp, typename _Compare, typename _Alloc>
class map {
    typedef _Rb_tree<key_type, value_type, _Select1st<value_type>,
                     key_compare, _Pair_alloc_type> _Rep_type;
    _Rep_type _M_t;  // 唯一的成员变量！

public:
    // 所有操作都委托给 _M_t
    iterator find(const key_type& __x) { return _M_t.find(__x); }
    iterator begin() noexcept { return _M_t.begin(); }
    size_type size() const noexcept { return _M_t.size(); }
    // ...
};

// set 类似，但 key 和 value 是同一个
template<typename _Key, typename _Compare, typename _Alloc>
class set {
    typedef _Rb_tree<key_type, key_type, _Identity<key_type>,
                     key_compare, _Key_alloc_type> _Rep_type;
    _Rep_type _M_t;
    // ...
};
```

**设计洞察**：
- `map` 用 `_Select1st` 从 `pair<K,V>` 提取 key
- `set` 用 `_Identity` 表示 key 就是 value 本身
- 底层共用同一套红黑树代码，通过策略类区分

### 2.5 GDB调试实战

**调试任务**：
```cpp
// debug_rbtree.cpp
#include <map>
#include <iostream>

int main() {
    std::map<int, std::string> m;

    // 设置断点，观察每次插入后的树结构
    m[5] = "five";   // 根节点
    m[3] = "three";  // 左子树
    m[7] = "seven";  // 右子树
    m[1] = "one";    // 触发重平衡？
    m[4] = "four";   // 观察结构变化
    m[6] = "six";
    m[8] = "eight";

    return 0;
}
```

**GDB命令指南**：
```bash
# 编译（带调试信息）
g++ -g -O0 debug_rbtree.cpp -o debug_rbtree

# 启动GDB
gdb ./debug_rbtree

# 设置断点
(gdb) break main
(gdb) run

# 查看map内部结构
(gdb) p m._M_t._M_impl._M_header          # header节点
(gdb) p m._M_t._M_impl._M_header._M_parent # 根节点
(gdb) p m._M_t._M_impl._M_node_count       # 节点数

# 遍历树结构（自定义函数）
define print_node
    set $node = $arg0
    if $node != 0
        printf "Node: %p, Color: %s, Value: %d\n", $node, \
               $node->_M_color == 0 ? "RED" : "BLACK", \
               ((std::_Rb_tree_node<std::pair<const int, std::string>>*)$node)->_M_value_field.first
        print_node $node->_M_left
        print_node $node->_M_right
    end
end
```

**LLDB（macOS）命令**：
```bash
lldb ./debug_rbtree

(lldb) breakpoint set -n main
(lldb) run

# 打印map大小
(lldb) p m.size()

# 查看根节点
(lldb) p m._M_t._M_impl._M_header._M_parent
```

### 2.6 思考题与深入问题

1. [ ] 为什么 `map::operator[]` 对于不存在的key会插入默认值？这带来什么问题？
   ```cpp
   std::map<int, int> m;
   if (m[5] > 0) { ... }  // 陷阱：这会插入 m[5] = 0
   ```

2. [ ] `lower_bound` 和 `upper_bound` 的区别是什么？为什么需要两个？
   ```cpp
   // lower_bound: 第一个 >= key 的位置
   // upper_bound: 第一个 > key 的位置
   // 用途：m.equal_range(k) 返回 [lower_bound, upper_bound)
   ```

3. [ ] 为什么 `map::insert` 不会覆盖已存在的值，而 `operator[]` 会？

4. [ ] 如何实现自定义比较器？什么是严格弱序？
   ```cpp
   // 错误示例：不满足严格弱序
   struct BadCompare {
       bool operator()(int a, int b) const {
           return a <= b;  // ✗ 错误！应该是 a < b
       }
   };
   ```

5. [ ] 为什么 `map<K,V>::iterator` 的 `first` 是 `const K` 而不是 `K`？

### 2.7 扩展阅读

**对比不同实现**：
- [ ] LLVM libc++ 的实现（`__tree`）
- [ ] MSVC STL 的实现（`_Tree`）
- [ ] Boost.Container 的 `flat_map`（基于排序vector）

**性能优化技术**：
| 技术 | 说明 | 影响 |
|------|------|------|
| Node handle (C++17) | 节点可以在容器间转移 | 减少拷贝 |
| Heterogeneous lookup (C++14) | 查找时可以用不同类型 | 避免构造临时key |
| try_emplace (C++17) | 只在key不存在时构造value | 避免不必要的构造 |

### 2.8 本周检验清单

- [ ] 能够解释 `_Rb_tree_node_base` 和 `_Rb_tree_node` 分离的原因
- [ ] 能够解释 header 节点的三个指针的用途
- [ ] 理解 `++iterator` 如何找到中序后继
- [ ] 能够使用GDB/LLDB观察红黑树结构
- [ ] 理解 map 和 set 如何复用同一套红黑树代码

---

## 第三周：哈希表理论与std::unordered_map源码分析

> **本周主题**：从数学原理到工程实现，掌握哈希表的奥秘

### 3.1 学习目标

- [ ] 理解哈希函数的设计原理与评判标准
- [ ] 掌握冲突解决的多种策略及其权衡
- [ ] 深入分析STL哈希表的实现细节
- [ ] 理解负载因子、rehash与性能的关系

### 3.2 阅读材料

**必读**：
- [ ] 《算法导论》第11章：散列表（精读）
- [ ] CppCon演讲："std::unordered_map: Inside and Out" (Matt Kulukundis)
- [ ] CppCon 2017: "Designing a Fast, Efficient, Cache-friendly Hash Table"

**选读（深入理解）**：
- [ ] 论文：Robin Hood Hashing (Pedro Celis, 1986)
- [ ] 博客：Swiss Table (Google's flat_hash_map)
- [ ] 博客：Facebook's F14 Hash Table

### 3.3 哈希函数深度解析

#### 3.3.1 什么是好的哈希函数？

**数学定义**：
```
设 U 为全域（所有可能的键），m 为桶数量
哈希函数 h: U → {0, 1, ..., m-1}

简单均匀散列假设（SUHA）：
每个键等可能地被映射到 m 个桶中的任何一个
```

**好的哈希函数的特性**：

| 特性 | 定义 | 重要性 |
|------|------|--------|
| 确定性 | 相同输入总是产生相同输出 | 必须 |
| 均匀分布 | 输出在 [0, m) 均匀分布 | 性能关键 |
| 雪崩效应 | 输入小变化 → 输出大变化 | 安全、分布 |
| 计算效率 | O(1) 时间复杂度 | 实用性 |
| 抗碰撞 | 难以找到碰撞对 | 安全（可选）|

#### 3.3.2 常见哈希函数分析

**除法散列法**：
```cpp
h(k) = k mod m

// m的选择很重要！
// 坏：m = 2^p（只看低p位）
// 好：m = 素数，且不接近2的幂
```

**乘法散列法**：
```cpp
h(k) = floor(m × (k × A mod 1))

// A推荐值：黄金分割率的倒数
// A = (√5 - 1) / 2 ≈ 0.6180339887

// Knuth建议：A = 2654435769 / 2^32
```

**FNV-1a（简单、快速）**：
```cpp
uint64_t fnv1a_hash(const char* data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 1099511628211ULL;  // FNV prime
    }
    return hash;
}
```

**MurmurHash3（生产级）**：
```cpp
// 特点：高质量、快速、开源
// 用途：Redis、Cassandra、Hadoop
// 非加密哈希，不适合安全场景
```

#### 3.3.3 std::hash 的实现

```cpp
// GCC libstdc++ 中的 std::hash 特化

// 整数类型：通常直接返回（或做简单变换）
template<>
struct hash<int> {
    size_t operator()(int __val) const noexcept {
        return static_cast<size_t>(__val);
    }
};

// 指针类型：将地址转换为整数
template<typename _Tp>
struct hash<_Tp*> {
    size_t operator()(_Tp* __p) const noexcept {
        return reinterpret_cast<size_t>(__p);
    }
};

// 字符串：FNV-like 哈希
template<>
struct hash<string> {
    size_t operator()(const string& __s) const noexcept {
        return _Hash_impl::hash(__s.data(), __s.length());
    }
};
```

**自定义类型的哈希**：
```cpp
struct Person {
    std::string name;
    int age;

    bool operator==(const Person& other) const {
        return name == other.name && age == other.age;
    }
};

// 方法1：特化 std::hash
template<>
struct std::hash<Person> {
    size_t operator()(const Person& p) const {
        // 组合哈希值的常用技术
        size_t h1 = std::hash<std::string>{}(p.name);
        size_t h2 = std::hash<int>{}(p.age);
        return h1 ^ (h2 << 1);  // 简单组合
    }
};

// 方法2：使用 boost::hash_combine（推荐）
template<>
struct std::hash<Person> {
    size_t operator()(const Person& p) const {
        size_t seed = 0;
        // hash_combine 的实现
        auto hash_combine = [&seed](size_t h) {
            seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        hash_combine(std::hash<std::string>{}(p.name));
        hash_combine(std::hash<int>{}(p.age));
        return seed;
    }
};
```

### 3.4 冲突解决策略对比

#### 3.4.1 链地址法（Chaining）- STL采用

```
桶数组：
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │
└─┬─┴───┴─┬─┴───┴─┬─┴───┴───┴───┘
  │       │       │
  ▼       ▼       ▼
 [A]     [B]     [E]
  │       │
  ▼       ▼
 [C]     [D]
  │
  ▼
 [F]

特点：
- 简单直观
- 负载因子可以 > 1
- 最坏情况 O(n)（所有元素在同一桶）
- 缓存不友好（链表遍历）
```

#### 3.4.2 开放寻址法（Open Addressing）

```
线性探测：h(k, i) = (h(k) + i) mod m

┌───┬───┬───┬───┬───┬───┬───┬───┐
│ A │ B │ C │   │ D │ E │   │   │
└───┴───┴───┴───┴───┴───┴───┴───┘

插入 X，h(X) = 1：
位置1被占 → 探测位置2 → 被占 → 位置3 → 空，插入！

优点：缓存友好、内存紧凑
缺点：聚集问题、删除复杂、负载因子必须 < 1
```

**探测序列对比**：

| 方法 | 公式 | 优点 | 缺点 |
|------|------|------|------|
| 线性探测 | h(k) + i | 缓存友好 | 一次聚集 |
| 二次探测 | h(k) + c₁i + c₂i² | 减少聚集 | 可能遗漏桶 |
| 双重哈希 | h₁(k) + i×h₂(k) | 分布好 | 两次哈希计算 |

#### 3.4.3 Robin Hood Hashing（现代优化）

```
核心思想：贫富均衡
如果新元素的"偏移"大于当前元素的"偏移"，交换它们

偏移 = 当前位置 - 理想位置

插入过程：
1. 计算新元素的理想位置
2. 如果位置被占，比较偏移
3. 如果新元素偏移更大，交换，继续为被换出的元素找位置

结果：所有元素的探测距离更均匀
     查找性能的方差大大降低
```

#### 3.4.4 策略选择指南

| 场景 | 推荐策略 | 原因 |
|------|----------|------|
| 通用场景 | 链地址法 | 简单、稳定 |
| 高性能要求 | Robin Hood + 开放寻址 | 缓存友好 |
| 内存受限 | 开放寻址 | 无指针开销 |
| 高并发 | 链地址法 + 细粒度锁 | 锁竞争小 |

### 3.5 STL unordered_map 源码分析

#### 3.5.1 源码结构

```
/usr/include/c++/[version]/
├── bits/
│   ├── hashtable.h            ← 核心！哈希表实现
│   ├── hashtable_policy.h     ← 策略类（桶数量、rehash策略）
│   ├── unordered_map.h        ← unordered_map 包装器
│   └── unordered_set.h        ← unordered_set 包装器
```

#### 3.5.2 _Hashtable 核心数据结构

```cpp
template<typename _Key, typename _Value, typename _Alloc,
         typename _ExtractKey, typename _Equal,
         typename _Hash, typename _RangeHash, typename _Unused,
         typename _RehashPolicy, typename _Traits>
class _Hashtable {
    // 节点结构
    struct _Hash_node_base {
        _Hash_node_base* _M_nxt;  // 指向下一个节点
    };

    template<typename _Value>
    struct _Hash_node : _Hash_node_base {
        _Value _M_v;              // 存储的值
        size_t _M_hash_code;      // 缓存的哈希值（可选）
    };

    // 核心成员
    _Node_allocator_type  _M_node_allocator;    // 节点分配器
    __bucket_type*        _M_buckets;           // 桶数组
    size_type             _M_bucket_count;      // 桶数量
    __node_base           _M_before_begin;      // 链表头（哨兵）
    size_type             _M_element_count;     // 元素数量
    _RehashPolicy         _M_rehash_policy;     // rehash策略
    _Hash                 _M_hash;              // 哈希函数
    _Equal                _M_equal;             // 相等函数
};
```

#### 3.5.3 内存布局深度分析

```
STL unordered_map 的巧妙设计：单链表 + 桶索引

所有节点连成一个单链表：
_M_before_begin → [node1] → [node2] → [node3] → [node4] → nullptr

桶数组存储的是指向"前一个节点"的指针：
_M_buckets[i] 指向桶i中第一个元素的前一个节点

示意图：
_M_before_begin ──→ [A,h=0] ──→ [B,h=2] ──→ [C,h=0] ──→ [D,h=2] ──→ null
                      │              │
_M_buckets[0] ────────┘              │
_M_buckets[1] = _M_before_begin      │
_M_buckets[2] = A ───────────────────┘

好处：
1. 遍历是O(n)而不是O(bucket_count)
2. begin() 直接返回 _M_before_begin->_M_nxt
3. 只需单向指针，内存更紧凑
```

#### 3.5.4 关键操作分析

**查找操作**：
```cpp
iterator find(const key_type& __k) {
    // 1. 计算哈希值
    size_type __hash_code = _M_hash(__k);

    // 2. 计算桶索引
    size_type __bkt = _M_bucket_index(__hash_code);

    // 3. 在桶的链表中查找
    _Node_base* __p = _M_buckets[__bkt];
    if (!__p)
        return end();

    _Node* __n = static_cast<_Node*>(__p->_M_nxt);
    while (__n) {
        // 检查是否在同一桶
        if (_M_bucket_index(__n->_M_hash_code) != __bkt)
            break;

        // 比较：先比较哈希值（快），再比较key（慢）
        if (__n->_M_hash_code == __hash_code &&
            _M_equal(__k, _ExtractKey()(__n->_M_v))) {
            return iterator(__n);
        }
        __n = __n->_M_next();
    }
    return end();
}
```

**插入操作**：
```cpp
pair<iterator, bool> insert(const value_type& __v) {
    // 1. 计算哈希值
    size_type __hash_code = _M_hash(_ExtractKey()(__v));

    // 2. 检查是否需要rehash
    if (_M_rehash_policy._M_need_rehash(_M_bucket_count, _M_element_count, 1)) {
        _M_rehash(_M_rehash_policy._M_next_bkt(_M_bucket_count));
    }

    // 3. 检查key是否已存在
    size_type __bkt = _M_bucket_index(__hash_code);
    if (_Node* __p = _M_find_node(__bkt, __hash_code, _ExtractKey()(__v))) {
        return {iterator(__p), false};  // 已存在，返回false
    }

    // 4. 创建新节点并插入
    _Node* __n = _M_allocate_node(__v);
    __n->_M_hash_code = __hash_code;
    _M_insert_bucket_begin(__bkt, __n);
    ++_M_element_count;
    return {iterator(__n), true};
}
```

### 3.6 负载因子与Rehash

#### 3.6.1 负载因子（Load Factor）

```
负载因子 α = n / m
其中：n = 元素数量，m = 桶数量

负载因子对性能的影响：
┌───────────┬────────────────────────────────────────┐
│ α值       │ 影响                                    │
├───────────┼────────────────────────────────────────┤
│ α < 0.5   │ 空间浪费，但查找非常快                  │
│ α ≈ 0.7   │ 开放寻址的推荐值                        │
│ α ≈ 1.0   │ STL默认值，链地址法的合理选择           │
│ α > 1.0   │ 链地址法可以，但性能下降                │
│ α → ∞     │ 退化为链表，O(n)查找                    │
└───────────┴────────────────────────────────────────┘
```

#### 3.6.2 Rehash机制

```cpp
// STL的rehash策略

// 素数桶数量序列（部分）
static const size_t __prime_list[] = {
    53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593,
    49157, 98317, 196613, 393241, 786433, 1572869, ...
};

void rehash(size_type __n) {
    // 1. 确定新的桶数量（下一个素数）
    size_type __new_bkt_count = _M_rehash_policy._M_next_bkt(__n);

    if (__new_bkt_count > _M_bucket_count) {
        // 2. 分配新桶数组
        __bucket_type* __new_buckets = _M_allocate_buckets(__new_bkt_count);

        // 3. 遍历所有节点，重新分配到新桶
        for (_Node* __p = _M_begin(); __p; ) {
            _Node* __next = __p->_M_next();
            size_type __new_bkt = _M_bucket_index(__p->_M_hash_code,
                                                   __new_bkt_count);
            // 将节点移动到新桶
            _M_insert_bucket_begin(__new_bkt, __p, __new_buckets);
            __p = __next;
        }

        // 4. 释放旧桶数组，更新成员
        _M_deallocate_buckets(_M_buckets, _M_bucket_count);
        _M_buckets = __new_buckets;
        _M_bucket_count = __new_bkt_count;
    }
}
```

**Rehash的时机**：
```cpp
// 插入前检查
if (load_factor() > max_load_factor()) {
    rehash(bucket_count() * 2);  // 实际会找下一个素数
}

// 用户可以预留空间
std::unordered_map<int, int> m;
m.reserve(10000);  // 预分配，避免后续rehash
```

### 3.7 性能陷阱与最佳实践

#### 3.7.1 哈希攻击（Hash Flooding）

```cpp
// 恶意输入可能导致所有元素落入同一桶
// 这会使 O(1) 退化为 O(n)

// 防护方法：
// 1. 使用随机种子
// 2. 使用SipHash等加密哈希（Python 3.4+采用）
// 3. 限制单桶长度
```

#### 3.7.2 正确使用unordered容器

```cpp
// 好：预留空间
std::unordered_map<int, int> m;
m.reserve(n);  // 避免多次rehash

// 好：使用emplace避免不必要的拷贝
m.emplace(key, value);

// 好：查找时使用find而不是operator[]
if (m.find(key) != m.end()) { ... }
// 坏：operator[]会插入默认值
if (m[key] > 0) { ... }  // 可能意外插入！

// 好：批量插入前禁用rehash
m.max_load_factor(std::numeric_limits<float>::infinity());
for (auto& kv : data) m.insert(kv);
m.max_load_factor(1.0);
m.rehash(m.size());  // 一次性rehash
```

### 3.8 思考题

1. [ ] 为什么STL选择链地址法而不是开放寻址？
2. [ ] 为什么桶数量要选择素数？如果用2的幂会怎样？
3. [ ] `unordered_map::bucket(key)` 的时间复杂度是多少？
4. [ ] 为什么默认 `max_load_factor` 是 1.0？
5. [ ] 如何设计一个支持并发访问的哈希表？

### 3.9 本周检验清单

- [ ] 能够解释 SUHA（简单均匀散列假设）
- [ ] 能够实现一个简单的哈希函数
- [ ] 理解链地址法和开放寻址的权衡
- [ ] 能够阅读 `bits/hashtable.h` 的核心代码
- [ ] 理解 rehash 的触发条件和实现

---

## 第四周：容器性能对比、选择策略与综合实践

> **本周主题**：综合运用所学知识，建立容器选择的系统性认知

### 4.1 学习目标

- [ ] 通过实验理解不同容器的性能特性
- [ ] 建立容器选择的决策框架
- [ ] 完成mini_map和mini_hash_map实现
- [ ] 掌握性能测试和分析方法

### 4.2 容器性能对比实验

#### 4.2.1 基准测试代码

```cpp
// benchmark_containers.cpp
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <chrono>
#include <random>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>

class Timer {
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start_;
public:
    Timer() : start_(Clock::now()) {}
    double elapsed_ms() const {
        auto end = Clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
};

template <typename Container>
void benchmark_insert(Container& c, const std::vector<int>& keys,
                      const std::string& name) {
    Timer t;
    for (int k : keys) {
        c.insert({k, k});
    }
    std::cout << std::setw(25) << name << " insert: "
              << std::fixed << std::setprecision(2)
              << t.elapsed_ms() << " ms\n";
}

template <typename Container>
void benchmark_find(Container& c, const std::vector<int>& keys,
                    const std::string& name) {
    Timer t;
    volatile int64_t sum = 0;
    for (int k : keys) {
        auto it = c.find(k);
        if (it != c.end()) sum += it->second;
    }
    std::cout << std::setw(25) << name << " find:   "
              << std::fixed << std::setprecision(2)
              << t.elapsed_ms() << " ms\n";
}

template <typename Container>
void benchmark_iterate(Container& c, const std::string& name) {
    Timer t;
    volatile int64_t sum = 0;
    for (const auto& [k, v] : c) {
        sum += v;
    }
    std::cout << std::setw(25) << name << " iterate: "
              << std::fixed << std::setprecision(2)
              << t.elapsed_ms() << " ms\n";
}

template <typename Container>
void benchmark_erase(Container& c, const std::vector<int>& keys,
                     const std::string& name) {
    Timer t;
    for (int k : keys) {
        c.erase(k);
    }
    std::cout << std::setw(25) << name << " erase:  "
              << std::fixed << std::setprecision(2)
              << t.elapsed_ms() << " ms\n";
}

void run_benchmark(size_t n, const std::string& scenario) {
    std::cout << "\n========== " << scenario << " (n=" << n << ") ==========\n";

    std::vector<int> keys(n);
    std::iota(keys.begin(), keys.end(), 0);

    // 根据场景选择数据分布
    if (scenario == "Random") {
        std::shuffle(keys.begin(), keys.end(), std::mt19937{42});
    }
    // Sequential: 保持有序

    std::map<int, int> m;
    std::unordered_map<int, int> um;

    // 预留空间（公平比较）
    um.reserve(n);

    std::cout << "\n--- Insert ---\n";
    benchmark_insert(m, keys, "std::map");
    benchmark_insert(um, keys, "std::unordered_map");

    std::cout << "\n--- Find (all keys) ---\n";
    std::shuffle(keys.begin(), keys.end(), std::mt19937{123});
    benchmark_find(m, keys, "std::map");
    benchmark_find(um, keys, "std::unordered_map");

    std::cout << "\n--- Iterate ---\n";
    benchmark_iterate(m, "std::map");
    benchmark_iterate(um, "std::unordered_map");

    std::cout << "\n--- Erase ---\n";
    auto keys_copy = keys;
    std::map<int, int> m2 = m;
    std::unordered_map<int, int> um2 = um;
    benchmark_erase(m2, keys_copy, "std::map");
    benchmark_erase(um2, keys_copy, "std::unordered_map");
}

int main() {
    std::cout << "Container Performance Benchmark\n";
    std::cout << "================================\n";

    // 不同规模
    for (size_t n : {1000, 10000, 100000, 1000000}) {
        run_benchmark(n, "Random");
    }

    // 顺序插入（对map有利）
    run_benchmark(100000, "Sequential");

    return 0;
}
```

#### 4.2.2 预期结果分析

**典型结果（仅供参考，实际结果因硬件而异）**：

| 操作 | n=1,000 | n=10,000 | n=100,000 | n=1,000,000 |
|------|---------|----------|-----------|-------------|
| map insert | 0.2ms | 3ms | 50ms | 800ms |
| unordered_map insert | 0.1ms | 1ms | 15ms | 200ms |
| map find | 0.1ms | 2ms | 30ms | 500ms |
| unordered_map find | 0.05ms | 0.5ms | 5ms | 60ms |

**分析要点**：
```
1. 小规模数据（n < 1000）
   - 差异不明显
   - map可能更快（常数因子小）

2. 中等规模（1000 < n < 100000）
   - unordered_map开始领先
   - 差距约2-5倍

3. 大规模数据（n > 100000）
   - unordered_map明显更快
   - 差距可达10倍以上

4. 遍历操作
   - map更快！（连续内存访问vs链表跳转）
   - 如果需要频繁遍历，考虑map或flat_map
```

### 4.3 容器选择决策框架

#### 4.3.1 决策流程图

```
                    需要关联容器？
                         │
              ┌─────────┴─────────┐
              ▼                   ▼
         需要排序？             不需要排序
              │                   │
    ┌────────┴────────┐          │
    ▼                 ▼          ▼
  经常遍历？      点查询为主？   unordered_*
    │                 │
    ▼                 ▼
  map/set         map/set
 或flat_map      （考虑内存局部性）

              额外考虑因素：
              - 自定义类型哈希的复杂度
              - 迭代器稳定性需求
              - 内存使用限制
              - 线程安全需求
```

#### 4.3.2 详细选择指南

| 场景 | 推荐容器 | 原因 |
|------|----------|------|
| 需要有序遍历 | map/set | 红黑树保证有序 |
| 纯查找/插入 | unordered_* | O(1) vs O(log n) |
| 范围查询 | map/set | lower_bound/upper_bound |
| 频繁插入删除 | map/set | 迭代器稳定性好 |
| 内存敏感 | unordered_* + reserve | 可预分配 |
| 小数据量 (<100) | 任意，或vector | 差异不大 |
| 自定义类型key | map（更简单） | 只需 operator< |
| 字符串key | unordered_* | 哈希通常更快 |
| 需要最大/最小 | map/set | O(1) begin()/rbegin() |

#### 4.3.3 特殊场景对比

**场景1：需要频繁的范围查询**
```cpp
// map 的优势
std::map<int, int> m;
// 查找 [100, 200] 范围内的所有元素
auto lo = m.lower_bound(100);
auto hi = m.upper_bound(200);
for (auto it = lo; it != hi; ++it) {
    // O(log n + k)，k是范围内元素数
}

// unordered_map 无法高效实现！
// 必须遍历所有元素 O(n)
```

**场景2：需要第k大元素**
```cpp
// map 可以近似实现（需要额外记录大小）
// 但标准库不直接支持

// 考虑：
// - 如果频繁需要：使用专门的order statistic tree
// - 偶尔需要：遍历set/map
```

**场景3：LRU Cache**
```cpp
// 经典实现：unordered_map + 双向链表
template<typename K, typename V>
class LRUCache {
    int capacity_;
    std::list<std::pair<K, V>> items_;  // 双向链表
    std::unordered_map<K, typename std::list<std::pair<K,V>>::iterator> cache_;

public:
    V* get(const K& key) {
        auto it = cache_.find(key);
        if (it == cache_.end()) return nullptr;

        // 移动到链表头部
        items_.splice(items_.begin(), items_, it->second);
        return &it->second->second;
    }

    void put(const K& key, const V& value) {
        // ... 实现
    }
};
```

### 4.4 现代替代方案

#### 4.4.1 Boost容器

| 容器 | 特点 | 使用场景 |
|------|------|----------|
| `boost::flat_map` | 基于排序vector | 查找快、遍历快、适合只读 |
| `boost::unordered_flat_map` | 开放寻址 | 高性能哈希表 |
| `boost::multi_index` | 多索引容器 | 需要多种访问方式 |

#### 4.4.2 第三方高性能容器

```cpp
// Google's Abseil (absl::flat_hash_map)
// - Swiss Table实现
// - 比std::unordered_map快2-3倍
#include "absl/container/flat_hash_map.h"
absl::flat_hash_map<int, int> m;

// Facebook's F14 (folly::F14FastMap)
// - SIMD加速
// - 极致性能
#include "folly/container/F14Map.h"
folly::F14FastMap<int, int> m;

// Robin Hood Hashing (robin_hood::unordered_map)
// - Header-only
// - 性能优秀
#include "robin_hood.h"
robin_hood::unordered_map<int, int> m;
```

### 4.5 综合实践项目

#### 4.5.1 mini_map 实现提示

```cpp
// 关键实现点

// 1. NIL节点设计
// 使用单个静态NIL节点，而不是为每个叶子分配
class mini_map {
    static Node NIL_NODE;  // 静态NIL
    Node* nil_ = &NIL_NODE;
};

// 2. 插入修复的实现
void insert_fixup(Node* z) {
    while (z->parent->color == Color::Red) {
        if (z->parent == z->parent->parent->left) {
            Node* y = z->parent->parent->right;  // 叔节点
            if (y->color == Color::Red) {
                // Case 1
                z->parent->color = Color::Black;
                y->color = Color::Black;
                z->parent->parent->color = Color::Red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    // Case 2
                    z = z->parent;
                    left_rotate(z);
                }
                // Case 3
                z->parent->color = Color::Black;
                z->parent->parent->color = Color::Red;
                right_rotate(z->parent->parent);
            }
        } else {
            // 对称情况
        }
    }
    root_->color = Color::Black;
}

// 3. 迭代器实现
class iterator {
    Node* node_;
    Node* nil_;

public:
    iterator& operator++() {
        if (node_->right != nil_) {
            // 有右子树：找右子树最小
            node_ = node_->right;
            while (node_->left != nil_)
                node_ = node_->left;
        } else {
            // 无右子树：向上找
            Node* p = node_->parent;
            while (p != nil_ && node_ == p->right) {
                node_ = p;
                p = p->parent;
            }
            node_ = p;
        }
        return *this;
    }
};

// 4. 红黑树性质验证
bool verify_rb_properties() const {
    if (root_ == nil_) return true;

    // 性质2: 根是黑色
    if (root_->color != Color::Black) return false;

    // 性质4 & 5: 递归检查
    int black_count = -1;
    return verify_node(root_, 0, black_count);
}

bool verify_node(Node* n, int count, int& black_count) const {
    if (n == nil_) {
        if (black_count == -1) black_count = count;
        return count == black_count;  // 性质5
    }

    // 性质4: 红节点的子节点是黑色
    if (n->color == Color::Red) {
        if (n->left->color == Color::Red ||
            n->right->color == Color::Red)
            return false;
    }

    int new_count = count + (n->color == Color::Black ? 1 : 0);
    return verify_node(n->left, new_count, black_count) &&
           verify_node(n->right, new_count, black_count);
}
```

#### 4.5.2 mini_hash_map 实现提示

```cpp
// 关键实现点

// 1. 桶设计
template<typename K, typename V>
class mini_hash_map {
    struct Node {
        std::pair<const K, V> kv;
        Node* next;
        size_t hash_code;  // 缓存哈希值
    };

    std::vector<Node*> buckets_;
    size_t size_ = 0;
    float max_load_factor_ = 1.0f;
    Hash hash_fn_;
    KeyEqual equal_fn_;

    // 素数表
    static constexpr size_t primes[] = {
        53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, ...
    };
};

// 2. 查找实现
iterator find(const K& key) {
    if (buckets_.empty()) return end();

    size_t h = hash_fn_(key);
    size_t idx = h % buckets_.size();

    for (Node* n = buckets_[idx]; n != nullptr; n = n->next) {
        // 先比较哈希值（快），再比较key（慢）
        if (n->hash_code == h && equal_fn_(n->kv.first, key)) {
            return iterator(this, idx, n);
        }
    }
    return end();
}

// 3. Rehash实现
void rehash(size_t new_bucket_count) {
    // 找下一个素数
    new_bucket_count = next_prime(new_bucket_count);

    std::vector<Node*> new_buckets(new_bucket_count, nullptr);

    // 移动所有节点
    for (Node* head : buckets_) {
        while (head) {
            Node* next = head->next;

            // 计算新索引（使用缓存的hash_code）
            size_t new_idx = head->hash_code % new_bucket_count;

            // 头插法
            head->next = new_buckets[new_idx];
            new_buckets[new_idx] = head;

            head = next;
        }
    }

    buckets_ = std::move(new_buckets);
}

// 4. 插入时检查rehash
std::pair<iterator, bool> insert(const std::pair<K, V>& kv) {
    // 检查是否需要rehash
    if (size_ + 1 > buckets_.size() * max_load_factor_) {
        rehash(buckets_.size() * 2);
    }

    // ... 插入逻辑
}
```

### 4.6 性能测试报告模板

```markdown
# 容器性能对比分析报告

## 1. 测试环境
- CPU: [型号]
- 内存: [容量]
- 编译器: [版本]
- 优化级别: -O2

## 2. 测试数据
- 数据规模: 1000, 10000, 100000, 1000000
- 数据分布: 随机、顺序、部分有序

## 3. 测试结果

### 3.1 插入性能
[图表或表格]

### 3.2 查找性能
[图表或表格]

### 3.3 遍历性能
[图表或表格]

## 4. 分析与结论
- map优势场景: [...]
- unordered_map优势场景: [...]
- 临界点分析: [...]

## 5. 建议
[基于数据的容器选择建议]
```

### 4.7 本周检验清单

- [ ] 完成性能测试程序并分析结果
- [ ] mini_map 通过所有测试，包括红黑树性质验证
- [ ] mini_hash_map 通过所有测试，包括 rehash 正确性
- [ ] 撰写性能对比分析报告
- [ ] 能够为给定场景选择合适的容器并说明理由

---

## 源码阅读任务总结

### 深度阅读清单

#### std::map/set 实现细节（第二周重点）
- [ ] `bits/stl_tree.h` 完整阅读
- [ ] `_Rb_tree_insert_and_rebalance` 函数（插入后平衡）
- [ ] `_Rb_tree_rebalance_for_erase` 函数（删除后平衡）
- [ ] `_Rb_tree_iterator` 和中序遍历实现
- [ ] `lower_bound` 和 `upper_bound` 的二分查找实现
- [ ] header节点的设计和用途

#### std::unordered_map/set 实现细节（第三周重点）
- [ ] `bits/hashtable.h` 完整阅读
- [ ] `bits/hashtable_policy.h` 策略类
- [ ] 单链表+桶索引的内存布局设计
- [ ] rehash 的触发条件和实现
- [ ] `local_iterator` vs `iterator` 的区别
- [ ] 素数桶数量序列的选择

---

## 实践项目详细要求

### 项目一：mini_map<K, V>

**目标**：实现一个功能完整的红黑树map

**代码框架**：
```cpp
// mini_map.hpp
#pragma once
#include <functional>
#include <utility>
#include <stdexcept>
#include <iostream>

template <typename Key, typename Value, typename Compare = std::less<Key>>
class mini_map {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = std::size_t;
    using key_compare = Compare;

private:
    enum class Color { Red, Black };

    struct Node {
        value_type data;
        Color color;
        Node* parent;
        Node* left;
        Node* right;

        template<typename... Args>
        Node(Args&&... args)
            : data(std::forward<Args>(args)...),
              color(Color::Red),
              parent(nullptr), left(nullptr), right(nullptr) {}
    };

    Node* root_ = nullptr;
    Node* nil_;  // 哨兵节点
    size_type size_ = 0;
    Compare comp_;

    // ========== 核心私有方法 ==========

    // 旋转操作
    void left_rotate(Node* x);
    void right_rotate(Node* x);

    // 插入修复
    void insert_fixup(Node* z);

    // 删除相关
    void delete_fixup(Node* x);
    void transplant(Node* u, Node* v);
    Node* minimum(Node* x) const;
    Node* maximum(Node* x) const;

    // 辅助方法
    void destroy_tree(Node* node);
    Node* find_node(const key_type& key) const;
    Node* successor(Node* x) const;
    Node* predecessor(Node* x) const;

public:
    // ========== 迭代器 ==========
    class iterator {
        friend class mini_map;
        Node* node_;
        const mini_map* map_;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = mini_map::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator(Node* node, const mini_map* map) : node_(node), map_(map) {}

        reference operator*() const { return node_->data; }
        pointer operator->() const { return &node_->data; }

        iterator& operator++();    // 中序后继
        iterator operator++(int);
        iterator& operator--();    // 中序前驱
        iterator operator--(int);

        bool operator==(const iterator& other) const { return node_ == other.node_; }
        bool operator!=(const iterator& other) const { return !(*this == other); }
    };

    using const_iterator = iterator;  // 简化版本

    // ========== 构造/析构 ==========
    mini_map();
    ~mini_map();
    mini_map(const mini_map& other);
    mini_map& operator=(const mini_map& other);
    mini_map(mini_map&& other) noexcept;
    mini_map& operator=(mini_map&& other) noexcept;

    // ========== 元素访问 ==========
    mapped_type& operator[](const key_type& key);
    mapped_type& at(const key_type& key);
    const mapped_type& at(const key_type& key) const;

    // ========== 容量 ==========
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }

    // ========== 修改器 ==========
    std::pair<iterator, bool> insert(const value_type& value);

    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args);

    size_type erase(const key_type& key);
    iterator erase(iterator pos);
    void clear();

    // ========== 查找 ==========
    iterator find(const key_type& key);
    const_iterator find(const key_type& key) const;
    size_type count(const key_type& key) const;
    bool contains(const key_type& key) const;  // C++20

    iterator lower_bound(const key_type& key);
    iterator upper_bound(const key_type& key);
    std::pair<iterator, iterator> equal_range(const key_type& key);

    // ========== 迭代器 ==========
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    // ========== 调试与验证 ==========
    void print_tree() const;
    bool verify_rb_properties() const;  // 验证红黑树性质
    size_type black_height() const;     // 返回黑高

private:
    // 验证辅助
    bool verify_node(Node* n, int black_count, int& path_black_count) const;
    void print_node(Node* node, const std::string& prefix, bool is_left) const;
};
```

**必须通过的测试用例**：
```cpp
// test_mini_map.cpp
#include "mini_map.hpp"
#include <cassert>
#include <string>
#include <vector>
#include <random>
#include <algorithm>

void test_basic_operations() {
    mini_map<int, std::string> m;

    // 插入测试
    m[1] = "one";
    m[2] = "two";
    m[3] = "three";
    assert(m.size() == 3);
    assert(!m.empty());

    // 访问测试
    assert(m[1] == "one");
    assert(m.at(2) == "two");

    // at()越界抛异常
    bool threw = false;
    try { m.at(99); }
    catch (const std::out_of_range&) { threw = true; }
    assert(threw);

    // 查找测试
    assert(m.find(2) != m.end());
    assert(m.find(2)->second == "two");
    assert(m.find(99) == m.end());
    assert(m.count(1) == 1);
    assert(m.count(99) == 0);

    std::cout << "Basic operations: PASSED\n";
}

void test_ordering() {
    mini_map<int, int> m;
    std::vector<int> keys = {5, 3, 7, 1, 4, 6, 8, 2};

    for (int k : keys) m[k] = k * 10;

    // 有序遍历测试
    int prev = -1;
    for (const auto& [k, v] : m) {
        assert(k > prev);
        assert(v == k * 10);
        prev = k;
    }

    // lower_bound/upper_bound
    auto lb = m.lower_bound(4);
    assert(lb->first == 4);

    auto ub = m.upper_bound(4);
    assert(ub->first == 5);

    std::cout << "Ordering: PASSED\n";
}

void test_deletion() {
    mini_map<int, int> m;
    for (int i = 1; i <= 10; ++i) m[i] = i;

    // 删除
    assert(m.erase(5) == 1);
    assert(m.erase(5) == 0);  // 再次删除返回0
    assert(m.find(5) == m.end());
    assert(m.size() == 9);

    // 删除后仍有序
    int prev = 0;
    for (const auto& [k, v] : m) {
        assert(k > prev);
        prev = k;
    }

    // 清空
    m.clear();
    assert(m.empty());
    assert(m.size() == 0);

    std::cout << "Deletion: PASSED\n";
}

void test_rb_properties() {
    mini_map<int, int> m;

    // 随机插入大量数据
    std::vector<int> keys(1000);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), std::mt19937{42});

    for (int k : keys) {
        m[k] = k;
        assert(m.verify_rb_properties());  // 每次插入后验证！
    }

    // 随机删除
    std::shuffle(keys.begin(), keys.end(), std::mt19937{123});
    for (int i = 0; i < 500; ++i) {
        m.erase(keys[i]);
        assert(m.verify_rb_properties());  // 每次删除后验证！
    }

    std::cout << "RB Properties: PASSED\n";
}

void test_stress() {
    mini_map<int, int> m;
    const int N = 100000;

    // 大规模插入
    for (int i = 0; i < N; ++i) {
        m[i] = i;
    }
    assert(m.size() == N);
    assert(m.verify_rb_properties());

    // 查找
    for (int i = 0; i < N; ++i) {
        assert(m.find(i) != m.end());
    }

    std::cout << "Stress test: PASSED\n";
}

int main() {
    test_basic_operations();
    test_ordering();
    test_deletion();
    test_rb_properties();
    test_stress();

    std::cout << "\n========== ALL TESTS PASSED ==========\n";
    return 0;
}
```

### 项目二：mini_hash_map<K, V>

**目标**：实现一个功能完整的链地址哈希表

**代码框架**：
```cpp
// mini_hash_map.hpp
#pragma once
#include <vector>
#include <functional>
#include <utility>
#include <stdexcept>
#include <cmath>

template <typename Key, typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class mini_hash_map {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = std::size_t;
    using hasher = Hash;
    using key_equal = KeyEqual;

private:
    struct Node {
        value_type kv;
        Node* next;
        size_t cached_hash;

        template<typename... Args>
        Node(size_t h, Args&&... args)
            : kv(std::forward<Args>(args)...),
              next(nullptr), cached_hash(h) {}
    };

    std::vector<Node*> buckets_;
    size_type size_ = 0;
    float max_load_factor_ = 1.0f;
    Hash hash_;
    KeyEqual equal_;

    // 素数表
    static constexpr size_t primes[] = {
        53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593,
        49157, 98317, 196613, 393241, 786433, 1572869, 3145739,
        6291469, 12582917, 25165843, 50331653, 100663319, 201326611
    };
    static constexpr size_t num_primes = sizeof(primes) / sizeof(primes[0]);

    size_t next_prime(size_t n) const;
    void rehash_if_needed();

public:
    // ========== 迭代器 ==========
    class iterator {
        friend class mini_hash_map;
        mini_hash_map* map_;
        size_t bucket_idx_;
        Node* node_;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = mini_hash_map::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator(mini_hash_map* map, size_t idx, Node* node);

        reference operator*() const { return node_->kv; }
        pointer operator->() const { return &node_->kv; }

        iterator& operator++();
        iterator operator++(int);

        bool operator==(const iterator& other) const;
        bool operator!=(const iterator& other) const;
    };

    // 桶迭代器
    class local_iterator {
        Node* node_;
    public:
        local_iterator(Node* node) : node_(node) {}
        value_type& operator*() { return node_->kv; }
        local_iterator& operator++() { node_ = node_->next; return *this; }
        bool operator!=(const local_iterator& o) const { return node_ != o.node_; }
    };

    // ========== 构造/析构 ==========
    mini_hash_map();
    explicit mini_hash_map(size_t bucket_count);
    ~mini_hash_map();

    // ========== 容量 ==========
    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }

    // ========== 修改器 ==========
    std::pair<iterator, bool> insert(const value_type& value);
    mapped_type& operator[](const key_type& key);
    size_type erase(const key_type& key);
    void clear();

    // ========== 查找 ==========
    iterator find(const key_type& key);
    size_type count(const key_type& key) const;
    bool contains(const key_type& key) const;

    // ========== 迭代器 ==========
    iterator begin();
    iterator end();

    // ========== 桶接口 ==========
    size_type bucket_count() const noexcept { return buckets_.size(); }
    size_type bucket_size(size_type n) const;
    size_type bucket(const key_type& key) const;
    local_iterator begin(size_type n);
    local_iterator end(size_type n);

    // ========== 哈希策略 ==========
    float load_factor() const noexcept;
    float max_load_factor() const noexcept { return max_load_factor_; }
    void max_load_factor(float ml);
    void rehash(size_type count);
    void reserve(size_type count);

    // ========== 观察器 ==========
    hasher hash_function() const { return hash_; }
    key_equal key_eq() const { return equal_; }
};
```

**必须通过的测试用例**：
```cpp
// test_mini_hash_map.cpp
#include "mini_hash_map.hpp"
#include <cassert>
#include <string>
#include <vector>
#include <random>

void test_basic() {
    mini_hash_map<int, std::string> m;

    m[1] = "one";
    m[2] = "two";
    assert(m.size() == 2);
    assert(m[1] == "one");
    assert(m.find(2)->second == "two");
    assert(m.find(99) == m.end());

    std::cout << "Basic: PASSED\n";
}

void test_rehash() {
    mini_hash_map<int, int> m;

    // 插入足够多的元素触发rehash
    size_t initial_buckets = m.bucket_count();
    for (int i = 0; i < 1000; ++i) {
        m[i] = i;
    }

    // 验证rehash发生
    assert(m.bucket_count() > initial_buckets);

    // 验证所有元素仍然可以找到
    for (int i = 0; i < 1000; ++i) {
        assert(m.find(i) != m.end());
        assert(m[i] == i);
    }

    std::cout << "Rehash: PASSED\n";
}

void test_load_factor() {
    mini_hash_map<int, int> m;
    m.max_load_factor(0.5f);

    for (int i = 0; i < 100; ++i) {
        m[i] = i;
        assert(m.load_factor() <= m.max_load_factor() + 0.01f);
    }

    std::cout << "Load factor: PASSED\n";
}

void test_custom_hash() {
    // 自定义哈希函数
    struct BadHash {
        size_t operator()(int) const { return 42; }  // 所有元素哈希到同一桶
    };

    mini_hash_map<int, int, BadHash> m;
    for (int i = 0; i < 100; ++i) {
        m[i] = i;
    }

    // 即使哈希函数很差，仍然能正确工作
    for (int i = 0; i < 100; ++i) {
        assert(m[i] == i);
    }

    std::cout << "Custom hash: PASSED\n";
}

void test_string_keys() {
    mini_hash_map<std::string, int> m;

    m["hello"] = 1;
    m["world"] = 2;
    m["test"] = 3;

    assert(m["hello"] == 1);
    assert(m.find("world")->second == 2);
    assert(m.find("missing") == m.end());

    std::cout << "String keys: PASSED\n";
}

void test_iteration() {
    mini_hash_map<int, int> m;
    std::vector<int> keys = {1, 2, 3, 4, 5};

    for (int k : keys) m[k] = k * 10;

    // 遍历所有元素
    int count = 0;
    for (const auto& [k, v] : m) {
        assert(v == k * 10);
        ++count;
    }
    assert(count == 5);

    std::cout << "Iteration: PASSED\n";
}

int main() {
    test_basic();
    test_rehash();
    test_load_factor();
    test_custom_hash();
    test_string_keys();
    test_iteration();

    std::cout << "\n========== ALL TESTS PASSED ==========\n";
    return 0;
}
```

---

## 本月检验标准

### 知识检验（口头问答）
- [ ] 红黑树的五个性质是什么？为什么能保证O(log n)？
- [ ] 插入操作的三种情况分别是什么？各自如何修复？
- [ ] 删除操作为什么比插入复杂？
- [ ] 哈希表的负载因子是什么？对性能有什么影响？
- [ ] 链地址法和开放寻址法各有什么优缺点？
- [ ] 什么场景用map？什么场景用unordered_map？

### 实践检验
- [ ] mini_map 通过所有测试用例
- [ ] mini_map 的 `verify_rb_properties()` 始终返回 true
- [ ] mini_hash_map 通过所有测试用例
- [ ] mini_hash_map 的 rehash 正确且高效
- [ ] 完成性能对比实验并撰写分析报告

### 输出物清单
| 文件 | 描述 | 检验标准 |
|------|------|----------|
| `src/mini_map.hpp` | 红黑树实现 | 通过所有测试 |
| `src/mini_hash_map.hpp` | 哈希表实现 | 通过所有测试 |
| `src/test_mini_map.cpp` | 红黑树测试 | 全部通过 |
| `src/test_mini_hash_map.cpp` | 哈希表测试 | 全部通过 |
| `src/benchmark_containers.cpp` | 性能测试 | 可运行 |
| `notes/month03_containers.md` | 源码分析笔记 | >3000字 |
| `notes/benchmark_report.md` | 性能分析报告 | 包含图表和结论 |

---

## 时间分配建议（140小时/月）

| 周次 | 主题 | 时间 | 详细安排 |
|------|------|------|----------|
| 第1周 | 红黑树理论 | 30h | 算法导论阅读(15h) + 手绘练习(10h) + 思考题(5h) |
| 第2周 | STL源码分析 | 35h | stl_tree.h阅读(20h) + GDB调试(10h) + 笔记整理(5h) |
| 第3周 | 哈希表理论与源码 | 35h | 算法导论(10h) + hashtable.h阅读(15h) + 实验(10h) |
| 第4周 | 实践与总结 | 40h | mini_map(18h) + mini_hash_map(12h) + 性能测试(10h) |

---

## 常见问题FAQ

**Q: 红黑树删除太复杂，怎么办？**
A: 先实现插入，确保插入正确后再实现删除。删除可以分步实现：
1. 先实现BST删除（不管颜色）
2. 再添加颜色修复逻辑

**Q: mini_hash_map 的迭代器怎么实现？**
A: 需要遍历所有桶：
```cpp
iterator& operator++() {
    node_ = node_->next;
    while (!node_ && ++bucket_idx_ < map_->buckets_.size()) {
        node_ = map_->buckets_[bucket_idx_];
    }
    return *this;
}
```

**Q: 如何验证红黑树性质？**
A: 递归检查：
1. 根是黑色
2. 红节点的子节点是黑色
3. 从根到所有叶子的黑色节点数相同

---

## 下月预告

**Month 04: 智能指针与RAII模式**

将深入分析：
- `unique_ptr` 的完整实现（移动语义、删除器）
- `shared_ptr` 的引用计数机制（原子操作、控制块）
- `weak_ptr` 与循环引用的解决
- RAII模式的最佳实践

核心技能：所有权语义、引用计数的线程安全实现
