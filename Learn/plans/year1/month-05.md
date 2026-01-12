# Month 05: 模板元编程基础——编译期计算与类型推导

## 本月主题概述

模板元编程（Template Metaprogramming, TMP）是C++区别于其他语言的独特能力。本月将系统学习SFINAE、type_traits、constexpr等技术，理解编译期计算的原理，为理解STL和现代C++库的实现奠定基础。

---

## 理论学习内容

### 第一周：模板基础与类型推导

**学习目标**：深入理解模板实例化和类型推导规则

**阅读材料**：
- [ ] 《C++ Templates: The Complete Guide》第1-4章
- [ ] 《Effective Modern C++》Item 1-4（类型推导）
- [ ] CppCon演讲："Template Metaprogramming: Type Traits"

**核心概念**：

#### 模板参数推导规则
```cpp
template <typename T>
void f(T param);           // 按值传递

template <typename T>
void f(T& param);          // 左值引用

template <typename T>
void f(T&& param);         // 转发引用（万能引用）

// 规则1: 按值传递时，忽略cv限定符和引用
int x = 42;
const int cx = x;
const int& rx = x;
f(x);   // T = int
f(cx);  // T = int (const被忽略)
f(rx);  // T = int (引用和const都被忽略)

// 规则2: 左值引用时，保留cv限定符
f(x);   // T = int, param = int&
f(cx);  // T = const int, param = const int&

// 规则3: 转发引用的引用折叠
// T& &, T& &&, T&& & -> T&
// T&& && -> T&&
```

#### auto和decltype
```cpp
auto x = expr;           // 同模板类型推导
decltype(expr)           // 返回表达式的精确类型
decltype(auto) x = expr; // C++14，精确保持表达式类型

// decltype的特殊规则
int x = 0;
decltype(x)    // int (变量)
decltype((x))  // int& (表达式)
```

### 第二周：SFINAE原理与应用

**学习目标**：掌握SFINAE（替换失败即非错误）机制

**核心概念**：

#### SFINAE基本原理
```cpp
// 当模板参数替换导致无效类型时，不是编译错误，而是从重载集中移除该模板

template <typename T>
typename T::value_type get_value(T container) {  // 只对有value_type的类型有效
    return container.front();
}

template <typename T>
T get_value(T value) {  // 后备版本
    return value;
}

std::vector<int> v{1, 2, 3};
int x = 42;
get_value(v);  // 调用第一个版本
get_value(x);  // 调用第二个版本（第一个SFINAE失败）
```

#### enable_if的实现与使用
```cpp
// enable_if的实现
template <bool B, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> {
    using type = T;
};

// 使用方式1：返回类型
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
process(T value) {
    return value * 2;
}

// 使用方式2：默认模板参数
template <typename T,
          typename = std::enable_if_t<std::is_floating_point_v<T>>>
void process(T value) {
    // 只对浮点数有效
}

// 使用方式3：非类型模板参数（C++11后推荐）
template <typename T,
          std::enable_if_t<std::is_class_v<T>, int> = 0>
void process(T value) {
    // 只对类类型有效
}
```

#### void_t（C++17）
```cpp
// void_t的实现
template <typename...>
using void_t = void;

// 检测类型是否有某个成员
template <typename, typename = void>
struct has_value_type : std::false_type {};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {};

// 使用
static_assert(has_value_type<std::vector<int>>::value);
static_assert(!has_value_type<int>::value);
```

### 第三周：type_traits深度分析

**学习目标**：理解标准库type_traits的实现

**阅读路径**（GCC libstdc++）：
- `bits/type_traits.h`
- `type_traits`

**核心type_traits分类**：

#### 基本类型分类
```cpp
// 实现原理：模板特化
template <typename T> struct is_void : std::false_type {};
template <> struct is_void<void> : std::true_type {};

// 更复杂的类型特征使用编译器内置
template <typename T>
struct is_class : std::integral_constant<bool, __is_class(T)> {};
// __is_class是编译器内置，无法用纯C++实现
```

#### 类型关系
```cpp
// is_same实现
template <typename T, typename U>
struct is_same : std::false_type {};

template <typename T>
struct is_same<T, T> : std::true_type {};

// is_base_of使用编译器内置或SFINAE技巧
template <typename Base, typename Derived>
struct is_base_of : std::integral_constant<bool, __is_base_of(Base, Derived)> {};
```

#### 类型变换
```cpp
// remove_const
template <typename T> struct remove_const { using type = T; };
template <typename T> struct remove_const<const T> { using type = T; };

// remove_reference
template <typename T> struct remove_reference { using type = T; };
template <typename T> struct remove_reference<T&> { using type = T; };
template <typename T> struct remove_reference<T&&> { using type = T; };

// decay（模拟按值传递的类型退化）
template <typename T>
struct decay {
    using U = std::remove_reference_t<T>;
    using type = std::conditional_t<
        std::is_array_v<U>,
        std::remove_extent_t<U>*,
        std::conditional_t<
            std::is_function_v<U>,
            std::add_pointer_t<U>,
            std::remove_cv_t<U>
        >
    >;
};
```

### 第四周：constexpr与编译期计算

**学习目标**：掌握constexpr的能力和限制

**核心概念**：

#### constexpr函数
```cpp
// C++11: 单return语句
constexpr int factorial_11(int n) {
    return n <= 1 ? 1 : n * factorial_11(n - 1);
}

// C++14: 几乎完整的函数
constexpr int factorial_14(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

// C++20: 更多特性，包括try-catch、虚函数等
```

#### constexpr变量和if constexpr
```cpp
// constexpr变量必须在编译期初始化
constexpr int size = 100;
constexpr auto arr = std::array<int, 5>{1, 2, 3, 4, 5};

// if constexpr（C++17）：编译期分支
template <typename T>
auto process(T value) {
    if constexpr (std::is_integral_v<T>) {
        return value * 2;
    } else if constexpr (std::is_floating_point_v<T>) {
        return value * 2.5;
    } else {
        return value;
    }
}
// 只有匹配的分支会被实例化
```

#### consteval和constinit（C++20）
```cpp
// consteval：必须在编译期求值
consteval int must_be_compile_time(int n) {
    return n * n;
}

// constinit：保证静态初始化
constinit int global = must_be_compile_time(10);
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `std::conditional` 实现
- [ ] `std::conjunction` / `std::disjunction` 短路实现
- [ ] `std::invoke` 实现（处理各种可调用对象）
- [ ] `std::function` 类型擦除实现

---

## 实践项目

### 项目：实现mini_type_traits库

**目标**：实现一套基本的类型特征库

```cpp
// mini_type_traits.hpp
#pragma once

namespace mini {

// ==================== 基础设施 ====================

// integral_constant
template <typename T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant;
    constexpr operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; }
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

// ==================== 基本类型特征 ====================

// is_void
template <typename T> struct is_void : false_type {};
template <> struct is_void<void> : true_type {};
template <> struct is_void<const void> : true_type {};
template <> struct is_void<volatile void> : true_type {};
template <> struct is_void<const volatile void> : true_type {};

template <typename T>
inline constexpr bool is_void_v = is_void<T>::value;

// is_null_pointer
template <typename T> struct is_null_pointer : false_type {};
template <> struct is_null_pointer<std::nullptr_t> : true_type {};
template <> struct is_null_pointer<const std::nullptr_t> : true_type {};

// is_integral
template <typename T> struct is_integral : false_type {};
template <> struct is_integral<bool> : true_type {};
template <> struct is_integral<char> : true_type {};
template <> struct is_integral<signed char> : true_type {};
template <> struct is_integral<unsigned char> : true_type {};
template <> struct is_integral<short> : true_type {};
template <> struct is_integral<unsigned short> : true_type {};
template <> struct is_integral<int> : true_type {};
template <> struct is_integral<unsigned int> : true_type {};
template <> struct is_integral<long> : true_type {};
template <> struct is_integral<unsigned long> : true_type {};
template <> struct is_integral<long long> : true_type {};
template <> struct is_integral<unsigned long long> : true_type {};
// 需要处理cv限定版本...

template <typename T>
inline constexpr bool is_integral_v = is_integral<std::remove_cv_t<T>>::value;

// is_floating_point
template <typename T> struct is_floating_point : false_type {};
template <> struct is_floating_point<float> : true_type {};
template <> struct is_floating_point<double> : true_type {};
template <> struct is_floating_point<long double> : true_type {};

template <typename T>
inline constexpr bool is_floating_point_v = is_floating_point<std::remove_cv_t<T>>::value;

// is_arithmetic
template <typename T>
struct is_arithmetic : integral_constant<bool,
    is_integral_v<T> || is_floating_point_v<T>> {};

// is_pointer
template <typename T> struct is_pointer : false_type {};
template <typename T> struct is_pointer<T*> : true_type {};
template <typename T> struct is_pointer<T* const> : true_type {};
template <typename T> struct is_pointer<T* volatile> : true_type {};
template <typename T> struct is_pointer<T* const volatile> : true_type {};

// is_reference
template <typename T> struct is_lvalue_reference : false_type {};
template <typename T> struct is_lvalue_reference<T&> : true_type {};

template <typename T> struct is_rvalue_reference : false_type {};
template <typename T> struct is_rvalue_reference<T&&> : true_type {};

template <typename T>
struct is_reference : integral_constant<bool,
    is_lvalue_reference<T>::value || is_rvalue_reference<T>::value> {};

// ==================== 类型关系 ====================

// is_same
template <typename T, typename U> struct is_same : false_type {};
template <typename T> struct is_same<T, T> : true_type {};

template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

// ==================== 类型变换 ====================

// remove_const
template <typename T> struct remove_const { using type = T; };
template <typename T> struct remove_const<const T> { using type = T; };
template <typename T> using remove_const_t = typename remove_const<T>::type;

// remove_volatile
template <typename T> struct remove_volatile { using type = T; };
template <typename T> struct remove_volatile<volatile T> { using type = T; };
template <typename T> using remove_volatile_t = typename remove_volatile<T>::type;

// remove_cv
template <typename T> struct remove_cv {
    using type = remove_volatile_t<remove_const_t<T>>;
};
template <typename T> using remove_cv_t = typename remove_cv<T>::type;

// remove_reference
template <typename T> struct remove_reference { using type = T; };
template <typename T> struct remove_reference<T&> { using type = T; };
template <typename T> struct remove_reference<T&&> { using type = T; };
template <typename T> using remove_reference_t = typename remove_reference<T>::type;

// remove_cvref (C++20)
template <typename T> struct remove_cvref {
    using type = remove_cv_t<remove_reference_t<T>>;
};
template <typename T> using remove_cvref_t = typename remove_cvref<T>::type;

// add_const, add_volatile, add_cv
template <typename T> struct add_const { using type = const T; };
template <typename T> struct add_volatile { using type = volatile T; };
template <typename T> struct add_cv { using type = const volatile T; };

// add_lvalue_reference, add_rvalue_reference
// 需要处理void等特殊情况
template <typename T, typename = void>
struct add_lvalue_reference { using type = T; };
template <typename T>
struct add_lvalue_reference<T, std::void_t<T&>> { using type = T&; };

template <typename T, typename = void>
struct add_rvalue_reference { using type = T; };
template <typename T>
struct add_rvalue_reference<T, std::void_t<T&&>> { using type = T&&; };

// add_pointer
template <typename T>
struct add_pointer {
    using type = remove_reference_t<T>*;
};
template <typename T> using add_pointer_t = typename add_pointer<T>::type;

// ==================== conditional ====================

template <bool B, typename T, typename F>
struct conditional { using type = T; };

template <typename T, typename F>
struct conditional<false, T, F> { using type = F; };

template <bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

// ==================== enable_if ====================

template <bool B, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> { using type = T; };

template <bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

// ==================== void_t ====================

template <typename...>
using void_t = void;

// ==================== 检测惯用法 ====================

// 检测是否有size_type成员
template <typename, typename = void>
struct has_size_type : false_type {};

template <typename T>
struct has_size_type<T, void_t<typename T::size_type>> : true_type {};

// 检测是否有特定成员函数
template <typename T, typename = void>
struct has_size_method : false_type {};

template <typename T>
struct has_size_method<T, void_t<decltype(std::declval<T>().size())>> : true_type {};

// ==================== conjunction/disjunction ====================

template <typename...> struct conjunction : true_type {};
template <typename B1> struct conjunction<B1> : B1 {};
template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : conditional_t<bool(B1::value), conjunction<Bn...>, B1> {};

template <typename...> struct disjunction : false_type {};
template <typename B1> struct disjunction<B1> : B1 {};
template <typename B1, typename... Bn>
struct disjunction<B1, Bn...>
    : conditional_t<bool(B1::value), B1, disjunction<Bn...>> {};

template <typename B>
struct negation : integral_constant<bool, !bool(B::value)> {};

} // namespace mini
```

### 项目：编译期字符串处理

```cpp
// constexpr_string.hpp
#pragma once
#include <array>
#include <cstddef>

template <size_t N>
struct ConstString {
    char data[N]{};

    constexpr ConstString(const char (&str)[N]) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = str[i];
        }
    }

    constexpr size_t size() const { return N - 1; }
    constexpr char operator[](size_t i) const { return data[i]; }

    constexpr bool operator==(const ConstString& other) const {
        for (size_t i = 0; i < N; ++i) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }
};

// 编译期字符串拼接
template <size_t N1, size_t N2>
constexpr auto concat(const ConstString<N1>& s1, const ConstString<N2>& s2) {
    std::array<char, N1 + N2 - 1> result{};
    for (size_t i = 0; i < N1 - 1; ++i) result[i] = s1[i];
    for (size_t i = 0; i < N2; ++i) result[N1 - 1 + i] = s2[i];
    return result;
}

// 编译期计算字符串哈希
constexpr size_t fnv1a_hash(const char* str, size_t len) {
    size_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<size_t>(str[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

template <size_t N>
constexpr size_t hash(const ConstString<N>& s) {
    return fnv1a_hash(s.data, s.size());
}
```

---

## 检验标准

### 知识检验
- [ ] 解释SFINAE的工作原理
- [ ] auto、decltype、decltype(auto)的区别
- [ ] enable_if的三种使用方式
- [ ] void_t如何用于类型检测
- [ ] constexpr函数在C++11/14/17/20的能力演进

### 实践检验
- [ ] mini_type_traits库通过测试
- [ ] 能实现自定义的类型特征
- [ ] 能使用if constexpr进行编译期分支
- [ ] 能使用constexpr编写编译期算法

### 输出物
1. `mini_type_traits.hpp`
2. `constexpr_string.hpp`
3. `test_type_traits.cpp`
4. `notes/month05_templates.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 40小时 | 29% |
| 源码阅读 | 30小时 | 21% |
| type_traits实现 | 35小时 | 25% |
| constexpr项目 | 25小时 | 18% |
| 测试与文档 | 10小时 | 7% |

---

## 下月预告

Month 06将学习**完美转发与移动语义深度**，深入理解右值引用、移动语义、完美转发的实现原理，以及std::move、std::forward的源码分析。
