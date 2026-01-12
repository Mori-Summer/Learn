# Month 56: C++23新特性 (C++23 New Features)

## 本月主题概述

C++23是C++语言的最新标准，带来了许多期待已久的特性。本月将系统学习C++23的核心新特性，包括std::expected错误处理、std::mdspan多维数组视图、改进的ranges库、std::print格式化输出等。通过实践这些新特性，提升代码的表达力、安全性和性能。

### 学习目标
- 掌握std::expected及其单子操作
- 理解std::mdspan的设计和使用
- 学习ranges库的增强功能
- 掌握std::print和格式化改进
- 了解其他实用新特性

---

## 理论学习内容

### 第一周：std::expected与错误处理

#### 阅读材料
1. P0323R12: std::expected提案
2. 《Functional Programming in C++》
3. Rust Result类型文档
4. Herb Sutter错误处理文章

#### 核心概念

**std::expected基础**
```cpp
#include <expected>
#include <string>
#include <iostream>

// std::expected<T, E>: 要么包含值T，要么包含错误E
// 类似于Rust的Result<T, E>

enum class ParseError {
    EmptyInput,
    InvalidFormat,
    OutOfRange
};

std::expected<int, ParseError> parseInt(std::string_view str) {
    if (str.empty()) {
        return std::unexpected(ParseError::EmptyInput);
    }

    try {
        size_t pos;
        int value = std::stoi(std::string(str), &pos);

        if (pos != str.size()) {
            return std::unexpected(ParseError::InvalidFormat);
        }

        return value;  // 隐式转换为expected

    } catch (const std::out_of_range&) {
        return std::unexpected(ParseError::OutOfRange);
    } catch (...) {
        return std::unexpected(ParseError::InvalidFormat);
    }
}

void basicUsage() {
    auto result = parseInt("42");

    // 方法1：检查并访问
    if (result.has_value()) {
        std::cout << "Value: " << result.value() << "\n";
    } else {
        std::cout << "Error: " << static_cast<int>(result.error()) << "\n";
    }

    // 方法2：operator* 和 operator->
    if (result) {
        std::cout << "Value: " << *result << "\n";
    }

    // 方法3：value_or提供默认值
    int value = parseInt("invalid").value_or(-1);

    // 方法4：C++23单子操作（见下文）
}
```

**单子操作（Monadic Operations）**
```cpp
#include <expected>
#include <string>
#include <optional>

// and_then: 链式操作，成功时转换
// transform: 映射值，保持错误
// or_else: 处理错误，可能恢复
// transform_error: 映射错误类型

struct User {
    int id;
    std::string name;
};

struct DatabaseError {
    int code;
    std::string message;
};

std::expected<User, DatabaseError> findUser(int id);
std::expected<std::string, DatabaseError> getUserEmail(const User& user);
std::expected<bool, DatabaseError> sendEmail(const std::string& email);

// 传统方式：嵌套if检查
std::expected<bool, DatabaseError> sendEmailToUserOld(int userId) {
    auto userResult = findUser(userId);
    if (!userResult) {
        return std::unexpected(userResult.error());
    }

    auto emailResult = getUserEmail(*userResult);
    if (!emailResult) {
        return std::unexpected(emailResult.error());
    }

    return sendEmail(*emailResult);
}

// C++23方式：单子链式调用
std::expected<bool, DatabaseError> sendEmailToUserNew(int userId) {
    return findUser(userId)
        .and_then(getUserEmail)
        .and_then(sendEmail);
}

// transform: 映射成功值
std::expected<std::string, DatabaseError> getUserNameUpper(int userId) {
    return findUser(userId)
        .transform([](const User& u) {
            std::string upper = u.name;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            return upper;
        });
}

// or_else: 错误恢复
std::expected<User, DatabaseError> findUserWithFallback(int userId) {
    return findUser(userId)
        .or_else([](const DatabaseError& err) -> std::expected<User, DatabaseError> {
            if (err.code == 404) {
                // 返回默认用户
                return User{0, "Guest"};
            }
            return std::unexpected(err);
        });
}

// transform_error: 转换错误类型
enum class AppError { NotFound, NetworkError, Unknown };

std::expected<User, AppError> findUserAppError(int userId) {
    return findUser(userId)
        .transform_error([](const DatabaseError& err) {
            if (err.code == 404) return AppError::NotFound;
            if (err.code >= 500) return AppError::NetworkError;
            return AppError::Unknown;
        });
}
```

### 第二周：std::mdspan多维数组视图

#### 阅读材料
1. P0009R18: mdspan提案
2. Kokkos mdspan参考实现
3. NumPy ndarray设计
4. 科学计算数组库比较

#### 核心概念

**std::mdspan基础**
```cpp
#include <mdspan>
#include <vector>
#include <iostream>

void mdspanBasics() {
    // mdspan是非拥有的多维数组视图
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    // 创建2D视图 (3行4列，行主序)
    std::mdspan<int, std::extents<size_t, 3, 4>> matrix(data.data());

    // 访问元素
    for (size_t i = 0; i < matrix.extent(0); ++i) {
        for (size_t j = 0; j < matrix.extent(1); ++j) {
            std::cout << matrix[i, j] << " ";  // C++23多维下标
            // 或 matrix(i, j) 也可以
        }
        std::cout << "\n";
    }

    // 动态尺寸
    std::mdspan<int, std::dextents<size_t, 2>> dynamicMatrix(
        data.data(), 3, 4);

    // 混合静态/动态尺寸
    std::mdspan<int, std::extents<size_t, 3, std::dynamic_extent>>
        mixedMatrix(data.data(), 4);
}
```

**布局和访问器**
```cpp
#include <mdspan>

// 布局策略
void layoutExamples() {
    std::vector<float> data(12);

    // 行主序（C风格，默认）
    std::mdspan<float, std::extents<size_t, 3, 4>,
                std::layout_right> rowMajor(data.data());

    // 列主序（Fortran风格）
    std::mdspan<float, std::extents<size_t, 3, 4>,
                std::layout_left> colMajor(data.data());

    // 自定义步长
    std::array<size_t, 2> strides = {8, 2};  // 跳跃访问
    std::layout_stride::mapping<std::extents<size_t, 3, 4>>
        strideMapping(std::extents<size_t, 3, 4>{}, strides);

    std::mdspan<float, std::extents<size_t, 3, 4>,
                std::layout_stride> stridedView(data.data(), strideMapping);
}

// 子视图（Subspan）
void subspanExample() {
    std::vector<int> data(100);
    std::iota(data.begin(), data.end(), 0);

    // 10x10矩阵
    std::mdspan<int, std::extents<size_t, 10, 10>> matrix(data.data());

    // 提取子矩阵（需要辅助函数）
    auto submatrix = std::submdspan(
        matrix,
        std::tuple{2, 5},  // 行 [2, 5)
        std::tuple{3, 7}   // 列 [3, 7)
    );
    // submatrix是3x4的视图
}

// 与科学计算结合
template<typename T, typename Extents>
void matrixMultiply(
    std::mdspan<const T, Extents> A,
    std::mdspan<const T, Extents> B,
    std::mdspan<T, Extents> C
) {
    static_assert(Extents::rank() == 2);

    for (size_t i = 0; i < A.extent(0); ++i) {
        for (size_t j = 0; j < B.extent(1); ++j) {
            T sum = 0;
            for (size_t k = 0; k < A.extent(1); ++k) {
                sum += A[i, k] * B[k, j];
            }
            C[i, j] = sum;
        }
    }
}
```

### 第三周：Ranges增强与std::generator

#### 阅读材料
1. C++23 Ranges增强提案
2. std::generator提案P2502
3. Ranges库实践指南
4. 惰性求值设计模式

#### 核心概念

**Ranges新增功能**
```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

void rangesEnhancements() {
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // C++23: views::chunk - 分块
    for (auto chunk : numbers | std::views::chunk(3)) {
        std::cout << "Chunk: ";
        for (int n : chunk) std::cout << n << " ";
        std::cout << "\n";
    }
    // 输出: [1,2,3], [4,5,6], [7,8,9], [10]

    // C++23: views::slide - 滑动窗口
    for (auto window : numbers | std::views::slide(3)) {
        std::cout << "Window: ";
        for (int n : window) std::cout << n << " ";
        std::cout << "\n";
    }
    // 输出: [1,2,3], [2,3,4], [3,4,5], ...

    // C++23: views::chunk_by - 条件分块
    std::vector<int> data = {1, 1, 2, 2, 2, 3, 1, 1};
    for (auto group : data | std::views::chunk_by(std::equal_to{})) {
        std::cout << "Group: ";
        for (int n : group) std::cout << n << " ";
        std::cout << "\n";
    }
    // 按相等分组

    // C++23: views::join_with - 带分隔符连接
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}, {5}};
    for (int n : nested | std::views::join_with(0)) {
        std::cout << n << " ";
    }
    // 输出: 1 2 0 3 4 0 5

    // C++23: views::zip - 打包多个范围
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    std::vector<int> ages = {25, 30, 35};

    for (auto [name, age] : std::views::zip(names, ages)) {
        std::cout << name << " is " << age << " years old\n";
    }

    // C++23: views::zip_transform - 打包并变换
    for (auto result : std::views::zip_transform(
            [](const std::string& n, int a) {
                return n + ": " + std::to_string(a);
            },
            names, ages)) {
        std::cout << result << "\n";
    }

    // C++23: views::adjacent - 相邻元素组
    for (auto [a, b] : numbers | std::views::adjacent<2>) {
        std::cout << "(" << a << ", " << b << ") ";
    }

    // C++23: views::cartesian_product - 笛卡尔积
    std::vector<char> chars = {'a', 'b'};
    std::vector<int> nums = {1, 2, 3};

    for (auto [c, n] : std::views::cartesian_product(chars, nums)) {
        std::cout << c << n << " ";
    }
    // 输出: a1 a2 a3 b1 b2 b3
}

// ranges::to - 转换为容器
void rangesToContainer() {
    auto evens = std::views::iota(1, 20)
               | std::views::filter([](int n) { return n % 2 == 0; })
               | std::ranges::to<std::vector>();  // C++23

    // 也可以指定分配器等
    auto strings = std::views::iota(1, 5)
                 | std::views::transform([](int n) {
                       return std::to_string(n);
                   })
                 | std::ranges::to<std::vector<std::string>>();
}
```

**std::generator协程**
```cpp
#include <generator>
#include <iostream>

// std::generator: 简化的协程生成器

std::generator<int> fibonacci(int limit) {
    int a = 0, b = 1;
    while (a < limit) {
        co_yield a;
        auto next = a + b;
        a = b;
        b = next;
    }
}

std::generator<int> range(int start, int end, int step = 1) {
    for (int i = start; i < end; i += step) {
        co_yield i;
    }
}

// 嵌套生成器
std::generator<int> flatten(std::vector<std::vector<int>>& nested) {
    for (auto& inner : nested) {
        for (int val : inner) {
            co_yield val;
        }
    }
}

// 递归生成器（树遍历）
struct TreeNode {
    int value;
    TreeNode* left = nullptr;
    TreeNode* right = nullptr;
};

std::generator<int> inorderTraversal(TreeNode* node) {
    if (!node) co_return;

    co_yield std::ranges::elements_of(inorderTraversal(node->left));
    co_yield node->value;
    co_yield std::ranges::elements_of(inorderTraversal(node->right));
}

void generatorUsage() {
    // 斐波那契
    std::cout << "Fibonacci: ";
    for (int n : fibonacci(100)) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // 与ranges结合
    auto evenFib = fibonacci(1000)
                 | std::views::filter([](int n) { return n % 2 == 0; })
                 | std::views::take(5);

    for (int n : evenFib) {
        std::cout << n << " ";
    }
}
```

### 第四周：std::print与其他特性

#### 阅读材料
1. P2093: std::print提案
2. 显式对象参数（Deducing this）提案
3. constexpr增强提案
4. 其他C++23特性概览

#### 核心概念

**std::print格式化输出**
```cpp
#include <print>
#include <vector>
#include <map>

void printExamples() {
    // 基本使用
    std::print("Hello, {}!\n", "World");
    std::println("This adds a newline automatically");

    // 格式化选项
    int n = 42;
    std::print("Decimal: {}, Hex: {:x}, Binary: {:b}\n", n, n, n);
    std::print("Padded: {:>10}, Left: {:<10}\n", n, n);
    std::print("With sign: {:+}, Space: {: }\n", n, n);

    // 浮点数
    double pi = 3.14159265358979;
    std::print("Default: {}, Fixed: {:.2f}, Scientific: {:.2e}\n",
               pi, pi, pi);

    // 容器
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::print("Vector: {}\n", vec);  // 需要formatter特化

    // 输出到文件
    FILE* file = fopen("output.txt", "w");
    std::print(file, "Writing to file: {}\n", 42);
    fclose(file);

    // 输出到stderr
    std::print(stderr, "Error: {}\n", "something went wrong");

    // Unicode支持
    std::print("Unicode: {} {} {}\n", "你好", "こんにちは", "مرحبا");
}

// 自定义类型的formatter
struct Point {
    double x, y;
};

template<>
struct std::formatter<Point> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const Point& p, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};

void customFormatter() {
    Point p{3.5, 4.2};
    std::print("Point: {}\n", p);
}
```

**显式对象参数（Deducing this）**
```cpp
// C++23: 显式对象参数，解决完美转发问题

class Widget {
    std::string data_;

public:
    // 传统方式：需要两个或四个重载
    // std::string& getData() & { return data_; }
    // const std::string& getData() const& { return data_; }
    // std::string&& getData() && { return std::move(data_); }

    // C++23: 一个模板搞定
    template<typename Self>
    auto&& getData(this Self&& self) {
        return std::forward<Self>(self).data_;
    }

    // 递归lambda
    void processTree() {
        auto traverse = [](this auto& self, TreeNode* node) -> void {
            if (!node) return;
            self(node->left);
            std::print("{} ", node->value);
            self(node->right);
        };

        // traverse(root);
    }
};

// CRTP简化
template<typename Derived>
class Clonable {
public:
    // 传统CRTP
    // Derived clone() const {
    //     return *static_cast<const Derived*>(this);
    // }

    // C++23: deducing this
    auto clone(this auto&& self) {
        return self;  // 自动推导正确类型
    }
};

class MyClass : public Clonable<MyClass> {
    int value_;
public:
    explicit MyClass(int v) : value_(v) {}
};
```

**其他实用特性**
```cpp
// if consteval - 编译期条件
constexpr int compute(int n) {
    if consteval {
        // 编译期执行
        return n * n;
    } else {
        // 运行时执行
        return std::sqrt(n);  // 假设这里用某种近似
    }
}

// auto(x) 和 auto{x} - 强制拷贝
void autoCopy() {
    std::vector<int> vec = {1, 2, 3};
    auto copy1 = auto(vec);   // 拷贝
    auto copy2 = auto{vec};   // 拷贝

    // 用于转发时强制拷贝
    auto process = [](auto&& x) {
        doSomething(auto(x));  // 传递拷贝
    };
}

// static operator() 和 static operator[]
struct Functor {
    static int operator()(int x, int y) {  // C++23
        return x + y;
    }
};

// size_t字面量
void sizeLiteral() {
    auto s = 42uz;  // std::size_t
    auto z = 42z;   // std::ptrdiff_t (带符号)
}

// [[assume]] 属性
void assumeAttribute(int* ptr) {
    [[assume(ptr != nullptr)]];
    // 编译器可以假设ptr非空进行优化
    *ptr = 42;
}

// 多维下标运算符
class Matrix3D {
    std::vector<double> data_;
    size_t dim1_, dim2_, dim3_;

public:
    double& operator[](size_t i, size_t j, size_t k) {  // C++23
        return data_[i * dim2_ * dim3_ + j * dim3_ + k];
    }
};

// constexpr增强
constexpr std::vector<int> createVector() {  // C++23: constexpr容器
    std::vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    return v;
}

constexpr auto vec = createVector();  // 编译期创建vector

// std::unreachable()
int getValue(int type) {
    switch (type) {
        case 0: return 10;
        case 1: return 20;
        default:
            std::unreachable();  // 告诉编译器这里不可达
    }
}
```

---

## 源码阅读任务

### 必读项目

1. **libstdc++ expected实现**
   - https://github.com/gcc-mirror/gcc/tree/master/libstdc++-v3
   - 学习目标：理解expected的实现细节
   - 阅读时间：8小时

2. **mdspan参考实现**
   - https://github.com/kokkos/mdspan
   - 学习目标：理解多维数组视图设计
   - 阅读时间：10小时

3. **ranges-v3**
   - https://github.com/ericniebler/range-v3
   - 学习目标：理解ranges库设计
   - 阅读时间：8小时

---

## 实践项目：使用C++23重构代码库

### 项目概述
将一个现有的代码库使用C++23特性进行现代化重构，展示新特性的实际应用价值。

### 完整代码实现

#### 1. 错误处理重构 (result.hpp)

```cpp
#pragma once

#include <expected>
#include <string>
#include <variant>
#include <print>

namespace app {

// 统一错误类型
struct Error {
    int code;
    std::string message;
    std::string source;

    static Error notFound(std::string what) {
        return {404, std::format("{} not found", what), ""};
    }

    static Error invalid(std::string what) {
        return {400, std::format("Invalid {}", what), ""};
    }

    static Error internal(std::string what) {
        return {500, what, ""};
    }

    Error withSource(std::string src) const {
        return {code, message, src};
    }
};

template<typename T>
using Result = std::expected<T, Error>;

// 辅助函数
template<typename T>
Result<T> Ok(T value) {
    return value;
}

inline auto Err(Error error) {
    return std::unexpected(error);
}

// Result组合器
template<typename... Ts>
auto combine(Result<Ts>... results)
    -> Result<std::tuple<Ts...>> {

    bool allOk = (results.has_value() && ...);
    if (!allOk) {
        // 返回第一个错误
        Error firstError;
        ((results.has_value() || (firstError = results.error(), false)) || ...);
        return std::unexpected(firstError);
    }

    return std::tuple{*results...};
}

} // namespace app
```

#### 2. 数据处理管道 (pipeline.hpp)

```cpp
#pragma once

#include <ranges>
#include <vector>
#include <string>
#include <generator>
#include <print>
#include "result.hpp"

namespace app {

// 数据记录
struct Record {
    int id;
    std::string name;
    double value;
    std::string category;
};

// 使用generator读取数据
std::generator<Record> readRecords(std::string_view data) {
    // 模拟解析CSV
    std::vector<Record> records = {
        {1, "Alpha", 100.5, "A"},
        {2, "Beta", 200.3, "B"},
        {3, "Gamma", 150.0, "A"},
        {4, "Delta", 300.0, "B"},
        {5, "Epsilon", 50.0, "A"},
    };

    for (const auto& r : records) {
        co_yield r;
    }
}

// 数据处理管道
class DataPipeline {
public:
    // 使用C++23 ranges处理数据
    static auto process(auto&& records) {
        return records
            // 过滤
            | std::views::filter([](const Record& r) {
                return r.value > 100.0;
            })
            // 变换
            | std::views::transform([](const Record& r) {
                return Record{r.id, r.name, r.value * 1.1, r.category};
            });
    }

    // 按类别分组统计
    static auto groupByCategory(auto&& records) {
        std::map<std::string, std::vector<Record>> groups;

        for (const auto& r : records) {
            groups[r.category].push_back(r);
        }

        return groups;
    }

    // 使用chunk处理批量数据
    static void processBatches(auto&& records, size_t batchSize) {
        for (auto batch : records | std::views::chunk(batchSize)) {
            std::println("Processing batch of {} records", std::ranges::size(batch));

            for (const auto& r : batch) {
                std::println("  {} - {} - {:.2f}", r.id, r.name, r.value);
            }
        }
    }

    // 滑动窗口计算移动平均
    static auto movingAverage(auto&& values, size_t windowSize) {
        return values
            | std::views::slide(windowSize)
            | std::views::transform([windowSize](auto window) {
                double sum = 0.0;
                for (double v : window) sum += v;
                return sum / windowSize;
            });
    }
};

// 使用zip处理关联数据
void processRelatedData() {
    std::vector<int> ids = {1, 2, 3, 4, 5};
    std::vector<std::string> names = {"A", "B", "C", "D", "E"};
    std::vector<double> scores = {85.5, 92.0, 78.5, 95.0, 88.0};

    std::println("Student Records:");
    for (auto [id, name, score] : std::views::zip(ids, names, scores)) {
        std::println("  ID: {}, Name: {}, Score: {:.1f}", id, name, score);
    }

    // 计算加权平均
    std::vector<double> weights = {0.1, 0.2, 0.2, 0.3, 0.2};

    double weightedAvg = 0.0;
    for (auto [score, weight] : std::views::zip(scores, weights)) {
        weightedAvg += score * weight;
    }
    std::println("Weighted Average: {:.2f}", weightedAvg);
}

} // namespace app
```

#### 3. 矩阵库使用mdspan (matrix.hpp)

```cpp
#pragma once

#include <mdspan>
#include <vector>
#include <span>
#include <print>
#include <cmath>

namespace app::math {

// 使用mdspan的矩阵视图
template<typename T>
using MatrixView = std::mdspan<T, std::dextents<size_t, 2>>;

template<typename T>
using ConstMatrixView = std::mdspan<const T, std::dextents<size_t, 2>>;

// 矩阵类（拥有数据）
template<typename T>
class Matrix {
    std::vector<T> data_;
    size_t rows_, cols_;

public:
    Matrix(size_t rows, size_t cols)
        : data_(rows * cols), rows_(rows), cols_(cols) {}

    Matrix(size_t rows, size_t cols, T initialValue)
        : data_(rows * cols, initialValue), rows_(rows), cols_(cols) {}

    // 获取视图
    MatrixView<T> view() {
        return MatrixView<T>(data_.data(), rows_, cols_);
    }

    ConstMatrixView<T> view() const {
        return ConstMatrixView<T>(data_.data(), rows_, cols_);
    }

    // 多维下标（C++23）
    T& operator[](size_t i, size_t j) {
        return data_[i * cols_ + j];
    }

    const T& operator[](size_t i, size_t j) const {
        return data_[i * cols_ + j];
    }

    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }
    T* data() { return data_.data(); }
    const T* data() const { return data_.data(); }
};

// 矩阵运算（使用mdspan）
template<typename T>
void matmul(ConstMatrixView<T> A, ConstMatrixView<T> B, MatrixView<T> C) {
    const size_t M = A.extent(0);
    const size_t N = B.extent(1);
    const size_t K = A.extent(1);

    for (size_t i = 0; i < M; ++i) {
        for (size_t j = 0; j < N; ++j) {
            T sum = 0;
            for (size_t k = 0; k < K; ++k) {
                sum += A[i, k] * B[k, j];
            }
            C[i, j] = sum;
        }
    }
}

template<typename T>
void transpose(ConstMatrixView<T> A, MatrixView<T> B) {
    for (size_t i = 0; i < A.extent(0); ++i) {
        for (size_t j = 0; j < A.extent(1); ++j) {
            B[j, i] = A[i, j];
        }
    }
}

template<typename T>
T frobeniusNorm(ConstMatrixView<T> A) {
    T sum = 0;
    for (size_t i = 0; i < A.extent(0); ++i) {
        for (size_t j = 0; j < A.extent(1); ++j) {
            sum += A[i, j] * A[i, j];
        }
    }
    return std::sqrt(sum);
}

// 打印矩阵
template<typename T>
void print(ConstMatrixView<T> A, std::string_view name = "") {
    if (!name.empty()) {
        std::println("{}:", name);
    }

    for (size_t i = 0; i < A.extent(0); ++i) {
        std::print("  [");
        for (size_t j = 0; j < A.extent(1); ++j) {
            std::print(" {:8.3f}", A[i, j]);
        }
        std::println(" ]");
    }
}

// 子矩阵视图
template<typename T>
auto submatrix(MatrixView<T> A,
               size_t rowStart, size_t rowEnd,
               size_t colStart, size_t colEnd) {
    return std::submdspan(
        A,
        std::tuple{rowStart, rowEnd},
        std::tuple{colStart, colEnd}
    );
}

} // namespace app::math
```

#### 4. 服务层重构 (service.hpp)

```cpp
#pragma once

#include "result.hpp"
#include <string>
#include <vector>
#include <optional>
#include <print>
#include <map>

namespace app {

// 用户实体
struct User {
    int id;
    std::string name;
    std::string email;
    bool active;
};

// 用户仓库接口
class UserRepository {
    std::map<int, User> users_;

public:
    UserRepository() {
        users_[1] = {1, "Alice", "alice@example.com", true};
        users_[2] = {2, "Bob", "bob@example.com", true};
        users_[3] = {3, "Charlie", "charlie@example.com", false};
    }

    Result<User> findById(int id) const {
        auto it = users_.find(id);
        if (it == users_.end()) {
            return Err(Error::notFound("User"));
        }
        return it->second;
    }

    Result<User> findByEmail(const std::string& email) const {
        for (const auto& [id, user] : users_) {
            if (user.email == email) {
                return user;
            }
        }
        return Err(Error::notFound("User"));
    }

    Result<void> save(const User& user) {
        users_[user.id] = user;
        return {};
    }

    std::vector<User> findAll() const {
        std::vector<User> result;
        for (const auto& [id, user] : users_) {
            result.push_back(user);
        }
        return result;
    }
};

// 邮件服务
class EmailService {
public:
    Result<void> sendEmail(const std::string& to,
                          const std::string& subject,
                          const std::string& body) {
        // 模拟发送邮件
        std::println("Sending email to {}: {}", to, subject);
        return {};
    }
};

// 用户服务 - 使用单子操作
class UserService {
    UserRepository& userRepo_;
    EmailService& emailService_;

public:
    UserService(UserRepository& userRepo, EmailService& emailService)
        : userRepo_(userRepo), emailService_(emailService) {}

    // 传统方式
    Result<void> activateUserOld(int userId) {
        auto userResult = userRepo_.findById(userId);
        if (!userResult) {
            return Err(userResult.error());
        }

        User user = *userResult;
        if (user.active) {
            return Err(Error::invalid("User already active"));
        }

        user.active = true;
        auto saveResult = userRepo_.save(user);
        if (!saveResult) {
            return Err(saveResult.error());
        }

        auto emailResult = emailService_.sendEmail(
            user.email,
            "Account Activated",
            "Your account has been activated!"
        );
        if (!emailResult) {
            return Err(emailResult.error());
        }

        return {};
    }

    // 使用C++23单子操作
    Result<void> activateUser(int userId) {
        return userRepo_.findById(userId)
            .and_then([](User user) -> Result<User> {
                if (user.active) {
                    return Err(Error::invalid("User already active"));
                }
                user.active = true;
                return user;
            })
            .and_then([this](User user) -> Result<User> {
                return userRepo_.save(user)
                    .transform([user]() { return user; });
            })
            .and_then([this](const User& user) -> Result<void> {
                return emailService_.sendEmail(
                    user.email,
                    "Account Activated",
                    "Your account has been activated!"
                );
            });
    }

    // 获取用户信息（带默认值）
    std::string getUserDisplayName(int userId) {
        return userRepo_.findById(userId)
            .transform([](const User& u) { return u.name; })
            .value_or("Unknown User");
    }

    // 批量处理
    void sendNewsletterToActive() {
        auto users = userRepo_.findAll();

        auto activeUsers = users
            | std::views::filter([](const User& u) { return u.active; });

        for (const auto& user : activeUsers) {
            auto result = emailService_.sendEmail(
                user.email,
                "Newsletter",
                "Here's your weekly newsletter!"
            );

            if (!result) {
                std::println(stderr, "Failed to send to {}: {}",
                           user.email, result.error().message);
            }
        }
    }
};

} // namespace app
```

#### 5. 主程序 (main.cpp)

```cpp
#include "result.hpp"
#include "pipeline.hpp"
#include "matrix.hpp"
#include "service.hpp"
#include <print>

void demonstrateExpected() {
    std::println("=== std::expected Demo ===\n");

    app::UserRepository repo;
    app::EmailService emailService;
    app::UserService service(repo, emailService);

    // 成功案例
    auto displayName = service.getUserDisplayName(1);
    std::println("User 1: {}", displayName);

    // 失败案例
    displayName = service.getUserDisplayName(999);
    std::println("User 999: {}", displayName);

    // 激活用户
    auto result = service.activateUser(3);  // Charlie is inactive
    if (result) {
        std::println("User activated successfully");
    } else {
        std::println("Failed: {}", result.error().message);
    }

    // 尝试再次激活（应该失败）
    result = service.activateUser(3);
    if (!result) {
        std::println("Expected failure: {}", result.error().message);
    }
}

void demonstrateRanges() {
    std::println("\n=== Ranges Demo ===\n");

    // 数据处理
    auto records = app::readRecords("");

    std::println("All records:");
    for (const auto& r : records) {
        std::println("  {} - {} - {:.2f} - {}",
                   r.id, r.name, r.value, r.category);
    }

    // 批处理
    std::println("\nProcessing in batches:");
    app::DataPipeline::processBatches(app::readRecords(""), 2);

    // 关联数据处理
    std::println("\nRelated data:");
    app::processRelatedData();

    // 滑动窗口
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    std::println("\nMoving average (window=3):");
    for (double avg : app::DataPipeline::movingAverage(values, 3)) {
        std::print("{:.2f} ", avg);
    }
    std::println("");
}

void demonstrateMdspan() {
    std::println("\n=== mdspan Demo ===\n");

    using namespace app::math;

    // 创建矩阵
    Matrix<double> A(3, 3);
    Matrix<double> B(3, 3);
    Matrix<double> C(3, 3);

    // 初始化 A 为单位矩阵
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            A[i, j] = (i == j) ? 1.0 : 0.0;
        }
    }

    // 初始化 B
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            B[i, j] = i * 3 + j + 1;
        }
    }

    print(A.view(), "Matrix A (Identity)");
    print(B.view(), "Matrix B");

    // 矩阵乘法
    matmul(A.view(), B.view(), C.view());
    print(C.view(), "C = A * B");

    // Frobenius范数
    std::println("Frobenius norm of B: {:.3f}", frobeniusNorm(B.view()));
}

void demonstratePrint() {
    std::println("\n=== std::print Demo ===\n");

    // 基本格式化
    std::println("Integer: {}, Float: {:.2f}, String: {}",
               42, 3.14159, "hello");

    // 对齐和填充
    std::println("Right: {:>10}", 42);
    std::println("Left:  {:<10}", 42);
    std::println("Center:{:^10}", 42);
    std::println("Fill:  {:*>10}", 42);

    // 数值格式
    int n = 255;
    std::println("Decimal: {}, Hex: {:x}, Binary: {:b}", n, n, n);

    // 浮点格式
    double x = 12345.6789;
    std::println("Default: {}", x);
    std::println("Fixed: {:.2f}", x);
    std::println("Scientific: {:.2e}", x);
    std::println("General: {:.4g}", x);

    // 布尔值
    std::println("Bool: {}, {}", true, false);
}

int main() {
    demonstrateExpected();
    demonstrateRanges();
    demonstrateMdspan();
    demonstratePrint();

    std::println("\n=== All demos completed ===");
    return 0;
}
```

---

## 检验标准

### 知识检验
1. [ ] 能够解释std::expected的设计和用途
2. [ ] 理解单子操作（and_then/transform等）的含义
3. [ ] 掌握mdspan的布局策略
4. [ ] 了解C++23 ranges新增的视图适配器
5. [ ] 理解deducing this的应用场景

### 实践检验
1. [ ] 配置支持C++23的编译环境
2. [ ] 使用std::expected重构错误处理代码
3. [ ] 使用mdspan实现矩阵运算
4. [ ] 使用新ranges特性处理数据
5. [ ] 使用std::print替代iostream

### 代码质量
1. [ ] 代码符合C++23最佳实践
2. [ ] 错误处理完善且一致
3. [ ] 代码可读性好
4. [ ] 有完整的测试覆盖

---

## 输出物清单

1. **学习笔记**
   - [ ] C++23新特性总结
   - [ ] 单子操作详解
   - [ ] ranges库完整参考

2. **代码产出**
   - [ ] 重构后的代码库
   - [ ] 工具库（Result, Pipeline等）
   - [ ] 示例程序

3. **文档产出**
   - [ ] C++23迁移指南
   - [ ] 新特性最佳实践
   - [ ] 性能对比分析

---

## 时间分配表

| 周次 | 理论学习 | 源码阅读 | 项目实践 | 总计 |
|------|----------|----------|----------|------|
| Week 1 | 15h | 8h | 12h | 35h |
| Week 2 | 12h | 10h | 13h | 35h |
| Week 3 | 12h | 8h | 15h | 35h |
| Week 4 | 6h | 6h | 23h | 35h |
| **总计** | **45h** | **32h** | **63h** | **140h** |

---

## 下月预告

**Month 57: C++26展望（Reflection/Contracts）**

下个月将预览C++26的重要特性：
- 静态反射（编译期反射）
- Contracts（契约编程）
- 模式匹配
- 其他提案预览
- 实践项目：使用反射实现序列化

建议提前：
1. 了解现有反射库（如Boost.Hana）
2. 学习契约编程概念
3. 关注C++26提案状态
