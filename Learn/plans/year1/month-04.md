# Month 04: 智能指针与RAII模式——所有权语义的工程实现

## 本月主题概述

RAII（资源获取即初始化）是C++最核心的编程范式，智能指针是RAII的最佳实践。本月将深入分析`unique_ptr`、`shared_ptr`、`weak_ptr`的源码实现，理解所有权转移、引用计数、循环引用等核心问题，并亲手实现完整的智能指针库。

---

## 理论学习内容

### 第一周：RAII原理与所有权语义

**学习目标**：深入理解RAII和C++所有权模型

**阅读材料**：
- [ ] 《Effective Modern C++》Item 18-22（智能指针相关）
- [ ] CppCon演讲："Back to Basics: RAII and the Rule of Zero"
- [ ] 博客：Herb Sutter - "GotW #89: Smart Pointers"

**核心概念**：

#### RAII的本质
```cpp
// RAII = 构造函数获取资源，析构函数释放资源
class FileHandle {
    FILE* fp;
public:
    FileHandle(const char* path) : fp(fopen(path, "r")) {
        if (!fp) throw std::runtime_error("Cannot open file");
    }
    ~FileHandle() {
        if (fp) fclose(fp);
    }
    // 禁止拷贝，允许移动
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) noexcept : fp(other.fp) {
        other.fp = nullptr;
    }
};
```

#### 所有权的三种模式
1. **独占所有权**（Unique Ownership）: `std::unique_ptr`
2. **共享所有权**（Shared Ownership）: `std::shared_ptr`
3. **观察者**（Non-owning Observer）: `std::weak_ptr`, 原始指针

**思考问题**：
- [ ] 为什么说RAII是"异常安全的基石"？
- [ ] Rule of 0/3/5分别是什么？何时使用？

### 第二周：std::unique_ptr源码分析

**学习目标**：理解独占所有权的实现

**阅读路径**（GCC libstdc++）：
- `bits/unique_ptr.h`

**源码分析要点**：

#### unique_ptr的内存布局
```cpp
template<typename _Tp, typename _Dp = default_delete<_Tp>>
class unique_ptr {
    // 使用tuple压缩存储，利用空基类优化
    __uniq_ptr_impl<_Tp, _Dp> _M_t;
};

// _M_t 内部实际是 tuple<pointer, deleter>
// 当deleter是空类（如default_delete）时，利用EBO不占额外空间
```

**关键实现分析**：

```cpp
// 1. 为什么unique_ptr只能移动不能拷贝？
unique_ptr(const unique_ptr&) = delete;
unique_ptr& operator=(const unique_ptr&) = delete;

// 2. 移动构造如何实现？
unique_ptr(unique_ptr&& __u) noexcept
    : _M_t(__u.release(), std::forward<deleter_type>(__u.get_deleter()))
{ }

// 3. release vs reset的区别
pointer release() noexcept {
    pointer __p = get();
    _M_t._M_ptr() = pointer();
    return __p;  // 返回指针，不删除
}

void reset(pointer __p = pointer()) noexcept {
    pointer __old = get();
    _M_t._M_ptr() = __p;
    if (__old) get_deleter()(__old);  // 删除旧指针
}
```

**自定义删除器**：
```cpp
// 文件句柄的unique_ptr
auto file_deleter = [](FILE* f) { if(f) fclose(f); };
std::unique_ptr<FILE, decltype(file_deleter)> fp(fopen("test.txt", "r"), file_deleter);

// Windows HANDLE
struct HandleDeleter {
    void operator()(HANDLE h) const {
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
};
std::unique_ptr<void, HandleDeleter> handle(CreateFile(...));
```

### 第三周：std::shared_ptr与控制块

**学习目标**：理解引用计数的工程实现

**阅读路径**（GCC libstdc++）：
- `bits/shared_ptr_base.h`
- `bits/shared_ptr.h`

**源码分析要点**：

#### shared_ptr的内存布局
```cpp
template<typename _Tp>
class shared_ptr : public __shared_ptr<_Tp> {
    // 继承自 __shared_ptr
};

template<typename _Tp, _Lock_policy _Lp>
class __shared_ptr {
    element_type* _M_ptr;         // 指向管理对象的指针
    __shared_count<_Lp> _M_refcount;  // 控制块指针
};
```

#### 控制块结构
```cpp
// 控制块基类
class _Sp_counted_base {
    _Atomic_word _M_use_count;   // strong引用计数
    _Atomic_word _M_weak_count;  // weak引用计数 + 1

    virtual void _M_dispose() noexcept = 0;  // 删除托管对象
    virtual void _M_destroy() noexcept = 0;  // 删除控制块本身
};

// 实际的控制块实现
template<typename _Ptr, typename _Deleter, typename _Alloc>
class _Sp_counted_deleter : public _Sp_counted_base {
    _Ptr _M_ptr;
    _Deleter _M_del;
    _Alloc _M_alloc;
};
```

**关键问题分析**：

```cpp
// 1. 为什么 weak_count 初始值是 1 而不是 0？
// 答：这个额外的1代表"所有strong引用"。当所有strong引用归零时，
// weak_count减1；当weak_count归零时，控制块才被销毁。

// 2. make_shared 如何实现单次分配？
auto sp = std::make_shared<Foo>(args...);
// 等价于分配一块内存同时包含控制块和Foo对象
// struct Combined {
//     _Sp_counted_base control_block;
//     aligned_storage<Foo> object;
// };

// 3. shared_ptr的线程安全性
// - 控制块的引用计数操作是原子的
// - 但对同一个shared_ptr对象的并发读写不是线程安全的
// - 指向的对象本身的线程安全性取决于对象自己
```

### 第四周：weak_ptr与循环引用

**学习目标**：理解weak_ptr的设计目的和实现

**核心概念**：

#### 循环引用问题
```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;  // 循环引用！
};

void create_cycle() {
    auto n1 = std::make_shared<Node>();
    auto n2 = std::make_shared<Node>();
    n1->next = n2;
    n2->prev = n1;  // 形成环
    // 函数结束时，n1和n2引用计数都是1，永远不会释放
}

// 解决方案：使用weak_ptr打破循环
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;  // weak不增加引用计数
};
```

#### weak_ptr的实现
```cpp
template<typename _Tp>
class weak_ptr : public __weak_ptr<_Tp> {
    // 和shared_ptr类似，但只持有控制块，不增加strong count
};

// 关键操作
shared_ptr<_Tp> lock() const noexcept {
    // 原子地检查use_count并增加（如果>0）
    return __shared_ptr<_Tp>(*this, std::nothrow);
}

bool expired() const noexcept {
    return _M_refcount._M_get_use_count() == 0;
}
```

**enable_shared_from_this的实现**：
```cpp
template<typename _Tp>
class enable_shared_from_this {
    mutable weak_ptr<_Tp> _M_weak_this;

public:
    shared_ptr<_Tp> shared_from_this() {
        return shared_ptr<_Tp>(this->_M_weak_this);
    }
};

// 当shared_ptr<Derived>创建时，会自动初始化_M_weak_this
// 这通过__enable_shared_from_this_base的友元机制实现
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `__shared_ptr` 构造函数的所有重载
- [ ] `make_shared` 的实现（单次分配优化）
- [ ] 原子引用计数操作（`_M_add_ref_lock`等）
- [ ] 数组支持（`shared_ptr<T[]>` C++17）
- [ ] aliasing constructor（别名构造函数）

---

## 实践项目

### 项目：实现完整的智能指针库

**目标**：从零实现 `unique_ptr`、`shared_ptr`、`weak_ptr`

#### Part 1: mini_unique_ptr
```cpp
// mini_unique_ptr.hpp
#include <utility>

template <typename T>
struct default_delete {
    constexpr default_delete() noexcept = default;

    void operator()(T* ptr) const {
        static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
        delete ptr;
    }
};

template <typename T>
struct default_delete<T[]> {
    void operator()(T* ptr) const {
        delete[] ptr;
    }
};

template <typename T, typename Deleter = default_delete<T>>
class mini_unique_ptr {
public:
    using element_type = T;
    using pointer = T*;
    using deleter_type = Deleter;

private:
    pointer ptr_ = nullptr;
    [[no_unique_address]] Deleter deleter_;  // C++20 EBO

public:
    // 构造函数
    constexpr mini_unique_ptr() noexcept = default;
    constexpr mini_unique_ptr(std::nullptr_t) noexcept : ptr_(nullptr) {}
    explicit mini_unique_ptr(pointer p) noexcept : ptr_(p) {}
    mini_unique_ptr(pointer p, const Deleter& d) : ptr_(p), deleter_(d) {}
    mini_unique_ptr(pointer p, Deleter&& d) : ptr_(p), deleter_(std::move(d)) {}

    // 移动构造/赋值
    mini_unique_ptr(mini_unique_ptr&& other) noexcept
        : ptr_(other.release()), deleter_(std::move(other.deleter_)) {}

    mini_unique_ptr& operator=(mini_unique_ptr&& other) noexcept {
        if (this != &other) {
            reset(other.release());
            deleter_ = std::move(other.deleter_);
        }
        return *this;
    }

    // 禁止拷贝
    mini_unique_ptr(const mini_unique_ptr&) = delete;
    mini_unique_ptr& operator=(const mini_unique_ptr&) = delete;

    // 析构
    ~mini_unique_ptr() {
        if (ptr_) deleter_(ptr_);
    }

    // 观察者
    pointer get() const noexcept { return ptr_; }
    Deleter& get_deleter() noexcept { return deleter_; }
    const Deleter& get_deleter() const noexcept { return deleter_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    // 解引用
    T& operator*() const { return *ptr_; }
    pointer operator->() const noexcept { return ptr_; }

    // 修改器
    pointer release() noexcept {
        pointer p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    void reset(pointer p = nullptr) noexcept {
        pointer old = ptr_;
        ptr_ = p;
        if (old) deleter_(old);
    }

    void swap(mini_unique_ptr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(deleter_, other.deleter_);
    }
};

// make_unique 实现
template <typename T, typename... Args>
mini_unique_ptr<T> make_mini_unique(Args&&... args) {
    return mini_unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

#### Part 2: mini_shared_ptr 和 mini_weak_ptr
```cpp
// mini_shared_ptr.hpp
#include <atomic>
#include <utility>

// 控制块基类
class control_block_base {
public:
    std::atomic<long> strong_count{1};
    std::atomic<long> weak_count{1};  // +1 for strong refs

    virtual void destroy_object() noexcept = 0;
    virtual void destroy_self() noexcept = 0;
    virtual ~control_block_base() = default;

    void add_strong_ref() noexcept {
        strong_count.fetch_add(1, std::memory_order_relaxed);
    }

    void release_strong_ref() noexcept {
        if (strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            destroy_object();
            release_weak_ref();  // release the +1
        }
    }

    void add_weak_ref() noexcept {
        weak_count.fetch_add(1, std::memory_order_relaxed);
    }

    void release_weak_ref() noexcept {
        if (weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            destroy_self();
        }
    }

    long use_count() const noexcept {
        return strong_count.load(std::memory_order_relaxed);
    }

    bool try_add_strong_ref() noexcept {
        long count = strong_count.load(std::memory_order_relaxed);
        while (count != 0) {
            if (strong_count.compare_exchange_weak(count, count + 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }
};

// 标准控制块（分离分配）
template <typename T, typename Deleter = std::default_delete<T>>
class control_block_ptr : public control_block_base {
    T* ptr_;
    Deleter deleter_;
public:
    control_block_ptr(T* p, Deleter d = Deleter())
        : ptr_(p), deleter_(std::move(d)) {}

    void destroy_object() noexcept override {
        deleter_(ptr_);
    }

    void destroy_self() noexcept override {
        delete this;
    }
};

// make_shared控制块（单次分配）
template <typename T>
class control_block_inplace : public control_block_base {
    alignas(T) unsigned char storage_[sizeof(T)];
public:
    template <typename... Args>
    control_block_inplace(Args&&... args) {
        new (storage_) T(std::forward<Args>(args)...);
    }

    T* get_ptr() noexcept {
        return reinterpret_cast<T*>(storage_);
    }

    void destroy_object() noexcept override {
        get_ptr()->~T();
    }

    void destroy_self() noexcept override {
        delete this;
    }
};

// 前向声明
template <typename T> class mini_weak_ptr;

template <typename T>
class mini_shared_ptr {
    template <typename U> friend class mini_shared_ptr;
    template <typename U> friend class mini_weak_ptr;

    T* ptr_ = nullptr;
    control_block_base* ctrl_ = nullptr;

public:
    // 构造函数
    constexpr mini_shared_ptr() noexcept = default;
    constexpr mini_shared_ptr(std::nullptr_t) noexcept {}

    template <typename U>
    explicit mini_shared_ptr(U* p) : ptr_(p) {
        try {
            ctrl_ = new control_block_ptr<U>(p);
        } catch (...) {
            delete p;
            throw;
        }
    }

    // 拷贝
    mini_shared_ptr(const mini_shared_ptr& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) ctrl_->add_strong_ref();
    }

    // 移动
    mini_shared_ptr(mini_shared_ptr&& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_ = nullptr;
        other.ctrl_ = nullptr;
    }

    // 从weak_ptr构造
    explicit mini_shared_ptr(const mini_weak_ptr<T>& weak);

    ~mini_shared_ptr() {
        if (ctrl_) ctrl_->release_strong_ref();
    }

    // 赋值
    mini_shared_ptr& operator=(const mini_shared_ptr& other) noexcept {
        mini_shared_ptr(other).swap(*this);
        return *this;
    }

    mini_shared_ptr& operator=(mini_shared_ptr&& other) noexcept {
        mini_shared_ptr(std::move(other)).swap(*this);
        return *this;
    }

    // 观察者
    T* get() const noexcept { return ptr_; }
    long use_count() const noexcept { return ctrl_ ? ctrl_->use_count() : 0; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T& operator*() const { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }

    void reset() noexcept { mini_shared_ptr().swap(*this); }

    void swap(mini_shared_ptr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(ctrl_, other.ctrl_);
    }

private:
    // 用于make_shared
    mini_shared_ptr(T* p, control_block_base* c) : ptr_(p), ctrl_(c) {}

    template <typename U, typename... Args>
    friend mini_shared_ptr<U> make_mini_shared(Args&&...);
};

template <typename T>
class mini_weak_ptr {
    template <typename U> friend class mini_shared_ptr;
    template <typename U> friend class mini_weak_ptr;

    T* ptr_ = nullptr;
    control_block_base* ctrl_ = nullptr;

public:
    constexpr mini_weak_ptr() noexcept = default;

    mini_weak_ptr(const mini_shared_ptr<T>& sp) noexcept
        : ptr_(sp.ptr_), ctrl_(sp.ctrl_) {
        if (ctrl_) ctrl_->add_weak_ref();
    }

    mini_weak_ptr(const mini_weak_ptr& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) ctrl_->add_weak_ref();
    }

    mini_weak_ptr(mini_weak_ptr&& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_ = nullptr;
        other.ctrl_ = nullptr;
    }

    ~mini_weak_ptr() {
        if (ctrl_) ctrl_->release_weak_ref();
    }

    mini_weak_ptr& operator=(const mini_weak_ptr& other) noexcept {
        mini_weak_ptr(other).swap(*this);
        return *this;
    }

    mini_weak_ptr& operator=(mini_weak_ptr&& other) noexcept {
        mini_weak_ptr(std::move(other)).swap(*this);
        return *this;
    }

    mini_weak_ptr& operator=(const mini_shared_ptr<T>& sp) noexcept {
        mini_weak_ptr(sp).swap(*this);
        return *this;
    }

    long use_count() const noexcept { return ctrl_ ? ctrl_->use_count() : 0; }
    bool expired() const noexcept { return use_count() == 0; }

    mini_shared_ptr<T> lock() const noexcept {
        if (ctrl_ && ctrl_->try_add_strong_ref()) {
            mini_shared_ptr<T> sp;
            sp.ptr_ = ptr_;
            sp.ctrl_ = ctrl_;
            return sp;
        }
        return mini_shared_ptr<T>();
    }

    void reset() noexcept { mini_weak_ptr().swap(*this); }

    void swap(mini_weak_ptr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(ctrl_, other.ctrl_);
    }
};

// mini_shared_ptr从weak_ptr构造的实现
template <typename T>
mini_shared_ptr<T>::mini_shared_ptr(const mini_weak_ptr<T>& weak) {
    if (weak.ctrl_ && weak.ctrl_->try_add_strong_ref()) {
        ptr_ = weak.ptr_;
        ctrl_ = weak.ctrl_;
    } else {
        throw std::bad_weak_ptr();
    }
}

// make_shared实现
template <typename T, typename... Args>
mini_shared_ptr<T> make_mini_shared(Args&&... args) {
    auto* ctrl = new control_block_inplace<T>(std::forward<Args>(args)...);
    return mini_shared_ptr<T>(ctrl->get_ptr(), ctrl);
}
```

---

## 检验标准

### 知识检验
- [ ] 解释RAII如何保证异常安全
- [ ] unique_ptr如何实现零开销抽象？
- [ ] shared_ptr的控制块包含什么？为什么weak_count初始值是1？
- [ ] make_shared比直接构造shared_ptr有什么优势？有什么劣势？
- [ ] 如何用weak_ptr解决循环引用？

### 实践检验
- [ ] mini_unique_ptr支持自定义删除器
- [ ] mini_shared_ptr正确处理引用计数
- [ ] mini_weak_ptr的lock()在对象销毁后返回空
- [ ] make_mini_shared实现单次分配

### 输出物
1. `mini_unique_ptr.hpp`
2. `mini_shared_ptr.hpp`（含weak_ptr）
3. `test_smart_pointers.cpp`
4. `notes/month04_smart_pointers.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 30小时 | 21% |
| 源码阅读 | 35小时 | 25% |
| unique_ptr实现 | 20小时 | 14% |
| shared_ptr/weak_ptr实现 | 40小时 | 29% |
| 测试与文档 | 15小时 | 11% |

---

## 下月预告

Month 05将进入**模板元编程基础**，学习SFINAE、type_traits、constexpr等编译期计算技术，为理解现代C++库的实现打下基础。
