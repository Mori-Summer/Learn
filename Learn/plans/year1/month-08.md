# Month 08: 函数对象与Lambda深度——可调用对象的本质

## 本月主题概述

Lambda表达式是现代C++最强大的特性之一，它本质上是编译器生成的函数对象。本月将深入理解lambda的底层实现、std::function的类型擦除机制，以及函数式编程在C++中的应用。

---

## 理论学习内容

### 第一周：函数对象基础

**学习目标**：理解可调用对象的统一模型

**阅读材料**：
- [ ] 《Effective Modern C++》Item 31-34
- [ ] CppCon演讲："Back to Basics: Lambdas"
- [ ] 博客：Lambda Under the Hood

**核心概念**：

#### 可调用对象的分类
```cpp
// 1. 普通函数
int add(int a, int b) { return a + b; }

// 2. 函数指针
int (*fp)(int, int) = add;

// 3. 成员函数指针
struct Foo {
    int multiply(int x) { return x * factor; }
    int factor = 2;
};
int (Foo::*mfp)(int) = &Foo::multiply;

// 4. 函数对象（仿函数）
struct Adder {
    int operator()(int a, int b) const { return a + b; }
};

// 5. Lambda表达式
auto lambda = [](int a, int b) { return a + b; };

// std::invoke可以统一调用所有可调用对象
std::invoke(add, 1, 2);           // 函数
std::invoke(fp, 1, 2);            // 函数指针
std::invoke(mfp, foo, 3);         // 成员函数
std::invoke(Adder{}, 1, 2);       // 函数对象
std::invoke(lambda, 1, 2);        // Lambda
```

#### 函数对象的优势
```cpp
// 1. 可以携带状态
struct Counter {
    int count = 0;
    int operator()(int x) {
        ++count;
        return x * 2;
    }
};

// 2. 内联优化
// 函数对象调用编译器可以直接内联
// 函数指针调用通常不能内联（间接调用）

// 3. 类型安全
// 每个函数对象都有独立类型
Counter c1, c2;  // c1和c2类型相同但状态独立
```

### 第二周：Lambda表达式深度

**学习目标**：理解lambda的编译器实现

#### Lambda的本质
```cpp
// Lambda表达式
auto lambda = [x, &y](int a) mutable { x += a; return x + y; };

// 编译器生成的等价代码
class __lambda_unique_name {
    int x;      // 值捕获
    int& y;     // 引用捕获

public:
    __lambda_unique_name(int x, int& y) : x(x), y(y) {}

    // mutable使operator()非const
    int operator()(int a) {
        x += a;
        return x + y;
    }
};
auto lambda = __lambda_unique_name(x, y);
```

#### 捕获方式详解
```cpp
int x = 1, y = 2, z = 3;

// 值捕获
[x]{}                   // 只捕获x，按值
[=]{}                   // 捕获所有，按值

// 引用捕获
[&x]{}                  // 只捕获x，按引用
[&]{}                   // 捕获所有，按引用

// 混合捕获
[=, &x]{}               // 默认按值，x按引用
[&, x]{}                // 默认按引用，x按值

// 初始化捕获（C++14）
[z = x + y]{}           // z是一个新变量，初始化为x+y
[p = std::move(ptr)]{}  // 移动捕获
[&r = x]{}              // r是x的引用

// this捕获
struct Foo {
    int value;
    auto get_lambda() {
        return [this]() { return value; };   // 捕获this指针
        return [*this]() { return value; };  // C++17，拷贝整个对象
    }
};
```

#### 泛型Lambda（C++14）
```cpp
// auto参数 -> 模板operator()
auto generic = [](auto x, auto y) { return x + y; };

// 编译器生成
struct __generic_lambda {
    template <typename T, typename U>
    auto operator()(T x, U y) const {
        return x + y;
    }
};

// C++20 template lambda
auto template_lambda = []<typename T>(std::vector<T>& v) {
    return v.size();
};
```

### 第三周：std::function与类型擦除

**学习目标**：理解std::function的实现原理

#### 类型擦除概念
```cpp
// 问题：不同lambda有不同类型
auto l1 = [](int x) { return x; };
auto l2 = [](int x) { return x * 2; };
// decltype(l1) != decltype(l2)

// std::function通过类型擦除统一类型
std::function<int(int)> f1 = l1;
std::function<int(int)> f2 = l2;
// f1和f2类型相同
```

#### std::function实现原理（简化版）
```cpp
template <typename>
class Function;  // 主模板未定义

template <typename R, typename... Args>
class Function<R(Args...)> {
    // 类型擦除的基类
    struct CallableBase {
        virtual R invoke(Args...) = 0;
        virtual CallableBase* clone() const = 0;
        virtual ~CallableBase() = default;
    };

    // 持有具体可调用对象的派生类
    template <typename F>
    struct CallableImpl : CallableBase {
        F func;

        CallableImpl(F f) : func(std::move(f)) {}

        R invoke(Args... args) override {
            return func(std::forward<Args>(args)...);
        }

        CallableBase* clone() const override {
            return new CallableImpl(func);
        }
    };

    CallableBase* callable_ = nullptr;

public:
    Function() = default;

    template <typename F>
    Function(F f) : callable_(new CallableImpl<F>(std::move(f))) {}

    Function(const Function& other) {
        if (other.callable_) {
            callable_ = other.callable_->clone();
        }
    }

    Function(Function&& other) noexcept {
        callable_ = other.callable_;
        other.callable_ = nullptr;
    }

    ~Function() { delete callable_; }

    R operator()(Args... args) {
        if (!callable_) {
            throw std::bad_function_call();
        }
        return callable_->invoke(std::forward<Args>(args)...);
    }

    explicit operator bool() const { return callable_ != nullptr; }
};
```

#### std::function的开销
```cpp
// 1. 堆分配：存储可调用对象（除非有SBO优化）
// 2. 虚函数调用：invoke通过虚函数表
// 3. 无法内联：编译器看不到具体类型

// 优化：小对象优化（SBO）
// 如果可调用对象足够小，直接存储在std::function内部
// 大小通常是2-3个指针

// 何时使用std::function
// - 需要存储在容器中
// - 作为类成员
// - 运行时多态

// 何时避免std::function
// - 函数模板参数（用auto或模板）
// - 性能关键路径
```

### 第四周：高阶函数与函数式编程

**学习目标**：掌握函数式编程模式在C++中的应用

#### 常用高阶函数
```cpp
#include <algorithm>
#include <functional>

// map（变换）-> std::transform
std::vector<int> nums = {1, 2, 3, 4, 5};
std::vector<int> doubled;
std::transform(nums.begin(), nums.end(),
               std::back_inserter(doubled),
               [](int x) { return x * 2; });

// filter（过滤）-> std::copy_if / std::remove_if
std::vector<int> evens;
std::copy_if(nums.begin(), nums.end(),
             std::back_inserter(evens),
             [](int x) { return x % 2 == 0; });

// reduce（归约）-> std::accumulate / std::reduce
int sum = std::accumulate(nums.begin(), nums.end(), 0,
                          [](int acc, int x) { return acc + x; });

// C++20 ranges使用更自然
auto result = nums
    | std::views::filter([](int x) { return x % 2 == 0; })
    | std::views::transform([](int x) { return x * 2; });
```

#### 函数组合
```cpp
// 简单的compose
template <typename F, typename G>
auto compose(F f, G g) {
    return [=](auto x) { return f(g(x)); };
}

auto add1 = [](int x) { return x + 1; };
auto mul2 = [](int x) { return x * 2; };
auto add1_then_mul2 = compose(mul2, add1);  // (x + 1) * 2

// 可变参数compose
template <typename F>
auto compose(F f) { return f; }

template <typename F, typename... Fs>
auto compose(F f, Fs... fs) {
    return [=](auto x) { return f(compose(fs...)(x)); };
}
```

#### 柯里化（Currying）
```cpp
// 将多参数函数转换为单参数函数链
auto curry_add = [](int a) {
    return [a](int b) {
        return [a, b](int c) {
            return a + b + c;
        };
    };
};

int result = curry_add(1)(2)(3);  // 6

// 通用柯里化
template <typename F, typename... CapturedArgs>
auto curry(F f, CapturedArgs... captured) {
    return [=](auto... args) {
        return f(captured..., args...);
    };
}

auto add3 = [](int a, int b, int c) { return a + b + c; };
auto add3_with_1 = curry(add3, 1);      // f(b, c) = 1 + b + c
auto add3_with_1_2 = curry(add3, 1, 2); // f(c) = 1 + 2 + c
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `std::function`完整实现（含SBO）
- [ ] `std::invoke`实现
- [ ] `std::bind`实现
- [ ] `std::mem_fn`实现

---

## 实践项目

### 项目：实现完整的函数库

#### Part 1: mini_function
```cpp
// mini_function.hpp
#pragma once
#include <memory>
#include <utility>
#include <stdexcept>

namespace mini {

template <typename> class function;

template <typename R, typename... Args>
class function<R(Args...)> {
    // 小对象优化的缓冲区大小
    static constexpr size_t SBO_SIZE = sizeof(void*) * 3;

    // 类型擦除接口
    struct callable_base {
        virtual R invoke(Args...) = 0;
        virtual void copy_to(void* dest) const = 0;
        virtual void move_to(void* dest) noexcept = 0;
        virtual ~callable_base() = default;
    };

    // 实现类模板
    template <typename F>
    struct callable_impl : callable_base {
        F func_;

        template <typename Fn>
        callable_impl(Fn&& f) : func_(std::forward<Fn>(f)) {}

        R invoke(Args... args) override {
            return func_(std::forward<Args>(args)...);
        }

        void copy_to(void* dest) const override {
            new (dest) callable_impl(func_);
        }

        void move_to(void* dest) noexcept override {
            new (dest) callable_impl(std::move(func_));
        }
    };

    // SBO缓冲区或堆指针
    alignas(std::max_align_t) unsigned char storage_[SBO_SIZE];
    callable_base* callable_ = nullptr;
    bool is_small_ = false;

    callable_base* get_callable() {
        return is_small_ ? reinterpret_cast<callable_base*>(storage_) : callable_;
    }

    const callable_base* get_callable() const {
        return is_small_ ? reinterpret_cast<const callable_base*>(storage_) : callable_;
    }

    void destroy() {
        if (get_callable()) {
            if (is_small_) {
                get_callable()->~callable_base();
            } else {
                delete callable_;
            }
        }
        callable_ = nullptr;
        is_small_ = false;
    }

public:
    function() noexcept = default;

    function(std::nullptr_t) noexcept {}

    template <typename F,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, function>>>
    function(F&& f) {
        using impl_type = callable_impl<std::decay_t<F>>;

        if constexpr (sizeof(impl_type) <= SBO_SIZE &&
                      std::is_nothrow_move_constructible_v<std::decay_t<F>>) {
            // 使用SBO
            new (storage_) impl_type(std::forward<F>(f));
            is_small_ = true;
        } else {
            // 堆分配
            callable_ = new impl_type(std::forward<F>(f));
            is_small_ = false;
        }
    }

    function(const function& other) {
        if (other.get_callable()) {
            if (other.is_small_) {
                other.get_callable()->copy_to(storage_);
                is_small_ = true;
            } else {
                // 需要克隆到堆上
                // 这里简化处理，实际需要clone虚函数
                other.get_callable()->copy_to(storage_);
                is_small_ = true;  // 简化：总是用SBO
            }
        }
    }

    function(function&& other) noexcept {
        if (other.get_callable()) {
            if (other.is_small_) {
                other.get_callable()->move_to(storage_);
                is_small_ = true;
            } else {
                callable_ = other.callable_;
                other.callable_ = nullptr;
            }
        }
        other.is_small_ = false;
    }

    ~function() { destroy(); }

    function& operator=(const function& other) {
        if (this != &other) {
            function(other).swap(*this);
        }
        return *this;
    }

    function& operator=(function&& other) noexcept {
        if (this != &other) {
            destroy();
            if (other.get_callable()) {
                if (other.is_small_) {
                    other.get_callable()->move_to(storage_);
                    is_small_ = true;
                } else {
                    callable_ = other.callable_;
                    other.callable_ = nullptr;
                }
            }
            other.is_small_ = false;
        }
        return *this;
    }

    function& operator=(std::nullptr_t) noexcept {
        destroy();
        return *this;
    }

    void swap(function& other) noexcept {
        std::swap(storage_, other.storage_);
        std::swap(callable_, other.callable_);
        std::swap(is_small_, other.is_small_);
    }

    explicit operator bool() const noexcept {
        return get_callable() != nullptr;
    }

    R operator()(Args... args) {
        if (!get_callable()) {
            throw std::bad_function_call();
        }
        return get_callable()->invoke(std::forward<Args>(args)...);
    }
};

} // namespace mini
```

#### Part 2: mini_bind
```cpp
// mini_bind.hpp
#pragma once
#include <tuple>
#include <utility>

namespace mini {

// 占位符
template <int N>
struct placeholder {};

namespace placeholders {
    inline constexpr placeholder<1> _1{};
    inline constexpr placeholder<2> _2{};
    inline constexpr placeholder<3> _3{};
}

// 判断是否是占位符
template <typename T>
struct is_placeholder : std::integral_constant<int, 0> {};

template <int N>
struct is_placeholder<placeholder<N>> : std::integral_constant<int, N> {};

// 获取参数（如果是占位符则从调用参数中取，否则使用绑定参数）
template <typename T, typename Tuple>
decltype(auto) get_arg(T&& bound_arg, Tuple&&) {
    return std::forward<T>(bound_arg);
}

template <int N, typename Tuple>
decltype(auto) get_arg(placeholder<N>, Tuple&& call_args) {
    return std::get<N - 1>(std::forward<Tuple>(call_args));
}

// bind返回的可调用对象
template <typename F, typename... BoundArgs>
class binder {
    F func_;
    std::tuple<BoundArgs...> bound_args_;

    template <typename Tuple, size_t... Is>
    decltype(auto) call_impl(Tuple&& call_args, std::index_sequence<Is...>) {
        return func_(get_arg(std::get<Is>(bound_args_),
                            std::forward<Tuple>(call_args))...);
    }

public:
    template <typename Fn, typename... Args>
    binder(Fn&& f, Args&&... args)
        : func_(std::forward<Fn>(f))
        , bound_args_(std::forward<Args>(args)...) {}

    template <typename... CallArgs>
    decltype(auto) operator()(CallArgs&&... call_args) {
        return call_impl(std::forward_as_tuple(std::forward<CallArgs>(call_args)...),
                        std::index_sequence_for<BoundArgs...>{});
    }
};

// bind函数
template <typename F, typename... Args>
auto bind(F&& f, Args&&... args) {
    return binder<std::decay_t<F>, std::decay_t<Args>...>(
        std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace mini
```

#### Part 3: 函数式工具库
```cpp
// functional_utils.hpp
#pragma once
#include <utility>
#include <tuple>

namespace mini {

// compose: f(g(x))
template <typename F, typename G>
auto compose(F&& f, G&& g) {
    return [f = std::forward<F>(f), g = std::forward<G>(g)]
           (auto&&... args) {
        return f(g(std::forward<decltype(args)>(args)...));
    };
}

// pipe: g(f(x))，更符合阅读顺序
template <typename F, typename G>
auto pipe(F&& f, G&& g) {
    return compose(std::forward<G>(g), std::forward<F>(f));
}

// 多函数组合
template <typename F>
auto compose_all(F&& f) {
    return std::forward<F>(f);
}

template <typename F, typename... Fs>
auto compose_all(F&& f, Fs&&... fs) {
    return compose(std::forward<F>(f), compose_all(std::forward<Fs>(fs)...));
}

// partial application
template <typename F, typename... PartialArgs>
auto partial(F&& f, PartialArgs&&... partial_args) {
    return [f = std::forward<F>(f),
            captured = std::make_tuple(std::forward<PartialArgs>(partial_args)...)]
           (auto&&... args) mutable {
        return std::apply(f, std::tuple_cat(
            captured,
            std::forward_as_tuple(std::forward<decltype(args)>(args)...)
        ));
    };
}

// memoize: 缓存函数结果
template <typename F>
auto memoize(F f) {
    using ResultType = std::invoke_result_t<F, int>;  // 简化：假设单参数int
    std::unordered_map<int, ResultType> cache;

    return [f = std::move(f), cache = std::move(cache)](int arg) mutable {
        auto it = cache.find(arg);
        if (it != cache.end()) {
            return it->second;
        }
        auto result = f(arg);
        cache[arg] = result;
        return result;
    };
}

// Y组合子：实现匿名递归
template <typename F>
auto Y(F f) {
    return [f](auto... args) {
        return f(f, args...);
    };
}

// 使用Y组合子的阶乘
auto factorial = Y([](auto self, int n) -> int {
    return n <= 1 ? 1 : n * self(self, n - 1);
});

} // namespace mini
```

---

## 检验标准

### 知识检验
- [ ] Lambda表达式会被编译器转换成什么？
- [ ] 解释值捕获和引用捕获的区别，以及init capture
- [ ] std::function如何实现类型擦除？
- [ ] std::function的性能开销有哪些？
- [ ] 什么时候应该用std::function，什么时候用auto或模板？

### 实践检验
- [ ] mini_function正确实现SBO优化
- [ ] mini_bind支持占位符和部分应用
- [ ] 函数式工具库能正确组合函数

### 输出物
1. `mini_function.hpp`
2. `mini_bind.hpp`
3. `functional_utils.hpp`
4. `test_functional.cpp`
5. `notes/month08_lambda.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 30小时 | 21% |
| 源码阅读 | 25小时 | 18% |
| mini_function实现 | 35小时 | 25% |
| mini_bind实现 | 25小时 | 18% |
| 函数式工具 | 25小时 | 18% |

---

## 下月预告

Month 09将学习**迭代器与算法库深度**，深入分析STL算法的实现，理解迭代器分类，并实现自定义迭代器和算法。
