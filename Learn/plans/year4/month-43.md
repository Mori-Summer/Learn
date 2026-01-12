# Month 43: Clang-Tidy静态分析——代码质量的守护者

## 本月主题概述

本月深入学习Clang-Tidy，这是LLVM项目提供的强大静态分析工具。学习如何配置和使用各类检查器，实现代码风格统一、潜在Bug检测、性能优化建议，以及现代C++迁移。

**学习目标**：
- 掌握Clang-Tidy的安装、配置和使用
- 理解各类检查器的作用和使用场景
- 学会创建自定义检查规则
- 将静态分析集成到开发流程中

---

## 理论学习内容

### 第一周：Clang-Tidy基础

**学习目标**：安装和基本使用Clang-Tidy

**阅读材料**：
- [ ] Clang-Tidy官方文档
- [ ] LLVM Coding Standards
- [ ] Extra Clang Tools文档

**核心概念**：

```bash
# ==========================================
# 安装Clang-Tidy
# ==========================================

# Ubuntu/Debian
sudo apt-get install clang-tidy

# 指定版本
sudo apt-get install clang-tidy-15

# macOS
brew install llvm
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

# Windows (通过LLVM安装包)
# 下载: https://releases.llvm.org/

# 验证安装
clang-tidy --version

# ==========================================
# 基本使用
# ==========================================

# 分析单个文件
clang-tidy source.cpp -- -std=c++17 -I/path/to/include

# 使用编译数据库
clang-tidy -p build source.cpp

# 分析多个文件
clang-tidy -p build src/*.cpp

# 使用run-clang-tidy脚本（并行）
run-clang-tidy -p build

# 自动修复
clang-tidy -p build -fix source.cpp

# 导出修复建议
clang-tidy -p build -export-fixes=fixes.yaml source.cpp
clang-apply-replacements fixes.yaml
```

**生成编译数据库**：

```bash
# ==========================================
# compile_commands.json生成
# ==========================================

# CMake方式（推荐）
cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 或在CMakeLists.txt中设置
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Bear工具（包装make）
bear -- make

# Ninja
ninja -t compdb > compile_commands.json

# 符号链接到源码目录（方便IDE使用）
ln -s build/compile_commands.json .
```

**检查器分类**：

```bash
# ==========================================
# 检查器类别
# ==========================================

# 列出所有检查器
clang-tidy --list-checks -checks='*'

# 主要类别：
# bugprone-*      - 潜在Bug检测
# cert-*          - CERT编码标准
# clang-analyzer-* - Clang静态分析器
# cppcoreguidelines-* - C++ Core Guidelines
# google-*        - Google编码规范
# hicpp-*         - High Integrity C++
# llvm-*          - LLVM编码规范
# misc-*          - 杂项检查
# modernize-*     - 现代C++迁移
# performance-*   - 性能优化
# portability-*   - 可移植性
# readability-*   - 可读性

# 查看检查器详情
clang-tidy --explain-check=modernize-use-nullptr
```

### 第二周：.clang-tidy配置

**学习目标**：配置项目级别的检查规则

**阅读材料**：
- [ ] Clang-Tidy Configuration
- [ ] 各检查器的详细文档

```yaml
# ==========================================
# .clang-tidy - 完整配置示例
# ==========================================

# 检查器配置
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  cert-*,
  -cert-err58-cpp,
  clang-analyzer-*,
  cppcoreguidelines-*,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-owning-memory,
  -cppcoreguidelines-pro-bounds-array-to-pointer-decay,
  google-*,
  -google-build-using-namespace,
  -google-readability-todo,
  hicpp-*,
  -hicpp-no-array-decay,
  misc-*,
  -misc-non-private-member-variables-in-classes,
  modernize-*,
  -modernize-use-trailing-return-type,
  performance-*,
  portability-*,
  readability-*,
  -readability-magic-numbers,
  -readability-identifier-length

# 警告当作错误
WarningsAsErrors: ''

# 头文件过滤（正则表达式）
HeaderFilterRegex: '.*'

# 分析系统头文件
AnalyzeTemporaryDtors: false
FormatStyle: file

# 检查器选项
CheckOptions:
  # readability
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.ClassMemberCase
    value: lower_case
  - key: readability-identifier-naming.ClassMemberSuffix
    value: '_'
  - key: readability-identifier-naming.ConstantCase
    value: UPPER_CASE
  - key: readability-identifier-naming.EnumCase
    value: CamelCase
  - key: readability-identifier-naming.EnumConstantCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.GlobalConstantCase
    value: UPPER_CASE
  - key: readability-identifier-naming.GlobalConstantPrefix
    value: 'k'
  - key: readability-identifier-naming.LocalVariableCase
    value: lower_case
  - key: readability-identifier-naming.MemberCase
    value: lower_case
  - key: readability-identifier-naming.MethodCase
    value: lower_case
  - key: readability-identifier-naming.NamespaceCase
    value: lower_case
  - key: readability-identifier-naming.ParameterCase
    value: lower_case
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: '_'
  - key: readability-identifier-naming.StructCase
    value: CamelCase
  - key: readability-identifier-naming.TypedefCase
    value: CamelCase
  - key: readability-identifier-naming.VariableCase
    value: lower_case

  # modernize
  - key: modernize-use-auto.MinTypeNameLength
    value: '5'
  - key: modernize-use-auto.RemoveStars
    value: 'false'
  - key: modernize-loop-convert.MinConfidence
    value: 'reasonable'
  - key: modernize-pass-by-value.IncludeStyle
    value: 'llvm'

  # performance
  - key: performance-move-const-arg.CheckTriviallyCopyableMove
    value: 'true'
  - key: performance-unnecessary-value-param.AllowedTypes
    value: 'std::function.*'

  # misc
  - key: misc-non-private-member-variables-in-classes.IgnoreClassesWithAllMemberVariablesBeingPublic
    value: 'true'

  # cppcoreguidelines
  - key: cppcoreguidelines-special-member-functions.AllowSoleDefaultDtor
    value: 'true'
  - key: cppcoreguidelines-special-member-functions.AllowMissingMoveFunctions
    value: 'true'

  # bugprone
  - key: bugprone-argument-comment.StrictMode
    value: 'true'
  - key: bugprone-assert-side-effect.AssertMacros
    value: 'assert,ASSERT'

  # readability-function-cognitive-complexity
  - key: readability-function-cognitive-complexity.Threshold
    value: '25'
  - key: readability-function-cognitive-complexity.DescribeBasicIncrements
    value: 'false'

  # readability-function-size
  - key: readability-function-size.LineThreshold
    value: '100'
  - key: readability-function-size.StatementThreshold
    value: '50'
  - key: readability-function-size.BranchThreshold
    value: '10'
  - key: readability-function-size.ParameterThreshold
    value: '6'

# 使用颜色输出
UseColor: true
```

### 第三周：重要检查器详解

**学习目标**：深入理解常用检查器

**阅读材料**：
- [ ] Bugprone Checks
- [ ] Modernize Checks
- [ ] Performance Checks

```cpp
// ==========================================
// bugprone-* 示例
// ==========================================

// bugprone-argument-comment
void process(bool enable, bool verbose);
// 错误：参数顺序可能混淆
process(true, false);
// 建议：使用参数注释
process(/*enable=*/true, /*verbose=*/false);

// bugprone-branch-clone
if (condition) {
    doA();
} else {
    doA();  // 警告：分支内容相同
}

// bugprone-copy-constructor-init
class Derived : public Base {
    Derived(const Derived& other)
        // 警告：应该调用基类拷贝构造
        : Base() {}
};

// bugprone-dangling-handle
std::string_view sv = std::string("temp");  // 警告：悬垂引用

// bugprone-exception-escape
void func() noexcept {
    throw std::runtime_error("oops");  // 警告：异常逃逸
}

// bugprone-forwarding-reference-overload
class Widget {
public:
    template<typename T>
    Widget(T&& param) {}  // 警告：可能遮蔽拷贝构造

    Widget(const Widget&) = default;
};

// bugprone-infinite-loop
while (true) {  // 警告：可能的无限循环
    if (condition) break;
}

// bugprone-integer-division
double result = 5 / 3;  // 警告：整数除法

// bugprone-move-forwarding-reference
template<typename T>
void func(T&& param) {
    auto x = std::move(param);  // 警告：应使用std::forward
}

// bugprone-narrowing-conversions
void func(long value) {
    int x = value;  // 警告：窄化转换
}

// bugprone-sizeof-expression
int arr[10];
int count = sizeof(arr);  // 警告：可能想用sizeof(arr)/sizeof(arr[0])

// bugprone-string-constructor
std::string s('x', 10);   // 警告：参数顺序错误
std::string s(10, 'x');   // 正确

// bugprone-suspicious-semicolon
if (condition);  // 警告：可疑的分号
{
    doSomething();
}

// bugprone-too-small-loop-variable
std::vector<int> v(100000);
for (short i = 0; i < v.size(); ++i) {}  // 警告：循环变量太小

// bugprone-use-after-move
auto widget = std::make_unique<Widget>();
auto other = std::move(widget);
widget->doSomething();  // 警告：移动后使用
```

```cpp
// ==========================================
// modernize-* 示例
// ==========================================

// modernize-avoid-bind
auto f = std::bind(&Foo::bar, this, _1, _2);
// 建议：
auto f = [this](auto a, auto b) { return bar(a, b); };

// modernize-avoid-c-arrays
int arr[10];  // 警告
// 建议：
std::array<int, 10> arr;

// modernize-concat-nested-namespaces（C++17）
namespace A { namespace B { namespace C {
}}}
// 建议：
namespace A::B::C {
}

// modernize-deprecated-headers
#include <stdio.h>   // 警告
#include <cstdio>    // 建议

// modernize-loop-convert
for (std::vector<int>::iterator it = v.begin(); it != v.end(); ++it) {
    *it *= 2;
}
// 建议：
for (auto& elem : v) {
    elem *= 2;
}

// modernize-make-shared
std::shared_ptr<Foo> p(new Foo());
// 建议：
auto p = std::make_shared<Foo>();

// modernize-make-unique
std::unique_ptr<Foo> p(new Foo());
// 建议：
auto p = std::make_unique<Foo>();

// modernize-pass-by-value
void setName(const std::string& name) {
    name_ = name;
}
// 建议（当需要复制时）：
void setName(std::string name) {
    name_ = std::move(name);
}

// modernize-raw-string-literal
std::string path = "C:\\Users\\name";
// 建议：
std::string path = R"(C:\Users\name)";

// modernize-redundant-void-arg
void func(void);
// 建议：
void func();

// modernize-replace-auto-ptr
std::auto_ptr<Foo> p;  // 警告：已废弃
// 建议：
std::unique_ptr<Foo> p;

// modernize-return-braced-init-list
Foo createFoo() {
    return Foo(1, 2, 3);
}
// 建议：
Foo createFoo() {
    return {1, 2, 3};
}

// modernize-shrink-to-fit
v.erase(v.begin(), v.end());
std::vector<int>(v).swap(v);
// 建议：
v.clear();
v.shrink_to_fit();

// modernize-use-auto
std::map<std::string, std::vector<int>>::iterator it = m.begin();
// 建议：
auto it = m.begin();

// modernize-use-bool-literals
int x = (condition) ? 1 : 0;  // 当类型是bool时警告
// 建议：
bool x = condition;

// modernize-use-default-member-init
class Foo {
    int x;
    Foo() : x(0) {}
};
// 建议：
class Foo {
    int x = 0;
    Foo() = default;
};

// modernize-use-emplace
v.push_back(Foo(1, 2));
// 建议：
v.emplace_back(1, 2);

// modernize-use-equals-default
Foo() {}
~Foo() {}
// 建议：
Foo() = default;
~Foo() = default;

// modernize-use-equals-delete
private:
    Foo(const Foo&);  // 阻止拷贝
// 建议：
public:
    Foo(const Foo&) = delete;

// modernize-use-nodiscard（C++17）
bool isEmpty() const { return size_ == 0; }
// 建议：
[[nodiscard]] bool isEmpty() const { return size_ == 0; }

// modernize-use-noexcept
void func() throw() {}
// 建议：
void func() noexcept {}

// modernize-use-nullptr
int* p = 0;
int* q = NULL;
// 建议：
int* p = nullptr;

// modernize-use-override
class Derived : public Base {
    void func() {}  // 重写但没有override
};
// 建议：
class Derived : public Base {
    void func() override {}
};

// modernize-use-transparent-functors
std::set<std::string, std::less<std::string>> s;
// 建议（C++14）：
std::set<std::string, std::less<>> s;

// modernize-use-using
typedef int MyInt;
// 建议：
using MyInt = int;
```

```cpp
// ==========================================
// performance-* 示例
// ==========================================

// performance-faster-string-find
str.find("a") != std::string::npos;  // 单字符用char更快
// 建议：
str.find('a') != std::string::npos;

// performance-for-range-copy
for (const auto item : container) {  // 复制每个元素
    use(item);
}
// 建议：
for (const auto& item : container) {
    use(item);
}

// performance-implicit-conversion-in-loop
for (auto elem : map) {  // pair被隐式转换
}
// 建议：
for (const auto& [key, value] : map) {
}

// performance-inefficient-algorithm
auto it = std::find(s.begin(), s.end(), value);
// 建议（对于关联容器）：
auto it = s.find(value);

// performance-inefficient-string-concatenation
std::string result = a + b + c + d;  // 多次分配
// 建议：
std::string result;
result.reserve(a.size() + b.size() + c.size() + d.size());
result += a;
result += b;
result += c;
result += d;

// performance-inefficient-vector-operation
std::vector<int> v;
for (int i = 0; i < n; ++i) {
    v.push_back(i);  // 多次重新分配
}
// 建议：
std::vector<int> v;
v.reserve(n);
for (int i = 0; i < n; ++i) {
    v.push_back(i);
}

// performance-move-const-arg
const std::string s = "hello";
func(std::move(s));  // move对const无效
// 警告：std::move对const参数无效

// performance-move-constructor-init
class Foo {
    std::string str_;
    Foo(Foo&& other) : str_(other.str_) {}  // 应该move
};
// 建议：
Foo(Foo&& other) : str_(std::move(other.str_)) {}

// performance-no-automatic-move
std::string func() {
    std::string s = "hello";
    return std::move(s);  // 阻止NRVO优化
}
// 建议：
std::string func() {
    std::string s = "hello";
    return s;  // 编译器会自动move
}

// performance-noexcept-move-constructor
class Foo {
    Foo(Foo&&) {}  // 缺少noexcept
};
// 建议：
class Foo {
    Foo(Foo&&) noexcept {}
};

// performance-trivially-destructible
class Simple {
    int x;
    ~Simple() {}  // 空析构函数阻止平凡析构
};
// 建议：
class Simple {
    int x;
    ~Simple() = default;
};

// performance-type-promotion-in-math-fn
float x = 1.5f;
double y = std::sin(x);  // float提升为double
// 建议：
float y = std::sin(x);  // 使用结果类型匹配

// performance-unnecessary-copy-initialization
auto copy = container.at(index);  // 不必要的拷贝
// 建议：
const auto& ref = container.at(index);

// performance-unnecessary-value-param
void func(std::string s) {  // 大对象按值传递
    // 只读取s，不修改
}
// 建议：
void func(const std::string& s) {
}
```

### 第四周：CMake集成与自定义检查

**学习目标**：将Clang-Tidy集成到构建系统

**阅读材料**：
- [ ] CMake: CMAKE_CXX_CLANG_TIDY
- [ ] Writing Clang-Tidy Checks

```cmake
# ==========================================
# CMakeLists.txt - Clang-Tidy集成
# ==========================================
cmake_minimum_required(VERSION 3.16)
project(MyProject)

# 选项
option(ENABLE_CLANG_TIDY "Enable clang-tidy analysis" ON)
option(CLANG_TIDY_FIX "Apply clang-tidy fixes" OFF)

# 查找clang-tidy
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy clang-tidy-15 clang-tidy-14
        DOC "Path to clang-tidy executable"
    )

    if(CLANG_TIDY_EXE)
        message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")

        # 基本配置
        set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")

        # 添加修复选项
        if(CLANG_TIDY_FIX)
            list(APPEND CLANG_TIDY_COMMAND "-fix")
        endif()

        # 设置全局属性（影响所有目标）
        set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_COMMAND})

        # 或者只对特定目标启用
        # set_target_properties(mytarget PROPERTIES
        #     CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}"
        # )
    else()
        message(WARNING "clang-tidy not found, static analysis disabled")
    endif()
endif()

# 添加库/可执行文件
add_library(mylib src/mylib.cpp)

# 为特定目标配置不同的检查
set_target_properties(mylib PROPERTIES
    CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=-*,modernize-*,performance-*"
)

# 排除某些目标
add_executable(tests tests/main.cpp)
set_target_properties(tests PROPERTIES
    CXX_CLANG_TIDY ""  # 禁用
)

# ==========================================
# 自定义目标：手动运行clang-tidy
# ==========================================
if(CLANG_TIDY_EXE)
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            ${CMAKE_SOURCE_DIR}/src/*.cpp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy..."
    )

    add_custom_target(clang-tidy-fix
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            -fix
            ${CMAKE_SOURCE_DIR}/src/*.cpp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy with fixes..."
    )
endif()
```

```cmake
# ==========================================
# cmake/ClangTidy.cmake - 可复用模块
# ==========================================
function(enable_clang_tidy target)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)

    if(NOT CLANG_TIDY_EXE)
        message(WARNING "clang-tidy not found")
        return()
    endif()

    # 解析参数
    set(options FIX)
    set(oneValueArgs CONFIG)
    set(multiValueArgs CHECKS EXTRA_ARGS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # 构建命令
    set(CLANG_TIDY_CMD "${CLANG_TIDY_EXE}")

    if(ARG_CHECKS)
        string(REPLACE ";" "," CHECKS_STR "${ARG_CHECKS}")
        list(APPEND CLANG_TIDY_CMD "-checks=${CHECKS_STR}")
    endif()

    if(ARG_CONFIG)
        list(APPEND CLANG_TIDY_CMD "--config-file=${ARG_CONFIG}")
    endif()

    if(ARG_FIX)
        list(APPEND CLANG_TIDY_CMD "-fix")
    endif()

    if(ARG_EXTRA_ARGS)
        list(APPEND CLANG_TIDY_CMD ${ARG_EXTRA_ARGS})
    endif()

    set_target_properties(${target} PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_CMD}"
    )
endfunction()

# 使用示例
# enable_clang_tidy(mylib
#     CHECKS modernize-* performance-*
#     CONFIG ${CMAKE_SOURCE_DIR}/.clang-tidy
# )
```

**自定义抑制警告**：

```cpp
// ==========================================
// 抑制警告的方法
// ==========================================

// 方法1：NOLINT注释
void func() {
    int x = 0;  // NOLINT
    int y = 0;  // NOLINT(clang-analyzer-deadcode.DeadStores)
    int z = 0;  // NOLINT(modernize-*, performance-*)
}

// 方法2：NOLINTNEXTLINE
// NOLINTNEXTLINE(bugprone-branch-clone)
if (condition) {
    doA();
} else {
    doA();
}

// 方法3：区域禁用
// NOLINTBEGIN(modernize-use-nullptr)
int* p = 0;
int* q = NULL;
// NOLINTEND(modernize-use-nullptr)

// 方法4：文件级别禁用
// 在文件开头添加
// NOLINTBEGIN(*)
// ... entire file ...
// NOLINTEND(*)

// 方法5：使用宏封装
#define SUPPRESS_WARNING(check) // NOLINT(check)
```

---

## 源码阅读任务

### 本月源码阅读

1. **Clang-Tidy源码**
   - 仓库：https://github.com/llvm/llvm-project
   - 路径：`clang-tools-extra/clang-tidy/`
   - 重点：`modernize/UseNullptrCheck.cpp`

2. **知名项目的配置**
   - LLVM项目的.clang-tidy
   - Chromium的clang-tidy配置

3. **检查器实现**
   - 理解AST匹配器
   - 理解诊断报告

---

## 实践项目

### 项目：代码质量检查集成工具

创建一个集成Clang-Tidy的代码质量检查工具。

**项目结构**：

```
code-quality/
├── CMakeLists.txt
├── .clang-tidy
├── cmake/
│   └── ClangTidy.cmake
├── include/
│   └── quality/
│       ├── analyzer.hpp
│       └── report.hpp
├── src/
│   ├── analyzer.cpp
│   └── report.cpp
├── tools/
│   └── check_quality.cpp
└── scripts/
    ├── run-clang-tidy.sh
    └── generate-report.py
```

**.clang-tidy**（项目配置）：

```yaml
Checks: >
  -*,
  bugprone-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic,
  -cppcoreguidelines-pro-type-reinterpret-cast,
  google-*,
  -google-build-using-namespace,
  -google-readability-todo,
  misc-*,
  modernize-*,
  -modernize-use-trailing-return-type,
  performance-*,
  readability-*,
  -readability-magic-numbers,
  -readability-identifier-length

WarningsAsErrors: >
  bugprone-use-after-move,
  bugprone-dangling-handle,
  modernize-use-nullptr,
  modernize-use-override

HeaderFilterRegex: '.*'

CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: camelBack
  - key: readability-identifier-naming.VariableCase
    value: camelBack
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: '_'
  - key: readability-function-cognitive-complexity.Threshold
    value: '20'
  - key: modernize-use-auto.MinTypeNameLength
    value: '5'
```

**include/quality/analyzer.hpp**：

```cpp
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <functional>

namespace quality {

/**
 * @brief 诊断信息
 */
struct Diagnostic {
    enum class Severity {
        Warning,
        Error,
        Note
    };

    std::string file;
    int line;
    int column;
    Severity severity;
    std::string check_name;
    std::string message;
    std::optional<std::string> fix;

    [[nodiscard]] std::string to_string() const;
};

/**
 * @brief 分析结果
 */
struct AnalysisResult {
    std::vector<Diagnostic> diagnostics;
    int warnings_count = 0;
    int errors_count = 0;
    double analysis_time_seconds = 0.0;

    [[nodiscard]] bool has_errors() const { return errors_count > 0; }
    [[nodiscard]] bool is_clean() const { return diagnostics.empty(); }
};

/**
 * @brief 分析器配置
 */
struct AnalyzerConfig {
    std::filesystem::path compile_commands_path;
    std::filesystem::path config_file;
    std::vector<std::string> checks;
    std::vector<std::string> warnings_as_errors;
    bool apply_fixes = false;
    int jobs = 0;  // 0表示自动检测
    std::string header_filter;
};

/**
 * @brief 代码分析器
 */
class Analyzer {
public:
    using ProgressCallback = std::function<void(const std::string& file, int current, int total)>;

    explicit Analyzer(AnalyzerConfig config);
    ~Analyzer();

    // 禁用拷贝
    Analyzer(const Analyzer&) = delete;
    Analyzer& operator=(const Analyzer&) = delete;

    /**
     * @brief 分析单个文件
     */
    AnalysisResult analyzeFile(const std::filesystem::path& file);

    /**
     * @brief 分析多个文件
     */
    AnalysisResult analyzeFiles(const std::vector<std::filesystem::path>& files);

    /**
     * @brief 分析整个项目
     */
    AnalysisResult analyzeProject();

    /**
     * @brief 设置进度回调
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief 获取可用的检查器列表
     */
    static std::vector<std::string> getAvailableChecks();

    /**
     * @brief 检查clang-tidy是否可用
     */
    static bool isAvailable();

    /**
     * @brief 获取clang-tidy版本
     */
    static std::string getVersion();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quality
```

**include/quality/report.hpp**：

```cpp
#pragma once

#include "analyzer.hpp"
#include <ostream>

namespace quality {

/**
 * @brief 报告格式
 */
enum class ReportFormat {
    Text,
    JSON,
    HTML,
    SARIF,  // Static Analysis Results Interchange Format
    JUnit   // 用于CI集成
};

/**
 * @brief 报告生成器
 */
class ReportGenerator {
public:
    virtual ~ReportGenerator() = default;

    virtual void generate(const AnalysisResult& result, std::ostream& out) = 0;

    static std::unique_ptr<ReportGenerator> create(ReportFormat format);
};

/**
 * @brief 文本报告生成器
 */
class TextReportGenerator : public ReportGenerator {
public:
    void generate(const AnalysisResult& result, std::ostream& out) override;
};

/**
 * @brief JSON报告生成器
 */
class JsonReportGenerator : public ReportGenerator {
public:
    void generate(const AnalysisResult& result, std::ostream& out) override;
};

/**
 * @brief HTML报告生成器
 */
class HtmlReportGenerator : public ReportGenerator {
public:
    void generate(const AnalysisResult& result, std::ostream& out) override;

    void setTitle(const std::string& title) { title_ = title; }
    void setStylesheet(const std::string& css) { stylesheet_ = css; }

private:
    std::string title_ = "Code Quality Report";
    std::string stylesheet_;
};

/**
 * @brief SARIF报告生成器（用于GitHub Code Scanning等）
 */
class SarifReportGenerator : public ReportGenerator {
public:
    void generate(const AnalysisResult& result, std::ostream& out) override;

    void setToolName(const std::string& name) { tool_name_ = name; }
    void setToolVersion(const std::string& version) { tool_version_ = version; }

private:
    std::string tool_name_ = "clang-tidy";
    std::string tool_version_;
};

} // namespace quality
```

**src/report.cpp**：

```cpp
#include "quality/report.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace quality {

using json = nlohmann::json;

std::unique_ptr<ReportGenerator> ReportGenerator::create(ReportFormat format) {
    switch (format) {
        case ReportFormat::Text:
            return std::make_unique<TextReportGenerator>();
        case ReportFormat::JSON:
            return std::make_unique<JsonReportGenerator>();
        case ReportFormat::HTML:
            return std::make_unique<HtmlReportGenerator>();
        case ReportFormat::SARIF:
            return std::make_unique<SarifReportGenerator>();
        default:
            return std::make_unique<TextReportGenerator>();
    }
}

void TextReportGenerator::generate(const AnalysisResult& result, std::ostream& out) {
    out << "=== Code Quality Report ===\n\n";
    out << "Summary:\n";
    out << "  Warnings: " << result.warnings_count << "\n";
    out << "  Errors: " << result.errors_count << "\n";
    out << "  Analysis time: " << std::fixed << std::setprecision(2)
        << result.analysis_time_seconds << "s\n\n";

    if (result.diagnostics.empty()) {
        out << "No issues found!\n";
        return;
    }

    out << "Issues:\n";
    for (const auto& diag : result.diagnostics) {
        out << diag.to_string() << "\n";
    }
}

void JsonReportGenerator::generate(const AnalysisResult& result, std::ostream& out) {
    json j;
    j["summary"] = {
        {"warnings", result.warnings_count},
        {"errors", result.errors_count},
        {"analysis_time_seconds", result.analysis_time_seconds}
    };

    j["diagnostics"] = json::array();
    for (const auto& diag : result.diagnostics) {
        json d;
        d["file"] = diag.file;
        d["line"] = diag.line;
        d["column"] = diag.column;
        d["severity"] = static_cast<int>(diag.severity);
        d["check"] = diag.check_name;
        d["message"] = diag.message;
        if (diag.fix) {
            d["fix"] = *diag.fix;
        }
        j["diagnostics"].push_back(d);
    }

    out << j.dump(2);
}

void HtmlReportGenerator::generate(const AnalysisResult& result, std::ostream& out) {
    out << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>)" << title_ << R"(</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 20px; }
        .summary { background: #f5f5f5; padding: 20px; border-radius: 8px; margin-bottom: 20px; }
        .diagnostic { border-left: 4px solid #ccc; padding: 10px; margin: 10px 0; }
        .warning { border-color: #ffa500; background: #fff8e1; }
        .error { border-color: #f44336; background: #ffebee; }
        .location { font-family: monospace; color: #666; }
        .check { color: #1976d2; font-size: 0.9em; }
        .message { margin-top: 5px; }
        .fix { background: #e8f5e9; padding: 10px; margin-top: 5px; font-family: monospace; }
    </style>
</head>
<body>
    <h1>)" << title_ << R"(</h1>
    <div class="summary">
        <h2>Summary</h2>
        <p>Warnings: )" << result.warnings_count << R"(</p>
        <p>Errors: )" << result.errors_count << R"(</p>
        <p>Analysis time: )" << std::fixed << std::setprecision(2)
                             << result.analysis_time_seconds << R"(s</p>
    </div>
    <h2>Diagnostics</h2>
)";

    for (const auto& diag : result.diagnostics) {
        std::string severity_class =
            diag.severity == Diagnostic::Severity::Error ? "error" : "warning";

        out << R"(    <div class="diagnostic )" << severity_class << R"(">
        <div class="location">)" << diag.file << ":" << diag.line
            << ":" << diag.column << R"(</div>
        <div class="check">)" << diag.check_name << R"(</div>
        <div class="message">)" << diag.message << R"(</div>)";

        if (diag.fix) {
            out << R"(
        <div class="fix"><strong>Fix:</strong> )" << *diag.fix << R"(</div>)";
        }

        out << R"(
    </div>
)";
    }

    out << R"(</body>
</html>)";
}

void SarifReportGenerator::generate(const AnalysisResult& result, std::ostream& out) {
    json sarif;
    sarif["$schema"] = "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json";
    sarif["version"] = "2.1.0";

    json run;
    run["tool"]["driver"]["name"] = tool_name_;
    if (!tool_version_.empty()) {
        run["tool"]["driver"]["version"] = tool_version_;
    }

    // Rules
    json rules = json::array();
    std::set<std::string> seen_checks;
    for (const auto& diag : result.diagnostics) {
        if (seen_checks.insert(diag.check_name).second) {
            json rule;
            rule["id"] = diag.check_name;
            rule["shortDescription"]["text"] = diag.check_name;
            rules.push_back(rule);
        }
    }
    run["tool"]["driver"]["rules"] = rules;

    // Results
    json results = json::array();
    for (const auto& diag : result.diagnostics) {
        json r;
        r["ruleId"] = diag.check_name;
        r["level"] = diag.severity == Diagnostic::Severity::Error ? "error" : "warning";
        r["message"]["text"] = diag.message;

        json location;
        location["physicalLocation"]["artifactLocation"]["uri"] = diag.file;
        location["physicalLocation"]["region"]["startLine"] = diag.line;
        location["physicalLocation"]["region"]["startColumn"] = diag.column;
        r["locations"] = json::array({location});

        if (diag.fix) {
            json fix;
            fix["description"]["text"] = *diag.fix;
            r["fixes"] = json::array({fix});
        }

        results.push_back(r);
    }
    run["results"] = results;

    sarif["runs"] = json::array({run});

    out << sarif.dump(2);
}

std::string Diagnostic::to_string() const {
    std::ostringstream oss;
    oss << file << ":" << line << ":" << column << ": ";

    switch (severity) {
        case Severity::Warning: oss << "warning: "; break;
        case Severity::Error: oss << "error: "; break;
        case Severity::Note: oss << "note: "; break;
    }

    oss << message << " [" << check_name << "]";

    if (fix) {
        oss << "\n  Fix: " << *fix;
    }

    return oss.str();
}

} // namespace quality
```

**scripts/run-clang-tidy.sh**：

```bash
#!/bin/bash
set -e

# 配置
BUILD_DIR="${BUILD_DIR:-build}"
JOBS="${JOBS:-$(nproc)}"
FIX="${FIX:-false}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Running clang-tidy analysis...${NC}"

# 检查编译数据库
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo -e "${YELLOW}Generating compile_commands.json...${NC}"
    cmake -B "$BUILD_DIR" -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

# 构建clang-tidy命令
CLANG_TIDY_CMD="run-clang-tidy"

if command -v run-clang-tidy-15 &> /dev/null; then
    CLANG_TIDY_CMD="run-clang-tidy-15"
fi

# 运行分析
if [ "$FIX" = "true" ]; then
    echo -e "${YELLOW}Applying fixes...${NC}"
    $CLANG_TIDY_CMD -p "$BUILD_DIR" -j "$JOBS" -fix
else
    $CLANG_TIDY_CMD -p "$BUILD_DIR" -j "$JOBS"
fi

echo -e "${GREEN}Analysis complete!${NC}"
```

---

## 检验标准

- [ ] 能够安装和配置Clang-Tidy
- [ ] 理解主要检查器类别和用途
- [ ] 能够编写.clang-tidy配置文件
- [ ] 能够将Clang-Tidy集成到CMake
- [ ] 能够生成和分析检查报告
- [ ] 理解如何抑制特定警告

### 知识检验问题

1. modernize-*和performance-*检查器的区别是什么？
2. 如何只对项目头文件运行检查？
3. CMAKE_CXX_CLANG_TIDY的工作原理是什么？
4. 如何创建自定义的clang-tidy检查？

---

## 输出物清单

1. **配置文件**
   - `.clang-tidy` - 项目配置
   - `cmake/ClangTidy.cmake` - CMake模块

2. **工具代码**
   - `code-quality/` - 分析工具项目

3. **文档**
   - `notes/month43_clang_tidy.md` - 学习笔记
   - `notes/clang_tidy_checks.md` - 检查器参考

4. **脚本**
   - `scripts/run-clang-tidy.sh`
   - `scripts/generate-report.py`

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | Clang-Tidy基础 | 15h | 15h | 5h |
| 第2周 | 配置文件编写 | 12h | 18h | 5h |
| 第3周 | 重要检查器 | 10h | 20h | 5h |
| 第4周 | CMake集成 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 44将学习**Sanitizers（ASan/TSan/UBSan）**，掌握运行时错误检测工具。
