# Month 09: 迭代器与算法库深度——STL的设计哲学

## 本月主题概述

迭代器是STL的灵魂，它将容器和算法解耦，实现了高度的通用性。本月将深入理解迭代器的分类体系、STL算法的实现，并掌握自定义迭代器和算法的技巧。

---

## 理论学习内容

### 第一周：迭代器概念与分类

**学习目标**：理解迭代器层次结构

**阅读材料**：
- [ ] 《STL源码剖析》第3章
- [ ] C++标准 [iterator] 章节
- [ ] CppCon演讲："The Iterator Concept"

**核心概念**：

#### 迭代器类别层次
```cpp
// 迭代器层次（从弱到强）
// Input Iterator        -> 单遍、只读
// Output Iterator       -> 单遍、只写
// Forward Iterator      -> 多遍、读写
// Bidirectional Iterator -> 可双向移动
// Random Access Iterator -> 支持随机访问
// Contiguous Iterator   -> C++20，连续存储

// 各类别支持的操作
// Input:       ++, *, ==, !=, -> (只能前进，只能读)
// Output:      ++, *（只能前进，只能写）
// Forward:     Input + 多遍保证
// Bidirectional: Forward + --
// RandomAccess: Bidirectional + [], +=, -=, +, -, <, >, <=, >=
// Contiguous:  RandomAccess + 元素连续存储（可以用指针算术）
```

#### 迭代器特征（iterator_traits）
```cpp
// iterator_traits提取迭代器信息
template <typename Iterator>
struct iterator_traits {
    using difference_type   = typename Iterator::difference_type;
    using value_type        = typename Iterator::value_type;
    using pointer           = typename Iterator::pointer;
    using reference         = typename Iterator::reference;
    using iterator_category = typename Iterator::iterator_category;
};

// 指针特化
template <typename T>
struct iterator_traits<T*> {
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = T*;
    using reference         = T&;
    using iterator_category = std::random_access_iterator_tag;
};

// 使用
template <typename Iterator>
void advance_impl(Iterator& it, int n,
                  std::random_access_iterator_tag) {
    it += n;  // O(1)
}

template <typename Iterator>
void advance_impl(Iterator& it, int n,
                  std::input_iterator_tag) {
    while (n--) ++it;  // O(n)
}

template <typename Iterator>
void advance(Iterator& it, int n) {
    advance_impl(it, n,
        typename std::iterator_traits<Iterator>::iterator_category{});
}
```

### 第二周：常用算法实现分析

**学习目标**：理解STL算法的实现技巧

#### std::find的实现
```cpp
template <typename InputIt, typename T>
InputIt find(InputIt first, InputIt last, const T& value) {
    for (; first != last; ++first) {
        if (*first == value) {
            return first;
        }
    }
    return last;
}

// 带谓词版本
template <typename InputIt, typename Pred>
InputIt find_if(InputIt first, InputIt last, Pred pred) {
    for (; first != last; ++first) {
        if (pred(*first)) {
            return first;
        }
    }
    return last;
}
```

#### std::sort的实现（简化版）
```cpp
// 实际的std::sort是IntroSort：
// - 小区间用InsertionSort
// - 中等用QuickSort
// - 递归过深切换HeapSort（防止最坏O(n²)）

template <typename RandomIt>
void insertion_sort(RandomIt first, RandomIt last) {
    for (auto it = first + 1; it != last; ++it) {
        auto key = std::move(*it);
        auto j = it;
        while (j != first && *(j - 1) > key) {
            *j = std::move(*(j - 1));
            --j;
        }
        *j = std::move(key);
    }
}

template <typename RandomIt>
RandomIt partition(RandomIt first, RandomIt last) {
    auto pivot = *(first + (last - first) / 2);
    auto left = first;
    auto right = last - 1;

    while (true) {
        while (*left < pivot) ++left;
        while (*right > pivot) --right;
        if (left >= right) return right;
        std::iter_swap(left++, right--);
    }
}

template <typename RandomIt>
void quicksort(RandomIt first, RandomIt last, int depth_limit) {
    while (last - first > 16) {  // 小区间用插入排序
        if (depth_limit == 0) {
            // 递归过深，切换堆排序
            std::make_heap(first, last);
            std::sort_heap(first, last);
            return;
        }
        --depth_limit;

        auto pivot = partition(first, last);
        quicksort(pivot + 1, last, depth_limit);
        last = pivot + 1;  // 尾递归优化
    }
}

template <typename RandomIt>
void introsort(RandomIt first, RandomIt last) {
    if (first == last) return;

    int depth_limit = 2 * static_cast<int>(std::log2(last - first));
    quicksort(first, last, depth_limit);
    insertion_sort(first, last);  // 清理小区间
}
```

### 第三周：迭代器适配器

**学习目标**：理解和使用迭代器适配器

#### 常用适配器
```cpp
// reverse_iterator
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.rbegin(); it != v.rend(); ++it) {
    std::cout << *it << " ";  // 5 4 3 2 1
}

// back_inserter / front_inserter / inserter
std::vector<int> src = {1, 2, 3};
std::vector<int> dst;
std::copy(src.begin(), src.end(), std::back_inserter(dst));

// move_iterator
std::vector<std::string> src2 = {"a", "b", "c"};
std::vector<std::string> dst2;
std::copy(std::make_move_iterator(src2.begin()),
          std::make_move_iterator(src2.end()),
          std::back_inserter(dst2));
// src2中的字符串已被移动

// istream_iterator / ostream_iterator
std::copy(std::istream_iterator<int>(std::cin),
          std::istream_iterator<int>(),
          std::back_inserter(v));

std::copy(v.begin(), v.end(),
          std::ostream_iterator<int>(std::cout, " "));
```

#### reverse_iterator实现
```cpp
template <typename Iterator>
class reverse_iterator {
    Iterator current_;

public:
    using iterator_type = Iterator;
    using difference_type = typename std::iterator_traits<Iterator>::difference_type;
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    using pointer = typename std::iterator_traits<Iterator>::pointer;
    using reference = typename std::iterator_traits<Iterator>::reference;
    using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;

    reverse_iterator() = default;
    explicit reverse_iterator(Iterator it) : current_(it) {}

    Iterator base() const { return current_; }

    reference operator*() const {
        Iterator tmp = current_;
        return *--tmp;  // 指向前一个元素
    }

    reverse_iterator& operator++() {
        --current_;
        return *this;
    }

    reverse_iterator& operator--() {
        ++current_;
        return *this;
    }

    reverse_iterator operator+(difference_type n) const {
        return reverse_iterator(current_ - n);
    }

    // ...其他操作
};
```

### 第四周：C++20 Ranges

**学习目标**：理解Ranges库的设计

#### Ranges基础
```cpp
#include <ranges>

std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// 传统方式
std::vector<int> result;
for (int x : v) {
    if (x % 2 == 0) {
        result.push_back(x * x);
    }
}

// Ranges方式
auto result = v
    | std::views::filter([](int x) { return x % 2 == 0; })
    | std::views::transform([](int x) { return x * x; });

// result是一个惰性视图，遍历时才计算
for (int x : result) {
    std::cout << x << " ";  // 4 16 36 64 100
}

// 转换为vector
auto vec = result | std::ranges::to<std::vector>();
```

#### Views是惰性的
```cpp
// Views不存储数据，只是对原数据的视图
// 每次遍历都会重新计算

auto view = v | std::views::filter([](int x) {
    std::cout << "filtering " << x << "\n";
    return x % 2 == 0;
});

// 此时没有任何输出

for (int x : view) {  // 遍历时才执行filter
    std::cout << "got " << x << "\n";
}
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `std::iterator_traits`实现
- [ ] `std::advance`和`std::distance`实现
- [ ] `std::reverse_iterator`实现
- [ ] `std::back_insert_iterator`实现
- [ ] `std::sort`完整实现（IntroSort）

---

## 实践项目

### 项目：实现自定义迭代器和算法

#### Part 1: 链表迭代器
```cpp
// list_iterator.hpp
#pragma once
#include <iterator>

template <typename T>
struct ListNode {
    T data;
    ListNode* next = nullptr;
    ListNode* prev = nullptr;

    ListNode(const T& d) : data(d) {}
};

template <typename T>
class ListIterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

private:
    ListNode<T>* node_;

public:
    ListIterator() : node_(nullptr) {}
    explicit ListIterator(ListNode<T>* n) : node_(n) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &node_->data; }

    ListIterator& operator++() {
        node_ = node_->next;
        return *this;
    }

    ListIterator operator++(int) {
        ListIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    ListIterator& operator--() {
        node_ = node_->prev;
        return *this;
    }

    ListIterator operator--(int) {
        ListIterator tmp = *this;
        --(*this);
        return tmp;
    }

    bool operator==(const ListIterator& other) const {
        return node_ == other.node_;
    }

    bool operator!=(const ListIterator& other) const {
        return node_ != other.node_;
    }

    ListNode<T>* node() const { return node_; }
};

// const版本
template <typename T>
class ListConstIterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

private:
    const ListNode<T>* node_;

public:
    ListConstIterator() : node_(nullptr) {}
    explicit ListConstIterator(const ListNode<T>* n) : node_(n) {}
    ListConstIterator(const ListIterator<T>& it) : node_(it.node()) {}

    reference operator*() const { return node_->data; }
    pointer operator->() const { return &node_->data; }

    ListConstIterator& operator++() {
        node_ = node_->next;
        return *this;
    }

    ListConstIterator& operator--() {
        node_ = node_->prev;
        return *this;
    }

    bool operator==(const ListConstIterator& other) const {
        return node_ == other.node_;
    }

    bool operator!=(const ListConstIterator& other) const {
        return node_ != other.node_;
    }
};
```

#### Part 2: 实现mini_algorithms
```cpp
// mini_algorithms.hpp
#pragma once
#include <iterator>
#include <utility>
#include <functional>

namespace mini {

// ==================== 非修改序列操作 ====================

template <typename InputIt, typename T>
InputIt find(InputIt first, InputIt last, const T& value) {
    for (; first != last; ++first) {
        if (*first == value) return first;
    }
    return last;
}

template <typename InputIt, typename Pred>
InputIt find_if(InputIt first, InputIt last, Pred pred) {
    for (; first != last; ++first) {
        if (pred(*first)) return first;
    }
    return last;
}

template <typename InputIt, typename Pred>
bool all_of(InputIt first, InputIt last, Pred pred) {
    for (; first != last; ++first) {
        if (!pred(*first)) return false;
    }
    return true;
}

template <typename InputIt, typename Pred>
bool any_of(InputIt first, InputIt last, Pred pred) {
    for (; first != last; ++first) {
        if (pred(*first)) return true;
    }
    return false;
}

template <typename InputIt, typename Pred>
bool none_of(InputIt first, InputIt last, Pred pred) {
    return !any_of(first, last, pred);
}

template <typename InputIt, typename T>
typename std::iterator_traits<InputIt>::difference_type
count(InputIt first, InputIt last, const T& value) {
    typename std::iterator_traits<InputIt>::difference_type n = 0;
    for (; first != last; ++first) {
        if (*first == value) ++n;
    }
    return n;
}

// ==================== 修改序列操作 ====================

template <typename InputIt, typename OutputIt>
OutputIt copy(InputIt first, InputIt last, OutputIt result) {
    for (; first != last; ++first, ++result) {
        *result = *first;
    }
    return result;
}

template <typename InputIt, typename OutputIt, typename Pred>
OutputIt copy_if(InputIt first, InputIt last, OutputIt result, Pred pred) {
    for (; first != last; ++first) {
        if (pred(*first)) {
            *result = *first;
            ++result;
        }
    }
    return result;
}

template <typename InputIt, typename OutputIt>
OutputIt move(InputIt first, InputIt last, OutputIt result) {
    for (; first != last; ++first, ++result) {
        *result = std::move(*first);
    }
    return result;
}

template <typename InputIt, typename OutputIt, typename UnaryOp>
OutputIt transform(InputIt first, InputIt last, OutputIt result, UnaryOp op) {
    for (; first != last; ++first, ++result) {
        *result = op(*first);
    }
    return result;
}

template <typename ForwardIt, typename T>
void fill(ForwardIt first, ForwardIt last, const T& value) {
    for (; first != last; ++first) {
        *first = value;
    }
}

template <typename ForwardIt, typename Generator>
void generate(ForwardIt first, ForwardIt last, Generator gen) {
    for (; first != last; ++first) {
        *first = gen();
    }
}

template <typename ForwardIt, typename T>
ForwardIt remove(ForwardIt first, ForwardIt last, const T& value) {
    first = mini::find(first, last, value);
    if (first == last) return first;

    ForwardIt result = first;
    ++first;
    for (; first != last; ++first) {
        if (*first != value) {
            *result = std::move(*first);
            ++result;
        }
    }
    return result;
}

template <typename BidirIt>
void reverse(BidirIt first, BidirIt last) {
    while (first != last && first != --last) {
        std::iter_swap(first++, last);
    }
}

template <typename ForwardIt>
ForwardIt unique(ForwardIt first, ForwardIt last) {
    if (first == last) return last;

    ForwardIt result = first;
    while (++first != last) {
        if (!(*result == *first)) {
            ++result;
            if (result != first) {
                *result = std::move(*first);
            }
        }
    }
    return ++result;
}

// ==================== 排序操作 ====================

template <typename RandomIt>
void insertion_sort(RandomIt first, RandomIt last) {
    if (first == last) return;

    for (auto it = first + 1; it != last; ++it) {
        auto key = std::move(*it);
        auto j = it;
        while (j != first && *(j - 1) > key) {
            *j = std::move(*(j - 1));
            --j;
        }
        *j = std::move(key);
    }
}

// 简化版quicksort
template <typename RandomIt, typename Compare = std::less<>>
void sort(RandomIt first, RandomIt last, Compare comp = Compare{}) {
    auto size = last - first;
    if (size <= 1) return;

    if (size <= 16) {
        insertion_sort(first, last);
        return;
    }

    // 三数取中选pivot
    auto mid = first + size / 2;
    if (comp(*mid, *first)) std::iter_swap(first, mid);
    if (comp(*(last - 1), *first)) std::iter_swap(first, last - 1);
    if (comp(*(last - 1), *mid)) std::iter_swap(mid, last - 1);

    auto pivot = *mid;

    auto left = first;
    auto right = last - 1;
    while (true) {
        while (comp(*left, pivot)) ++left;
        while (comp(pivot, *right)) --right;
        if (left >= right) break;
        std::iter_swap(left++, right--);
    }

    sort(first, left, comp);
    sort(right + 1, last, comp);
}

// ==================== 二分搜索 ====================

template <typename ForwardIt, typename T>
ForwardIt lower_bound(ForwardIt first, ForwardIt last, const T& value) {
    auto count = std::distance(first, last);

    while (count > 0) {
        auto step = count / 2;
        auto mid = first;
        std::advance(mid, step);

        if (*mid < value) {
            first = ++mid;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template <typename ForwardIt, typename T>
ForwardIt upper_bound(ForwardIt first, ForwardIt last, const T& value) {
    auto count = std::distance(first, last);

    while (count > 0) {
        auto step = count / 2;
        auto mid = first;
        std::advance(mid, step);

        if (!(value < *mid)) {
            first = ++mid;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template <typename ForwardIt, typename T>
bool binary_search(ForwardIt first, ForwardIt last, const T& value) {
    auto it = lower_bound(first, last, value);
    return it != last && !(value < *it);
}

// ==================== 堆操作 ====================

template <typename RandomIt>
void push_heap(RandomIt first, RandomIt last) {
    auto hole = last - first - 1;
    auto value = std::move(*(first + hole));

    while (hole > 0) {
        auto parent = (hole - 1) / 2;
        if (*(first + parent) < value) {
            *(first + hole) = std::move(*(first + parent));
            hole = parent;
        } else {
            break;
        }
    }
    *(first + hole) = std::move(value);
}

template <typename RandomIt>
void pop_heap(RandomIt first, RandomIt last) {
    auto value = std::move(*(last - 1));
    *(last - 1) = std::move(*first);

    auto len = last - first - 1;
    auto hole = 0;
    auto child = 2 * hole + 1;

    while (child < len) {
        if (child + 1 < len && *(first + child) < *(first + child + 1)) {
            ++child;
        }
        if (value < *(first + child)) {
            *(first + hole) = std::move(*(first + child));
            hole = child;
            child = 2 * hole + 1;
        } else {
            break;
        }
    }
    *(first + hole) = std::move(value);
}

template <typename RandomIt>
void make_heap(RandomIt first, RandomIt last) {
    auto len = last - first;
    if (len <= 1) return;

    for (auto i = len / 2; i > 0; --i) {
        // 从后向前调整每个非叶节点
        auto hole = i - 1;
        auto value = std::move(*(first + hole));
        auto child = 2 * hole + 1;

        while (child < len) {
            if (child + 1 < len && *(first + child) < *(first + child + 1)) {
                ++child;
            }
            if (value < *(first + child)) {
                *(first + hole) = std::move(*(first + child));
                hole = child;
                child = 2 * hole + 1;
            } else {
                break;
            }
        }
        *(first + hole) = std::move(value);
    }
}

template <typename RandomIt>
void sort_heap(RandomIt first, RandomIt last) {
    while (last - first > 1) {
        pop_heap(first, last--);
    }
}

// ==================== 数值操作 ====================

template <typename InputIt, typename T>
T accumulate(InputIt first, InputIt last, T init) {
    for (; first != last; ++first) {
        init = std::move(init) + *first;
    }
    return init;
}

template <typename InputIt, typename T, typename BinaryOp>
T accumulate(InputIt first, InputIt last, T init, BinaryOp op) {
    for (; first != last; ++first) {
        init = op(std::move(init), *first);
    }
    return init;
}

} // namespace mini
```

#### Part 3: 简单的Range View
```cpp
// mini_ranges.hpp
#pragma once
#include <iterator>

namespace mini::ranges {

// filter_view
template <typename Range, typename Pred>
class filter_view {
    Range& range_;
    Pred pred_;

public:
    class iterator {
        using base_iter = decltype(std::begin(std::declval<Range&>()));
        base_iter current_;
        base_iter end_;
        Pred* pred_;

        void find_next() {
            while (current_ != end_ && !(*pred_)(*current_)) {
                ++current_;
            }
        }

    public:
        using value_type = typename std::iterator_traits<base_iter>::value_type;
        using reference = typename std::iterator_traits<base_iter>::reference;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;

        iterator(base_iter cur, base_iter end, Pred* p)
            : current_(cur), end_(end), pred_(p) {
            find_next();
        }

        reference operator*() const { return *current_; }

        iterator& operator++() {
            ++current_;
            find_next();
            return *this;
        }

        bool operator==(const iterator& other) const {
            return current_ == other.current_;
        }
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

    filter_view(Range& r, Pred p) : range_(r), pred_(std::move(p)) {}

    iterator begin() {
        return iterator(std::begin(range_), std::end(range_), &pred_);
    }

    iterator end() {
        return iterator(std::end(range_), std::end(range_), &pred_);
    }
};

// transform_view
template <typename Range, typename Func>
class transform_view {
    Range& range_;
    Func func_;

public:
    class iterator {
        using base_iter = decltype(std::begin(std::declval<Range&>()));
        base_iter current_;
        Func* func_;

    public:
        using value_type = std::invoke_result_t<Func&,
            typename std::iterator_traits<base_iter>::reference>;
        using reference = value_type;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;

        iterator(base_iter cur, Func* f) : current_(cur), func_(f) {}

        value_type operator*() const { return (*func_)(*current_); }

        iterator& operator++() { ++current_; return *this; }

        bool operator==(const iterator& other) const {
            return current_ == other.current_;
        }
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

    transform_view(Range& r, Func f) : range_(r), func_(std::move(f)) {}

    iterator begin() { return iterator(std::begin(range_), &func_); }
    iterator end() { return iterator(std::end(range_), &func_); }
};

// 管道操作符支持
template <typename Pred>
struct filter_fn {
    Pred pred;
    template <typename Range>
    auto operator()(Range& r) const {
        return filter_view<Range, Pred>(r, pred);
    }
};

template <typename Func>
struct transform_fn {
    Func func;
    template <typename Range>
    auto operator()(Range& r) const {
        return transform_view<Range, Func>(r, func);
    }
};

template <typename Pred>
auto filter(Pred p) { return filter_fn<Pred>{std::move(p)}; }

template <typename Func>
auto transform(Func f) { return transform_fn<Func>{std::move(f)}; }

// 管道操作符
template <typename Range, typename Fn>
auto operator|(Range& r, Fn fn) {
    return fn(r);
}

} // namespace mini::ranges
```

---

## 检验标准

### 知识检验
- [ ] 解释五种迭代器类别的区别
- [ ] iterator_traits的作用是什么？
- [ ] std::sort使用什么算法？为什么？
- [ ] reverse_iterator的*操作为什么要减1？
- [ ] Ranges的Views为什么是惰性的？

### 实践检验
- [ ] 自定义迭代器能与STL算法配合使用
- [ ] mini_algorithms通过测试
- [ ] mini_ranges能实现基本的管道操作

### 输出物
1. `list_iterator.hpp`
2. `mini_algorithms.hpp`
3. `mini_ranges.hpp`
4. `test_iterators.cpp`
5. `notes/month09_iterators.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 30小时 | 21% |
| 源码阅读 | 30小时 | 21% |
| 迭代器实现 | 30小时 | 22% |
| 算法实现 | 35小时 | 25% |
| Ranges与测试 | 15小时 | 11% |

---

## 下月预告

Month 10将学习**字符串处理与正则表达式**，深入string/string_view的实现，SSO优化，以及std::regex的使用和性能考量。
