# Month 06: 完美转发与移动语义深度——右值引用的本质

## 本月主题概述

移动语义是C++11最重要的特性之一，它从根本上改变了C++的资源管理方式。本月将深入理解右值引用、移动语义、完美转发的底层原理，分析`std::move`、`std::forward`的实现，并掌握编写移动友好代码的技巧。

---

## 理论学习内容

### 第一周：值类别与右值引用

**学习目标**：彻底理解C++的值类别体系

**阅读材料**：
- [ ] 《Effective Modern C++》Item 23-30
- [ ] CppCon演讲："Understanding Value Categories" by Ben Deane
- [ ] C++标准 [basic.lval] 章节

**核心概念**：

#### 值类别（Value Categories）
```cpp
// C++11之前：左值(lvalue) vs 右值(rvalue)
// C++11之后：更精细的分类

//        expression
//        /        \
//     glvalue    rvalue
//     /    \    /     \
//  lvalue  xvalue    prvalue

// lvalue: 有身份，不可移动（有名字的对象）
int x = 42;        // x是lvalue
int& ref = x;      // ref是lvalue

// prvalue: 无身份，可移动（纯右值，临时对象）
int f();
f();               // f()的返回值是prvalue
42;                // 字面量是prvalue

// xvalue: 有身份，可移动（将亡值）
std::move(x);      // std::move(x)是xvalue
```

#### 右值引用
```cpp
// 右值引用只能绑定到右值
int&& rref = 42;           // OK: 绑定到prvalue
int&& rref2 = std::move(x); // OK: 绑定到xvalue
int&& rref3 = x;           // ERROR: 不能绑定到lvalue

// 但是！右值引用变量本身是lvalue
void process(int&& v) {
    // v是lvalue！因为它有名字
    // 如果要传递给另一个函数，需要std::move
    other_func(std::move(v));
}
```

### 第二周：移动语义的实现

**学习目标**：理解移动构造/赋值的实现原理

#### std::move的本质
```cpp
// std::move的实现（简化版）
template <typename T>
constexpr std::remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(t);
}

// std::move只是一个类型转换，不移动任何东西！
// 它将任意引用类型转换为右值引用
// 真正的移动发生在移动构造/赋值函数中
```

#### 移动构造与移动赋值
```cpp
class String {
    char* data_;
    size_t size_;

public:
    // 移动构造：窃取资源
    String(String&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
        // 源对象进入有效但未指定的状态
    }

    // 移动赋值：释放自己的资源，窃取对方的
    String& operator=(String&& other) noexcept {
        if (this != &other) {
            delete[] data_;           // 释放自己的
            data_ = other.data_;      // 窃取对方的
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    // 析构函数需要能处理moved-from状态
    ~String() {
        delete[] data_;  // delete nullptr是安全的
    }
};
```

#### noexcept的重要性
```cpp
// 为什么移动操作应该标记noexcept？
// 因为STL容器在扩容时会检查

// std::vector::push_back的简化逻辑
void push_back(T&& value) {
    if (size_ == capacity_) {
        // 需要扩容
        T* new_data = allocate(capacity_ * 2);
        if constexpr (std::is_nothrow_move_constructible_v<T>) {
            // 移动构造是noexcept，可以安全移动
            for (size_t i = 0; i < size_; ++i)
                new (new_data + i) T(std::move(data_[i]));
        } else {
            // 移动构造可能抛异常，退回拷贝以保证强异常安全
            for (size_t i = 0; i < size_; ++i)
                new (new_data + i) T(data_[i]);
        }
    }
    // ...
}
```

### 第三周：完美转发

**学习目标**：理解完美转发的原理和std::forward的实现

#### 转发引用（万能引用）
```cpp
// 转发引用的识别
template <typename T>
void f(T&& param);  // 转发引用

auto&& x = expr;    // 转发引用

// 注意：以下不是转发引用
void g(int&& param);           // 普通右值引用
template <typename T>
void h(std::vector<T>&& v);    // 普通右值引用
template <typename T>
void k(const T&& param);       // 普通右值引用（有const）
```

#### 引用折叠规则
```cpp
// 编译器内部的引用折叠
// T& &   -> T&
// T& &&  -> T&
// T&& &  -> T&
// T&& && -> T&&

// 当传入左值int时：
template <typename T>
void f(T&& param);  // T = int&, param = int& && = int&

// 当传入右值int时：
// T = int, param = int&&
```

#### std::forward的实现
```cpp
// std::forward的作用：保持参数的值类别

template <typename T>
constexpr T&& forward(std::remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}

template <typename T>
constexpr T&& forward(std::remove_reference_t<T>&& t) noexcept {
    static_assert(!std::is_lvalue_reference_v<T>,
                  "Cannot forward rvalue as lvalue");
    return static_cast<T&&>(t);
}

// 使用示例
template <typename T>
void wrapper(T&& arg) {
    // 不使用forward：arg总是作为左值传递
    process(arg);

    // 使用forward：保持原始值类别
    process(std::forward<T>(arg));
}
```

### 第四周：高级移动语义模式

**学习目标**：掌握实际工程中的移动语义应用

#### Copy-and-Swap惯用法
```cpp
class Resource {
    int* data_;
    size_t size_;

public:
    Resource(size_t n) : data_(new int[n]), size_(n) {}

    ~Resource() { delete[] data_; }

    // 拷贝构造
    Resource(const Resource& other)
        : data_(new int[other.size_]), size_(other.size_) {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    // 移动构造
    Resource(Resource&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    // 统一赋值运算符（Copy-and-Swap）
    Resource& operator=(Resource other) noexcept {
        swap(*this, other);
        return *this;
    }

    friend void swap(Resource& a, Resource& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }
};
```

#### 返回值优化（RVO/NRVO）
```cpp
// 不要对返回的局部对象使用std::move！

std::string good() {
    std::string s = "hello";
    return s;  // NRVO会省略拷贝，或自动移动
}

std::string bad() {
    std::string s = "hello";
    return std::move(s);  // 阻止NRVO！
}

// 何时返回值使用std::move
std::string maybe_ok(std::string&& param) {
    return std::move(param);  // OK: param不是局部变量
}
```

#### 移动迭代器
```cpp
std::vector<std::string> source = {"a", "b", "c"};
std::vector<std::string> dest;

// 移动元素而不是拷贝
dest.insert(dest.end(),
    std::make_move_iterator(source.begin()),
    std::make_move_iterator(source.end()));
// source中的元素现在处于moved-from状态
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `std::move`实现（bits/move.h）
- [ ] `std::forward`实现
- [ ] `std::move_if_noexcept`实现
- [ ] `std::exchange`实现
- [ ] `std::vector`的移动构造和push_back中的移动逻辑

---

## 实践项目

### 项目：实现移动友好的容器和工具

#### Part 1: 实现std::move和std::forward
```cpp
// mini_utility.hpp
#pragma once
#include <type_traits>

namespace mini {

// remove_reference
template <typename T> struct remove_reference { using type = T; };
template <typename T> struct remove_reference<T&> { using type = T; };
template <typename T> struct remove_reference<T&&> { using type = T; };
template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

// move
template <typename T>
constexpr remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<remove_reference_t<T>&&>(t);
}

// forward
template <typename T>
constexpr T&& forward(remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}

template <typename T>
constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
    static_assert(!std::is_lvalue_reference_v<T>,
                  "Cannot forward an rvalue as an lvalue.");
    return static_cast<T&&>(t);
}

// exchange
template <typename T, typename U = T>
constexpr T exchange(T& obj, U&& new_value)
    noexcept(std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_assignable_v<T&, U>) {
    T old_value = mini::move(obj);
    obj = mini::forward<U>(new_value);
    return old_value;
}

// move_if_noexcept
template <typename T>
constexpr std::conditional_t<
    !std::is_nothrow_move_constructible_v<T> &&
     std::is_copy_constructible_v<T>,
    const T&, T&&>
move_if_noexcept(T& x) noexcept {
    return mini::move(x);
}

} // namespace mini
```

#### Part 2: 移动友好的String类
```cpp
// mini_string.hpp
#pragma once
#include <cstring>
#include <utility>
#include <algorithm>

class MiniString {
    char* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;

    static constexpr size_t SSO_CAPACITY = 15;
    char sso_buffer_[SSO_CAPACITY + 1] = {};
    bool using_sso_ = true;

    bool is_sso() const { return using_sso_; }

public:
    // 默认构造
    MiniString() noexcept : using_sso_(true), size_(0) {
        sso_buffer_[0] = '\0';
    }

    // C字符串构造
    MiniString(const char* s) {
        size_t len = std::strlen(s);
        if (len <= SSO_CAPACITY) {
            std::memcpy(sso_buffer_, s, len + 1);
            size_ = len;
            using_sso_ = true;
        } else {
            data_ = new char[len + 1];
            std::memcpy(data_, s, len + 1);
            size_ = len;
            capacity_ = len;
            using_sso_ = false;
        }
    }

    // 拷贝构造
    MiniString(const MiniString& other) {
        if (other.is_sso()) {
            std::memcpy(sso_buffer_, other.sso_buffer_, SSO_CAPACITY + 1);
            size_ = other.size_;
            using_sso_ = true;
        } else {
            data_ = new char[other.capacity_ + 1];
            std::memcpy(data_, other.data_, other.size_ + 1);
            size_ = other.size_;
            capacity_ = other.capacity_;
            using_sso_ = false;
        }
    }

    // 移动构造
    MiniString(MiniString&& other) noexcept {
        if (other.is_sso()) {
            // SSO：必须拷贝（栈上数据无法"窃取"）
            std::memcpy(sso_buffer_, other.sso_buffer_, SSO_CAPACITY + 1);
            size_ = other.size_;
            using_sso_ = true;
        } else {
            // 堆分配：窃取指针
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            using_sso_ = false;

            // 将源对象置于有效的空状态
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
            other.using_sso_ = true;
            other.sso_buffer_[0] = '\0';
        }
    }

    // 析构
    ~MiniString() {
        if (!using_sso_) {
            delete[] data_;
        }
    }

    // Copy-and-Swap统一赋值
    MiniString& operator=(MiniString other) noexcept {
        swap(*this, other);
        return *this;
    }

    // swap
    friend void swap(MiniString& a, MiniString& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
        swap(a.capacity_, b.capacity_);
        swap(a.using_sso_, b.using_sso_);
        for (size_t i = 0; i <= SSO_CAPACITY; ++i) {
            swap(a.sso_buffer_[i], b.sso_buffer_[i]);
        }
    }

    // 访问
    const char* c_str() const noexcept {
        return using_sso_ ? sso_buffer_ : data_;
    }

    size_t size() const noexcept { return size_; }
    size_t capacity() const noexcept {
        return using_sso_ ? SSO_CAPACITY : capacity_;
    }
    bool empty() const noexcept { return size_ == 0; }

    char& operator[](size_t pos) {
        return using_sso_ ? sso_buffer_[pos] : data_[pos];
    }
    const char& operator[](size_t pos) const {
        return using_sso_ ? sso_buffer_[pos] : data_[pos];
    }

    // 追加
    MiniString& operator+=(const MiniString& other) {
        size_t new_size = size_ + other.size_;
        if (using_sso_ && new_size <= SSO_CAPACITY) {
            std::memcpy(sso_buffer_ + size_, other.c_str(), other.size_ + 1);
            size_ = new_size;
        } else {
            // 需要堆分配
            size_t new_cap = std::max(new_size, capacity_ * 2);
            char* new_data = new char[new_cap + 1];
            std::memcpy(new_data, c_str(), size_);
            std::memcpy(new_data + size_, other.c_str(), other.size_ + 1);

            if (!using_sso_) delete[] data_;

            data_ = new_data;
            size_ = new_size;
            capacity_ = new_cap;
            using_sso_ = false;
        }
        return *this;
    }
};
```

#### Part 3: 完美转发的工厂函数
```cpp
// factory.hpp
#pragma once
#include <memory>
#include <utility>

namespace mini {

// make_unique的实现
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// 通用工厂函数
template <typename T>
class Factory {
public:
    template <typename... Args>
    static T create(Args&&... args) {
        return T(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static std::unique_ptr<T> create_unique(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static std::shared_ptr<T> create_shared(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

// 延迟构造的包装器
template <typename T>
class Lazy {
    alignas(T) unsigned char storage_[sizeof(T)];
    bool initialized_ = false;

public:
    Lazy() = default;

    ~Lazy() {
        if (initialized_) {
            reinterpret_cast<T*>(storage_)->~T();
        }
    }

    template <typename... Args>
    T& emplace(Args&&... args) {
        if (initialized_) {
            reinterpret_cast<T*>(storage_)->~T();
        }
        new (storage_) T(std::forward<Args>(args)...);
        initialized_ = true;
        return *reinterpret_cast<T*>(storage_);
    }

    T& get() {
        if (!initialized_) {
            throw std::runtime_error("Lazy not initialized");
        }
        return *reinterpret_cast<T*>(storage_);
    }

    bool has_value() const { return initialized_; }
};

} // namespace mini
```

---

## 检验标准

### 知识检验
- [ ] 解释lvalue、prvalue、xvalue、glvalue、rvalue的区别
- [ ] std::move做了什么？它真的移动了数据吗？
- [ ] 什么是转发引用？如何识别？
- [ ] 引用折叠规则是什么？
- [ ] 为什么不应该对返回的局部变量使用std::move？

### 实践检验
- [ ] 实现的std::move和std::forward与标准行为一致
- [ ] MiniString正确处理SSO和移动语义
- [ ] 工厂函数能正确转发所有参数类型
- [ ] Lazy类正确实现延迟构造

### 输出物
1. `mini_utility.hpp`
2. `mini_string.hpp`
3. `factory.hpp`
4. `test_move_semantics.cpp`
5. `notes/month06_move_semantics.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 35小时 | 25% |
| 源码阅读 | 25小时 | 18% |
| mini_utility实现 | 20小时 | 14% |
| MiniString实现 | 30小时 | 21% |
| 工厂与测试 | 30小时 | 22% |

---

## 下月预告

Month 07将进入**异常安全与错误处理**，学习异常的底层实现、异常安全等级、std::expected（C++23）等现代错误处理方式。
