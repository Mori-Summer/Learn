# Month 57: C++26展望 - Reflection与Contracts

## 本月主题概述

C++26将带来几个革命性的特性，其中最受关注的是静态反射（Reflection）和契约编程（Contracts）。虽然标准尚在制定中，但这些特性的设计已经相对成熟。本月将预览这些即将到来的特性，理解它们的设计理念，并通过模拟实现和现有工具来实践相关概念。

### 学习目标
- 理解静态反射的设计和能力
- 掌握契约编程的概念和用法
- 了解模式匹配提案
- 预览其他C++26特性
- 使用现有工具实践类似功能

---

## 理论学习内容

### 第一周：静态反射（Reflection）

#### 阅读材料
1. P2996: 静态反射提案
2. P1240: 可扩展反射提案
3. 《C++ Templates》第二版 - 反射章节
4. Circle编译器文档

#### 核心概念

**什么是静态反射**
```
┌─────────────────────────────────────────────────────────┐
│                    C++26 静态反射                        │
└─────────────────────────────────────────────────────────┘

传统C++: 编译器知道所有类型信息，但程序员无法访问

┌──────────────────┐      ┌──────────────────┐
│   源代码         │  →   │   机器码          │
│   struct Foo {   │      │   (类型信息丢失)   │
│     int x;       │      │                  │
│   };            │      │                  │
└──────────────────┘      └──────────────────┘

静态反射: 允许程序员在编译期查询和操作类型信息

┌──────────────────┐      ┌──────────────────┐
│   源代码         │  →   │   机器码          │
│   struct Foo {   │      │   + 保留的元数据   │
│     int x;       │      │   + 生成的代码     │
│   };            │      │                  │
│                  │      │                  │
│   // 反射查询    │      │                  │
│   ^Foo          │      │                  │
└──────────────────┘      └──────────────────┘

关键特性：
1. 编译期执行 - 零运行时开销
2. 类型安全 - 完全的编译期检查
3. 可组合 - 与其他元编程特性配合
```

**P2996 反射语法（预览）**
```cpp
// 注意：这是提案中的语法，最终标准可能有变化

#include <meta>  // 反射头文件

// 基本反射操作符 ^
// ^T 返回 T 的反射信息

struct Point {
    int x;
    int y;
    std::string name;

    void print() const;
};

// 获取类型反射
constexpr auto point_info = ^Point;

// 查询类型名称
constexpr std::string_view name = std::meta::name_of(point_info);
// name == "Point"

// 获取成员
constexpr auto members = std::meta::members_of(point_info);

// 遍历成员（编译期）
template for (constexpr auto member : members) {
    // member 是每个成员的反射
    constexpr std::string_view member_name = std::meta::name_of(member);
    using MemberType = typename[:std::meta::type_of(member):];

    // 打印成员信息
    std::cout << member_name << ": " << typeid(MemberType).name() << "\n";
}

// 生成代码 - splice操作符 [: :]
template<typename T>
void printAllMembers(const T& obj) {
    constexpr auto members = std::meta::nonstatic_data_members_of(^T);

    template for (constexpr auto member : members) {
        // [:member:] 将反射"拼接"回代码
        std::cout << std::meta::name_of(member) << " = "
                  << obj.[:member:] << "\n";
    }
}

// 使用
Point p{10, 20, "origin"};
printAllMembers(p);
// 输出:
// x = 10
// y = 20
// name = origin
```

**反射的常见用途**
```cpp
// 1. 自动序列化
template<typename T>
std::string toJson(const T& obj) {
    std::string result = "{";
    bool first = true;

    constexpr auto members = std::meta::nonstatic_data_members_of(^T);

    template for (constexpr auto member : members) {
        if (!first) result += ",";
        first = false;

        result += "\"";
        result += std::meta::name_of(member);
        result += "\":";
        result += toJsonValue(obj.[:member:]);
    }

    result += "}";
    return result;
}

// 2. 自动比较
template<typename T>
bool equal(const T& a, const T& b) {
    constexpr auto members = std::meta::nonstatic_data_members_of(^T);

    return (... && (a.[:members:] == b.[:members:]));
}

// 3. 枚举字符串转换
enum class Color { Red, Green, Blue };

constexpr std::string_view colorToString(Color c) {
    template for (constexpr auto e : std::meta::enumerators_of(^Color)) {
        if (c == [:e:]) {
            return std::meta::name_of(e);
        }
    }
    return "Unknown";
}

// 4. 工厂模式
template<typename Base>
std::unique_ptr<Base> create(std::string_view className) {
    // 假设有方法获取所有派生类
    constexpr auto derived = std::meta::derived_classes_of(^Base);

    template for (constexpr auto cls : derived) {
        if (std::meta::name_of(cls) == className) {
            using DerivedType = typename[:cls:];
            return std::make_unique<DerivedType>();
        }
    }
    return nullptr;
}
```

### 第二周：契约编程（Contracts）

#### 阅读材料
1. P2900: 契约提案
2. Design by Contract原则（Bertrand Meyer）
3. D语言契约实现
4. 《Programming in Eiffel》

#### 核心概念

**契约三要素**
```
┌─────────────────────────────────────────────────────────┐
│                    契约编程概念                          │
└─────────────────────────────────────────────────────────┘

1. 前置条件 (Preconditions) - pre:
   调用函数前必须满足的条件
   责任在调用方

2. 后置条件 (Postconditions) - post:
   函数返回时必须满足的条件
   责任在被调用方

3. 不变量 (Invariants) - assert:
   程序执行过程中始终为真的条件

示例：

int divide(int a, int b)
    pre(b != 0)                    // 调用方保证b非零
    post(r: r * b == a || a % b != 0)  // 函数保证结果正确
{
    return a / b;
}

class BankAccount {
    int balance;

    contract_assert(balance >= 0);  // 类不变量

public:
    void withdraw(int amount)
        pre(amount > 0)
        pre(amount <= balance)
        post(balance == old(balance) - amount)
    {
        balance -= amount;
    }
};
```

**C++26契约语法（预览）**
```cpp
// 注意：语法仍在讨论中

// 前置条件
int sqrt(int n)
    pre(n >= 0)  // n必须非负
{
    // 实现
}

// 后置条件
int abs(int n)
    post(r: r >= 0)  // 返回值非负
    post(r: r == n || r == -n)  // 返回值是n或-n
{
    return n >= 0 ? n : -n;
}

// 多个条件
void copyArray(int* dest, const int* src, size_t n)
    pre(dest != nullptr)
    pre(src != nullptr)
    pre(n > 0)
    pre(dest + n <= src || src + n <= dest)  // 不重叠
{
    std::copy(src, src + n, dest);
}

// 契约继承
class Shape {
public:
    virtual double area() const
        post(r: r >= 0.0) = 0;
};

class Circle : public Shape {
    double radius;
public:
    double area() const override
        // 自动继承基类后置条件
        // 可以加强（更严格）
        post(r: r == 3.14159 * radius * radius)
    {
        return 3.14159 * radius * radius;
    }
};

// 契约级别
// 可以在编译时控制契约检查级别
// - off: 不检查任何契约
// - default: 检查标记为default的契约
// - audit: 检查所有契约

void process(std::vector<int>& v, int idx)
    pre(idx >= 0 && idx < v.size())  // default级别
    pre audit(isValidData(v))         // audit级别（开销大的检查）
{
    // ...
}
```

### 第三周：模式匹配（Pattern Matching）

#### 阅读材料
1. P2688: 模式匹配提案
2. Rust match表达式文档
3. 函数式编程模式匹配概念
4. Swift switch表达式

#### 核心概念

**模式匹配语法（预览）**
```cpp
// inspect表达式 - C++26模式匹配

// 基本用法
int describe(int n) {
    return inspect(n) {
        0 => "zero";
        1 => "one";
        _ => "many";  // _ 是通配符
    };
}

// 结构化绑定模式
struct Point { int x, y; };

std::string describePoint(Point p) {
    return inspect(p) {
        [0, 0] => "origin";
        [x, 0] => std::format("on x-axis at {}", x);
        [0, y] => std::format("on y-axis at {}", y);
        [x, y] => std::format("at ({}, {})", x, y);
    };
}

// 类型模式（与variant配合）
using Value = std::variant<int, double, std::string>;

std::string describe(const Value& v) {
    return inspect(v) {
        <int> i => std::format("int: {}", i);
        <double> d => std::format("double: {:.2f}", d);
        <std::string> s => std::format("string: \"{}\"", s);
    };
}

// 守卫条件
std::string classify(int n) {
    return inspect(n) {
        x if (x < 0) => "negative";
        x if (x == 0) => "zero";
        x if (x % 2 == 0) => "positive even";
        _ => "positive odd";
    };
}

// 嵌套模式
struct Person {
    std::string name;
    std::optional<int> age;
};

std::string describe(const Person& p) {
    return inspect(p) {
        [name, std::nullopt] =>
            std::format("{} (age unknown)", name);
        [name, age] if (*age < 18) =>
            std::format("{} is a minor", name);
        [name, age] =>
            std::format("{} is {} years old", name, *age);
    };
}

// 表达式语句（inspect作为语句）
void processCommand(const Command& cmd) {
    inspect(cmd) {
        <QuitCommand> => { cleanup(); exit(0); };
        <SaveCommand> [filename] => { save(filename); };
        <LoadCommand> [filename] => { load(filename); };
        _ => { std::cerr << "Unknown command\n"; };
    };
}

// 与范围结合
bool contains(const std::vector<int>& v, int target) {
    return inspect(v) {
        [] => false;  // 空vector
        [x, ...] if (x == target) => true;  // 首元素匹配
        [_, ...rest] => contains(rest, target);  // 递归
    };
}
```

### 第四周：其他C++26特性

#### 阅读材料
1. C++26提案列表
2. std::execution提案
3. 线性代数库提案
4. 网络库提案

#### 核心概念

**std::execution（异步执行框架）**
```cpp
// 详见Month 58专门讲解
```

**其他重要特性**
```cpp
// 1. 占位符改进
// _0, _1 等占位符可能成为标准

auto f = std::bind_back(std::greater{}, _0);
// 等价于 [](auto x) { return x > 0; }

// 2. constexpr增强
// 更多标准库组件支持constexpr

constexpr std::vector<int> v = {1, 2, 3, 4, 5};
constexpr int sum = std::accumulate(v.begin(), v.end(), 0);
// 全部在编译期计算

// 3. std::simd (可能)
// 便携的SIMD支持
std::native_simd<float> a, b, c;
// 加载数据
a.copy_from(data1);
b.copy_from(data2);
// SIMD操作
c = a + b;
c *= 2.0f;
// 存储结果
c.copy_to(result);

// 4. Hazard Pointers
// 用于无锁编程的安全内存回收
std::hazard_pointer<MyData> hp;
MyData* ptr = hp.protect(shared_ptr.load());
// 安全访问ptr
// ...
hp.clear();

// 5. RCU (Read-Copy-Update)
// 高效的并发数据结构支持
std::rcu_domain domain;
// ...

// 6. std::hive (原名colony)
// 用于频繁插入删除的容器，迭代器稳定
std::hive<GameObject> objects;
auto it = objects.insert(obj);
// 其他删除不会使it失效
```

---

## 源码阅读任务

### 必读项目

1. **Circle Compiler** (https://github.com/seanbaxter/circle)
   - 已实现反射的编译器
   - 学习目标：理解反射实际用法
   - 阅读时间：12小时

2. **Boost.Contract** (https://github.com/boostorg/contract)
   - 学习目标：理解契约模拟实现
   - 阅读时间：8小时

3. **mpark/patterns** (https://github.com/mpark/patterns)
   - 学习目标：理解模式匹配模拟
   - 阅读时间：6小时

---

## 实践项目：使用现有工具实践未来特性

### 项目概述
使用现有的库和技术模拟C++26特性，为未来迁移做准备。

### 完整代码实现

#### 1. 反射模拟 - 使用宏和模板 (reflection_sim.hpp)

```cpp
#pragma once

#include <string>
#include <string_view>
#include <tuple>
#include <array>
#include <iostream>

namespace reflect {

// 成员信息
template<typename T, typename Class>
struct MemberInfo {
    const char* name;
    T Class::* pointer;

    T& get(Class& obj) const { return obj.*pointer; }
    const T& get(const Class& obj) const { return obj.*pointer; }
};

// 反射特征 - 需要为每个类型特化
template<typename T>
struct TypeInfo {
    static constexpr bool is_reflectable = false;
};

// 辅助宏定义反射
#define REFLECT_BEGIN(Type) \
    template<> struct reflect::TypeInfo<Type> { \
        static constexpr bool is_reflectable = true; \
        static constexpr const char* name = #Type; \
        using type = Type; \
        static constexpr auto members() { return std::make_tuple(

#define REFLECT_MEMBER(name) \
    reflect::MemberInfo<decltype(type::name), type>{#name, &type::name}

#define REFLECT_END() \
        ); } \
    };

// 遍历成员
template<typename T, typename Func, size_t... Is>
void forEachMemberImpl(T& obj, Func&& func, std::index_sequence<Is...>) {
    constexpr auto members = TypeInfo<T>::members();
    (func(std::get<Is>(members), std::get<Is>(members).get(obj)), ...);
}

template<typename T, typename Func>
void forEachMember(T& obj, Func&& func) {
    static_assert(TypeInfo<T>::is_reflectable, "Type is not reflectable");
    constexpr auto members = TypeInfo<T>::members();
    forEachMemberImpl(obj, std::forward<Func>(func),
                      std::make_index_sequence<std::tuple_size_v<decltype(members)>>{});
}

// const版本
template<typename T, typename Func>
void forEachMember(const T& obj, Func&& func) {
    static_assert(TypeInfo<T>::is_reflectable, "Type is not reflectable");
    constexpr auto members = TypeInfo<T>::members();
    forEachMemberImpl(const_cast<T&>(obj), std::forward<Func>(func),
                      std::make_index_sequence<std::tuple_size_v<decltype(members)>>{});
}

// 获取类型名
template<typename T>
constexpr const char* typeName() {
    if constexpr (TypeInfo<T>::is_reflectable) {
        return TypeInfo<T>::name;
    } else {
        return typeid(T).name();
    }
}

// 成员数量
template<typename T>
constexpr size_t memberCount() {
    static_assert(TypeInfo<T>::is_reflectable, "Type is not reflectable");
    return std::tuple_size_v<decltype(TypeInfo<T>::members())>;
}

} // namespace reflect

// ========== 示例使用 ==========

// 定义一个可反射的类型
struct Person {
    std::string name;
    int age;
    double height;
};

// 注册反射信息
REFLECT_BEGIN(Person)
    REFLECT_MEMBER(name),
    REFLECT_MEMBER(age),
    REFLECT_MEMBER(height)
REFLECT_END()

// 另一个类型
struct Point3D {
    float x, y, z;
};

REFLECT_BEGIN(Point3D)
    REFLECT_MEMBER(x),
    REFLECT_MEMBER(y),
    REFLECT_MEMBER(z)
REFLECT_END()
```

#### 2. 基于反射的序列化 (serialization.hpp)

```cpp
#pragma once

#include "reflection_sim.hpp"
#include <sstream>
#include <vector>
#include <map>

namespace serialize {

// JSON序列化（简化版）
class JsonSerializer {
    std::ostringstream oss_;
    int indent_ = 0;
    bool prettyPrint_ = true;

    void writeIndent() {
        if (prettyPrint_) {
            oss_ << std::string(indent_ * 2, ' ');
        }
    }

    void writeNewline() {
        if (prettyPrint_) oss_ << "\n";
    }

public:
    explicit JsonSerializer(bool pretty = true) : prettyPrint_(pretty) {}

    // 基本类型
    void write(int value) { oss_ << value; }
    void write(double value) { oss_ << value; }
    void write(float value) { oss_ << value; }
    void write(bool value) { oss_ << (value ? "true" : "false"); }

    void write(const std::string& value) {
        oss_ << "\"" << value << "\"";  // 简化，未处理转义
    }

    void write(const char* value) {
        oss_ << "\"" << value << "\"";
    }

    // vector
    template<typename T>
    void write(const std::vector<T>& vec) {
        oss_ << "[";
        writeNewline();
        indent_++;

        for (size_t i = 0; i < vec.size(); ++i) {
            writeIndent();
            write(vec[i]);
            if (i < vec.size() - 1) oss_ << ",";
            writeNewline();
        }

        indent_--;
        writeIndent();
        oss_ << "]";
    }

    // map
    template<typename K, typename V>
    void write(const std::map<K, V>& m) {
        oss_ << "{";
        writeNewline();
        indent_++;

        size_t i = 0;
        for (const auto& [key, value] : m) {
            writeIndent();
            write(key);
            oss_ << ": ";
            write(value);
            if (i < m.size() - 1) oss_ << ",";
            writeNewline();
            ++i;
        }

        indent_--;
        writeIndent();
        oss_ << "}";
    }

    // 可反射类型
    template<typename T>
    requires reflect::TypeInfo<T>::is_reflectable
    void write(const T& obj) {
        oss_ << "{";
        writeNewline();
        indent_++;

        bool first = true;
        reflect::forEachMember(obj, [&](const auto& member, const auto& value) {
            if (!first) {
                oss_ << ",";
                writeNewline();
            }
            first = false;

            writeIndent();
            oss_ << "\"" << member.name << "\": ";
            write(value);
        });

        writeNewline();
        indent_--;
        writeIndent();
        oss_ << "}";
    }

    std::string str() const { return oss_.str(); }
};

template<typename T>
std::string toJson(const T& obj, bool pretty = true) {
    JsonSerializer serializer(pretty);
    serializer.write(obj);
    return serializer.str();
}

// 打印任何可反射类型
template<typename T>
requires reflect::TypeInfo<T>::is_reflectable
void debugPrint(const T& obj, std::ostream& os = std::cout) {
    os << reflect::typeName<T>() << " {\n";

    reflect::forEachMember(obj, [&](const auto& member, const auto& value) {
        os << "  " << member.name << " = " << value << "\n";
    });

    os << "}\n";
}

// 比较两个可反射对象
template<typename T>
requires reflect::TypeInfo<T>::is_reflectable
bool equals(const T& a, const T& b) {
    bool result = true;
    reflect::forEachMember(a, [&](const auto& member, const auto& valueA) {
        if (result) {
            result = (valueA == member.get(b));
        }
    });
    return result;
}

// 克隆对象
template<typename T>
requires reflect::TypeInfo<T>::is_reflectable
T clone(const T& obj) {
    T result;
    reflect::forEachMember(obj, [&](const auto& member, const auto& value) {
        member.get(result) = value;
    });
    return result;
}

} // namespace serialize
```

#### 3. 契约模拟 (contracts_sim.hpp)

```cpp
#pragma once

#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <functional>

namespace contracts {

// 契约违反异常
class ContractViolation : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

// 契约检查级别
enum class Level {
    Off,     // 不检查
    Default, // 默认检查
    Audit    // 全部检查
};

// 全局检查级别
inline Level globalLevel = Level::Default;

// 契约处理策略
enum class ViolationPolicy {
    Terminate,  // 终止程序
    Throw,      // 抛出异常
    Log         // 仅记录
};

inline ViolationPolicy globalPolicy = ViolationPolicy::Throw;

// 处理契约违反
inline void handleViolation(
    const char* type,
    const char* condition,
    const std::source_location& loc
) {
    std::string msg = std::string(type) + " violation: " + condition +
                     " at " + loc.file_name() + ":" +
                     std::to_string(loc.line());

    switch (globalPolicy) {
        case ViolationPolicy::Terminate:
            std::cerr << msg << std::endl;
            std::terminate();
            break;
        case ViolationPolicy::Throw:
            throw ContractViolation(msg);
        case ViolationPolicy::Log:
            std::cerr << "[CONTRACT] " << msg << std::endl;
            break;
    }
}

// 前置条件宏
#define CONTRACT_PRE(condition) \
    do { \
        if (contracts::globalLevel >= contracts::Level::Default) { \
            if (!(condition)) { \
                contracts::handleViolation( \
                    "Precondition", #condition, std::source_location::current()); \
            } \
        } \
    } while(0)

#define CONTRACT_PRE_AUDIT(condition) \
    do { \
        if (contracts::globalLevel >= contracts::Level::Audit) { \
            if (!(condition)) { \
                contracts::handleViolation( \
                    "Precondition (audit)", #condition, std::source_location::current()); \
            } \
        } \
    } while(0)

// 后置条件宏
#define CONTRACT_POST(condition) \
    do { \
        if (contracts::globalLevel >= contracts::Level::Default) { \
            if (!(condition)) { \
                contracts::handleViolation( \
                    "Postcondition", #condition, std::source_location::current()); \
            } \
        } \
    } while(0)

// 不变量宏
#define CONTRACT_ASSERT(condition) \
    do { \
        if (contracts::globalLevel >= contracts::Level::Default) { \
            if (!(condition)) { \
                contracts::handleViolation( \
                    "Assertion", #condition, std::source_location::current()); \
            } \
        } \
    } while(0)

// RAII风格的后置条件检查
template<typename F>
class [[nodiscard]] PostConditionChecker {
    F check_;
    std::source_location loc_;

public:
    PostConditionChecker(F&& check, std::source_location loc = std::source_location::current())
        : check_(std::forward<F>(check)), loc_(loc) {}

    ~PostConditionChecker() noexcept(false) {
        if (globalLevel >= Level::Default && !check_()) {
            handleViolation("Postcondition", "lambda check", loc_);
        }
    }
};

#define CONTRACT_ENSURE(check) \
    auto _post_checker_##__LINE__ = contracts::PostConditionChecker([&]{ return (check); })

// 辅助：保存旧值用于后置条件
template<typename T>
class OldValue {
    T value_;
public:
    explicit OldValue(const T& v) : value_(v) {}
    const T& get() const { return value_; }
    operator const T&() const { return value_; }
};

#define CONTRACT_OLD(expr) contracts::OldValue(expr)

} // namespace contracts
```

#### 4. 模式匹配模拟 (pattern_sim.hpp)

```cpp
#pragma once

#include <variant>
#include <optional>
#include <functional>
#include <tuple>

namespace pattern {

// 通配符
struct Wildcard {};
constexpr Wildcard _;

// 匹配结果
template<typename T>
using MatchResult = std::optional<T>;

// 基本匹配器
template<typename T>
struct Matcher {
    std::function<bool(const T&)> predicate;
    std::function<void(const T&)> action;
};

// inspect模拟
template<typename T, typename... Cases>
auto inspect(const T& value, Cases&&... cases) {
    // 使用折叠表达式尝试每个case
    std::optional<std::invoke_result_t<decltype(std::get<1>(cases)), const T&>> result;

    auto tryCase = [&](auto&& caseItem) {
        if (!result) {
            auto&& [pred, action] = caseItem;
            if (pred(value)) {
                result = action(value);
                return true;
            }
        }
        return false;
    };

    (tryCase(cases), ...);

    return result;
}

// 常用谓词构建器
template<typename T>
auto equals(T target) {
    return [target](const auto& v) { return v == target; };
}

template<typename Pred>
auto when(Pred&& pred) {
    return std::forward<Pred>(pred);
}

inline auto always() {
    return [](const auto&) { return true; };
}

// 范围匹配
template<typename T>
auto inRange(T low, T high) {
    return [low, high](const auto& v) { return v >= low && v <= high; };
}

// variant匹配辅助
template<typename T, typename Variant>
bool holdsType(const Variant& v) {
    return std::holds_alternative<T>(v);
}

template<typename T>
auto ofType() {
    return [](const auto& v) {
        using V = std::decay_t<decltype(v)>;
        if constexpr (requires { std::get_if<T>(&v); }) {
            return std::holds_alternative<T>(v);
        } else {
            return false;
        }
    };
}

// 更简洁的API
template<typename T>
struct Case {
    template<typename Pred, typename Action>
    static auto make(Pred&& pred, Action&& action) {
        return std::make_pair(std::forward<Pred>(pred), std::forward<Action>(action));
    }
};

// 示例：简单的match函数
template<typename T, typename R, typename... Cases>
R match(const T& value, Cases&&... cases) {
    R result{};
    bool matched = false;

    auto tryMatch = [&](auto&& pred, auto&& action) {
        if (!matched && pred(value)) {
            result = action(value);
            matched = true;
        }
    };

    // 假设cases是pair<pred, action>
    (std::apply(tryMatch, std::forward<Cases>(cases)), ...);

    return result;
}

} // namespace pattern

// 便捷宏
#define CASE(pred, action) std::make_pair(pred, action)
#define DEFAULT_CASE(action) std::make_pair(pattern::always(), action)
```

#### 5. 使用示例 (main.cpp)

```cpp
#include "reflection_sim.hpp"
#include "serialization.hpp"
#include "contracts_sim.hpp"
#include "pattern_sim.hpp"
#include <iostream>
#include <variant>
#include <cmath>

// ========== 反射示例 ==========

void reflectionDemo() {
    std::cout << "=== Reflection Demo ===\n\n";

    Person person{"Alice", 30, 1.65};

    // 打印类型信息
    std::cout << "Type: " << reflect::typeName<Person>() << "\n";
    std::cout << "Member count: " << reflect::memberCount<Person>() << "\n\n";

    // 遍历成员
    std::cout << "Members:\n";
    reflect::forEachMember(person, [](const auto& member, const auto& value) {
        std::cout << "  " << member.name << " = " << value << "\n";
    });
    std::cout << "\n";

    // JSON序列化
    std::cout << "JSON:\n" << serialize::toJson(person) << "\n\n";

    // 调试打印
    serialize::debugPrint(person);
    std::cout << "\n";

    // 比较
    Person person2{"Alice", 30, 1.65};
    Person person3{"Bob", 25, 1.80};

    std::cout << "person == person2: " << serialize::equals(person, person2) << "\n";
    std::cout << "person == person3: " << serialize::equals(person, person3) << "\n";
}

// ========== 契约示例 ==========

// 安全除法
double safeDivide(double a, double b) {
    CONTRACT_PRE(b != 0);  // 前置条件

    double result = a / b;

    CONTRACT_POST(std::isfinite(result));  // 后置条件
    return result;
}

// 数组访问
int safeArrayAccess(const std::vector<int>& arr, size_t index) {
    CONTRACT_PRE(!arr.empty());
    CONTRACT_PRE(index < arr.size());

    return arr[index];
}

// 银行账户示例
class BankAccount {
    double balance_;
    std::string owner_;

    void checkInvariant() const {
        CONTRACT_ASSERT(balance_ >= 0);
        CONTRACT_ASSERT(!owner_.empty());
    }

public:
    BankAccount(std::string owner, double initial)
        : balance_(initial), owner_(std::move(owner)) {
        CONTRACT_PRE(initial >= 0);
        checkInvariant();
    }

    void deposit(double amount) {
        CONTRACT_PRE(amount > 0);
        auto oldBalance = CONTRACT_OLD(balance_);

        balance_ += amount;

        CONTRACT_POST(balance_ == oldBalance.get() + amount);
        checkInvariant();
    }

    void withdraw(double amount) {
        CONTRACT_PRE(amount > 0);
        CONTRACT_PRE(amount <= balance_);
        auto oldBalance = CONTRACT_OLD(balance_);

        balance_ -= amount;

        CONTRACT_POST(balance_ == oldBalance.get() - amount);
        checkInvariant();
    }

    double balance() const { return balance_; }
};

void contractsDemo() {
    std::cout << "=== Contracts Demo ===\n\n";

    // 正常使用
    std::cout << "10 / 2 = " << safeDivide(10, 2) << "\n";

    // 契约违反
    try {
        std::cout << "10 / 0 = ";
        safeDivide(10, 0);
    } catch (const contracts::ContractViolation& e) {
        std::cout << "Contract violated: " << e.what() << "\n";
    }

    std::cout << "\n";

    // 银行账户
    BankAccount account("Alice", 100);
    account.deposit(50);
    std::cout << "Balance after deposit: " << account.balance() << "\n";

    account.withdraw(30);
    std::cout << "Balance after withdraw: " << account.balance() << "\n";

    try {
        account.withdraw(200);  // 超出余额
    } catch (const contracts::ContractViolation& e) {
        std::cout << "Contract violated: " << e.what() << "\n";
    }
}

// ========== 模式匹配示例 ==========

std::string describeNumber(int n) {
    if (n == 0) return "zero";
    if (n < 0) return "negative";
    if (n % 2 == 0) return "positive even";
    return "positive odd";
}

// 使用variant
using JsonValue = std::variant<
    std::nullptr_t,
    bool,
    int,
    double,
    std::string
>;

std::string describeJson(const JsonValue& v) {
    return std::visit([](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            return val ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int>) {
            return "int: " + std::to_string(val);
        } else if constexpr (std::is_same_v<T, double>) {
            return "double: " + std::to_string(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "string: \"" + val + "\"";
        }
    }, v);
}

void patternMatchDemo() {
    std::cout << "=== Pattern Matching Demo ===\n\n";

    // 数字分类
    for (int n : {-5, 0, 3, 8}) {
        std::cout << n << " is " << describeNumber(n) << "\n";
    }
    std::cout << "\n";

    // JSON值描述
    std::vector<JsonValue> values = {
        nullptr,
        true,
        42,
        3.14,
        std::string("hello")
    };

    for (const auto& v : values) {
        std::cout << describeJson(v) << "\n";
    }
}

int main() {
    reflectionDemo();
    std::cout << "\n";
    contractsDemo();
    std::cout << "\n";
    patternMatchDemo();

    return 0;
}
```

---

## 检验标准

### 知识检验
1. [ ] 能够解释静态反射的核心概念
2. [ ] 理解契约编程的三要素
3. [ ] 掌握模式匹配的常见模式
4. [ ] 了解C++26其他重要提案
5. [ ] 能够评估新特性的应用场景

### 实践检验
1. [ ] 完成反射模拟库
2. [ ] 实现基于反射的序列化
3. [ ] 完成契约模拟库
4. [ ] 使用模拟库重构现有代码
5. [ ] 编写展示新特性的示例程序

### 代码质量
1. [ ] 模拟实现接近提案设计
2. [ ] 代码有良好的错误处理
3. [ ] 有完整的文档说明
4. [ ] 便于未来迁移到标准库

---

## 输出物清单

1. **学习笔记**
   - [ ] C++26反射提案分析
   - [ ] 契约编程设计笔记
   - [ ] 模式匹配提案解读

2. **代码产出**
   - [ ] 反射模拟库
   - [ ] 契约模拟库
   - [ ] 模式匹配模拟
   - [ ] 示例应用

3. **文档产出**
   - [ ] C++26特性预览文档
   - [ ] 迁移准备指南
   - [ ] 最佳实践建议

---

## 时间分配表

| 周次 | 理论学习 | 源码阅读 | 项目实践 | 总计 |
|------|----------|----------|----------|------|
| Week 1 | 18h | 8h | 9h | 35h |
| Week 2 | 15h | 8h | 12h | 35h |
| Week 3 | 12h | 5h | 18h | 35h |
| Week 4 | 10h | 5h | 20h | 35h |
| **总计** | **55h** | **26h** | **59h** | **140h** |

---

## 下月预告

**Month 58: std::execution与异步执行框架**

下个月将深入学习C++26的异步执行框架：
- Sender/Receiver模型
- 执行上下文和调度器
- 结构化并发
- 取消和停止令牌
- 实践项目：构建异步任务系统

建议提前：
1. 复习C++20协程
2. 了解libunifex库
3. 学习结构化并发概念
