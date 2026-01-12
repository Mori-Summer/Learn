# Month 07: 异常安全与错误处理——构建健壮的C++代码

## 本月主题概述

异常是C++错误处理的核心机制，但也是最容易被误用的特性之一。本月将深入理解异常的底层实现、异常安全等级，掌握编写异常安全代码的技术，并探索现代C++的错误处理替代方案。

---

## 理论学习内容

### 第一周：异常的底层实现

**学习目标**：理解异常机制的运行时开销

**阅读材料**：
- [ ] Itanium C++ ABI异常处理规范
- [ ] 博客："How Exception Handling Works" (各平台实现)
- [ ] CppCon演讲："De-fragmenting C++: Making Exceptions and RTTI More Affordable"

**核心概念**：

#### 零成本异常模型（Zero-Cost Exceptions）
```cpp
// 现代编译器采用的模型
// "零成本"指：在不抛出异常时，几乎没有运行时开销
// 代价是：抛出异常时开销很大

// 实现原理：
// 1. 编译器生成异常处理表（Exception Tables）
// 2. 表存储在只读数据段，不影响正常执行路径
// 3. 抛出时通过栈展开（Stack Unwinding）查表
```

#### 栈展开（Stack Unwinding）
```cpp
void f() {
    std::string s = "hello";
    throw std::runtime_error("error");
    // s的析构函数会被调用
}

// 栈展开过程：
// 1. 保存异常对象
// 2. 查找当前栈帧的异常处理表
// 3. 如果有匹配的catch，跳转处理
// 4. 否则，析构所有局部对象，回到上一栈帧
// 5. 重复直到找到匹配的catch或terminate
```

#### 异常表结构
```cpp
// 伪代码表示异常处理表
struct ExceptionTable {
    // Landing Pad：catch块的入口点
    // Type Info：catch的类型信息
    // Cleanup：需要调用的析构函数
};

// 可以通过 readelf 查看
// readelf --unwind a.out
```

### 第二周：异常安全等级

**学习目标**：掌握异常安全保证的分类和实现

**核心概念**：

#### 四种异常安全等级
```cpp
// 1. 无保证（No Guarantee）
// 异常抛出后，程序状态未定义，可能有资源泄漏
void bad_function() {
    int* p = new int[100];
    may_throw();  // 如果这里抛异常，p泄漏
    delete[] p;
}

// 2. 基本保证（Basic Guarantee）
// 异常抛出后，程序状态有效但可能改变
// 没有资源泄漏，对象处于有效状态
void basic_guarantee(std::vector<int>& v) {
    v.push_back(1);
    may_throw();
    v.push_back(2);
    // 如果may_throw抛异常，v可能只有1个新元素或没有
}

// 3. 强保证（Strong Guarantee）
// 异常抛出后，程序状态与调用前完全相同
// 要么完全成功，要么完全失败（事务语义）
void strong_guarantee(std::vector<int>& v, int x) {
    std::vector<int> temp = v;  // 先拷贝
    temp.push_back(x);
    may_throw();
    temp.push_back(x * 2);
    v.swap(temp);  // 只在最后交换（swap是noexcept）
}

// 4. 不抛保证（No-throw Guarantee）
// 函数保证不抛出任何异常
void nothrow_guarantee() noexcept {
    // 所有操作都不会抛异常
}
```

#### STL的异常安全保证
```cpp
// vector::push_back
// - 如果元素类型的移动构造是noexcept：强保证
// - 否则使用拷贝构造：强保证
// - 如果拷贝构造也可能抛异常：基本保证

// vector::insert
// - 基本保证

// swap
// - 通常是noexcept（不抛保证）
```

### 第三周：编写异常安全代码

**学习目标**：掌握实现异常安全的关键技术

#### RAII是异常安全的基石
```cpp
// 不好的代码
void bad() {
    FILE* f = fopen("data.txt", "r");
    process();  // 可能抛异常
    fclose(f);  // 可能不会执行
}

// 好的代码
void good() {
    std::unique_ptr<FILE, decltype(&fclose)> f(
        fopen("data.txt", "r"), fclose);
    process();  // 即使抛异常，f也会被正确关闭
}
```

#### Copy-and-Swap实现强保证
```cpp
class Document {
    std::string title_;
    std::vector<Page> pages_;

public:
    // 异常安全的赋值
    Document& operator=(Document other) noexcept {
        // 1. other是按值传递，拷贝发生在参数传递时
        // 2. 如果拷贝失败，还没进入函数体
        // 3. swap是noexcept
        swap(*this, other);
        return *this;
    }

    friend void swap(Document& a, Document& b) noexcept {
        using std::swap;
        swap(a.title_, b.title_);
        swap(a.pages_, b.pages_);
    }
};
```

#### 写前拷贝（Copy-on-Write）实现强保证
```cpp
void update_document(Document& doc, const Updates& updates) {
    Document temp = doc;  // 先拷贝
    for (const auto& u : updates) {
        temp.apply(u);  // 在副本上操作
    }
    // 所有操作成功后才替换原对象
    doc = std::move(temp);  // 或 swap(doc, temp)
}
```

### 第四周：现代错误处理替代方案

**学习目标**：了解异常的替代方案

#### std::optional（可能无值）
```cpp
std::optional<int> parse_int(const std::string& s) {
    try {
        return std::stoi(s);
    } catch (...) {
        return std::nullopt;
    }
}

auto result = parse_int("42");
if (result) {
    std::cout << *result << "\n";
}
```

#### std::expected（C++23）
```cpp
// 类似Rust的Result<T, E>
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) {
        return std::unexpected("Division by zero");
    }
    return a / b;
}

auto result = divide(10, 2);
if (result) {
    std::cout << "Result: " << *result << "\n";
} else {
    std::cout << "Error: " << result.error() << "\n";
}
```

#### 错误码（Error Codes）
```cpp
enum class ErrorCode {
    Success,
    InvalidInput,
    IOError,
    OutOfMemory
};

struct Result {
    int value;
    ErrorCode error;

    bool ok() const { return error == ErrorCode::Success; }
};

Result compute(int x) {
    if (x < 0) {
        return {0, ErrorCode::InvalidInput};
    }
    return {x * 2, ErrorCode::Success};
}
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] libstdc++中的异常处理（`libsupc++/eh_*.cc`）
- [ ] `std::exception`层次结构
- [ ] `std::optional`实现
- [ ] `std::variant`的异常安全实现
- [ ] Boost.Outcome / std::expected的设计

---

## 实践项目

### 项目：实现异常安全的容器和错误处理库

#### Part 1: 实现mini_expected
```cpp
// mini_expected.hpp
#pragma once
#include <variant>
#include <utility>
#include <stdexcept>

namespace mini {

template <typename E>
class unexpected {
    E error_;
public:
    explicit unexpected(const E& e) : error_(e) {}
    explicit unexpected(E&& e) : error_(std::move(e)) {}

    const E& error() const& { return error_; }
    E& error() & { return error_; }
    E&& error() && { return std::move(error_); }
};

template <typename E>
unexpected(E) -> unexpected<E>;

template <typename T, typename E>
class expected {
    std::variant<T, unexpected<E>> storage_;

public:
    using value_type = T;
    using error_type = E;

    // 成功构造
    expected(const T& value) : storage_(value) {}
    expected(T&& value) : storage_(std::move(value)) {}

    // 失败构造
    expected(const unexpected<E>& e) : storage_(e) {}
    expected(unexpected<E>&& e) : storage_(std::move(e)) {}

    // 检查状态
    bool has_value() const noexcept {
        return std::holds_alternative<T>(storage_);
    }
    explicit operator bool() const noexcept { return has_value(); }

    // 访问值
    T& value() & {
        if (!has_value()) {
            throw std::runtime_error("expected has no value");
        }
        return std::get<T>(storage_);
    }

    const T& value() const& {
        if (!has_value()) {
            throw std::runtime_error("expected has no value");
        }
        return std::get<T>(storage_);
    }

    T&& value() && {
        if (!has_value()) {
            throw std::runtime_error("expected has no value");
        }
        return std::get<T>(std::move(storage_));
    }

    // 访问错误
    E& error() & {
        return std::get<unexpected<E>>(storage_).error();
    }

    const E& error() const& {
        return std::get<unexpected<E>>(storage_).error();
    }

    // 解引用
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(*this).value(); }

    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

    // value_or
    template <typename U>
    T value_or(U&& default_value) const& {
        return has_value() ? value() : static_cast<T>(std::forward<U>(default_value));
    }

    template <typename U>
    T value_or(U&& default_value) && {
        return has_value() ? std::move(*this).value()
                           : static_cast<T>(std::forward<U>(default_value));
    }

    // and_then (monadic)
    template <typename F>
    auto and_then(F&& f) & {
        using Result = std::invoke_result_t<F, T&>;
        if (has_value()) {
            return std::forward<F>(f)(value());
        }
        return Result(unexpected(error()));
    }

    template <typename F>
    auto and_then(F&& f) && {
        using Result = std::invoke_result_t<F, T&&>;
        if (has_value()) {
            return std::forward<F>(f)(std::move(*this).value());
        }
        return Result(unexpected(std::move(*this).error()));
    }

    // transform
    template <typename F>
    auto transform(F&& f) & {
        using U = std::invoke_result_t<F, T&>;
        if (has_value()) {
            return expected<U, E>(std::forward<F>(f)(value()));
        }
        return expected<U, E>(unexpected(error()));
    }

    // or_else
    template <typename F>
    auto or_else(F&& f) & {
        if (has_value()) {
            return *this;
        }
        return std::forward<F>(f)(error());
    }
};

// void特化
template <typename E>
class expected<void, E> {
    std::variant<std::monostate, unexpected<E>> storage_;

public:
    expected() : storage_(std::monostate{}) {}
    expected(const unexpected<E>& e) : storage_(e) {}
    expected(unexpected<E>&& e) : storage_(std::move(e)) {}

    bool has_value() const noexcept {
        return std::holds_alternative<std::monostate>(storage_);
    }
    explicit operator bool() const noexcept { return has_value(); }

    void value() const {
        if (!has_value()) {
            throw std::runtime_error("expected has no value");
        }
    }

    E& error() & {
        return std::get<unexpected<E>>(storage_).error();
    }

    const E& error() const& {
        return std::get<unexpected<E>>(storage_).error();
    }
};

} // namespace mini
```

#### Part 2: 异常安全的Stack类
```cpp
// safe_stack.hpp
#pragma once
#include <stdexcept>
#include <memory>

template <typename T>
class SafeStack {
    struct Node {
        T data;
        std::unique_ptr<Node> next;

        template <typename... Args>
        Node(Args&&... args) : data(std::forward<Args>(args)...) {}
    };

    std::unique_ptr<Node> head_;
    size_t size_ = 0;

public:
    // 基本保证的push
    void push(const T& value) {
        auto new_node = std::make_unique<Node>(value);
        new_node->next = std::move(head_);
        head_ = std::move(new_node);
        ++size_;
    }

    // 移动版本
    void push(T&& value) {
        auto new_node = std::make_unique<Node>(std::move(value));
        new_node->next = std::move(head_);
        head_ = std::move(new_node);
        ++size_;
    }

    // 强保证的emplace
    template <typename... Args>
    void emplace(Args&&... args) {
        auto new_node = std::make_unique<Node>(std::forward<Args>(args)...);
        new_node->next = std::move(head_);
        head_ = std::move(new_node);
        ++size_;
    }

    // 异常安全的pop
    // 问题：传统pop()返回值时，如果拷贝抛异常，元素已被移除
    // 解决方案1：分离top()和pop()
    T& top() {
        if (!head_) {
            throw std::runtime_error("Stack is empty");
        }
        return head_->data;
    }

    void pop() {
        if (!head_) {
            throw std::runtime_error("Stack is empty");
        }
        head_ = std::move(head_->next);
        --size_;
    }

    // 解决方案2：返回shared_ptr
    std::shared_ptr<T> pop_shared() {
        if (!head_) {
            throw std::runtime_error("Stack is empty");
        }
        auto result = std::make_shared<T>(std::move(head_->data));
        head_ = std::move(head_->next);
        --size_;
        return result;
    }

    // 解决方案3：使用optional
    std::optional<T> try_pop() {
        if (!head_) {
            return std::nullopt;
        }
        std::optional<T> result(std::move(head_->data));
        head_ = std::move(head_->next);
        --size_;
        return result;
    }

    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
};
```

#### Part 3: 事务类（实现强保证）
```cpp
// transaction.hpp
#pragma once
#include <functional>
#include <vector>
#include <exception>

class Transaction {
    std::vector<std::function<void()>> rollback_actions_;
    bool committed_ = false;

public:
    ~Transaction() {
        if (!committed_) {
            rollback();
        }
    }

    // 记录一个可回滚的操作
    template <typename Action, typename Rollback>
    void add(Action&& action, Rollback&& rollback) {
        // 先记录回滚操作
        rollback_actions_.push_back(std::forward<Rollback>(rollback));
        try {
            // 执行操作
            std::forward<Action>(action)();
        } catch (...) {
            // 如果失败，移除刚添加的回滚操作（因为它没成功执行）
            rollback_actions_.pop_back();
            throw;
        }
    }

    void commit() noexcept {
        committed_ = true;
        rollback_actions_.clear();
    }

    void rollback() noexcept {
        // 逆序执行回滚操作
        for (auto it = rollback_actions_.rbegin();
             it != rollback_actions_.rend(); ++it) {
            try {
                (*it)();
            } catch (...) {
                // 回滚操作不应该抛异常，但以防万一
            }
        }
        rollback_actions_.clear();
    }
};

// 使用示例
void transfer_money(Account& from, Account& to, int amount) {
    Transaction tx;

    int original_from = from.balance();
    tx.add(
        [&]() { from.withdraw(amount); },
        [&]() { from.deposit(amount); }  // 回滚：存回去
    );

    int original_to = to.balance();
    tx.add(
        [&]() { to.deposit(amount); },
        [&]() { to.withdraw(amount); }  // 回滚：取出来
    );

    // 如果到这里没有异常，提交事务
    tx.commit();
    // 如果之前有异常，析构函数会自动回滚
}
```

---

## 检验标准

### 知识检验
- [ ] 解释零成本异常模型的含义
- [ ] 四种异常安全等级分别是什么？各举一例
- [ ] 为什么RAII是异常安全的关键？
- [ ] Copy-and-Swap如何实现强保证？
- [ ] std::expected相比异常有什么优势和劣势？

### 实践检验
- [ ] mini_expected支持基本的值/错误处理和monadic操作
- [ ] SafeStack的所有操作都有明确的异常安全保证
- [ ] Transaction类能正确处理多步操作的回滚

### 输出物
1. `mini_expected.hpp`
2. `safe_stack.hpp`
3. `transaction.hpp`
4. `test_exception_safety.cpp`
5. `notes/month07_exceptions.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 35小时 | 25% |
| 源码阅读 | 25小时 | 18% |
| mini_expected实现 | 30小时 | 21% |
| SafeStack实现 | 25小时 | 18% |
| Transaction与测试 | 25小时 | 18% |

---

## 下月预告

Month 08将学习**函数对象与Lambda深度**，深入理解lambda的底层实现、std::function的类型擦除机制，以及高阶函数的应用。
