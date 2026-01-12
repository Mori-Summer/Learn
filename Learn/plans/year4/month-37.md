# Month 37: Modern CMake深度使用——构建系统的现代化实践

## 本月主题概述

进入第四年的现代工程化学习。本月深入掌握Modern CMake（3.x版本）的最佳实践，从传统的变量驱动方式转向目标驱动的现代风格。学习如何编写可维护、可复用的CMake代码，为后续的包管理和CI/CD打下坚实基础。

**学习目标**：
- 掌握Modern CMake的核心概念（Target、Properties、Generator Expressions）
- 理解PUBLIC/PRIVATE/INTERFACE的依赖传播机制
- 学会编写可复用的CMake模块和函数
- 构建跨平台的专业级项目结构

---

## 理论学习内容

### 第一周：Modern CMake基础理念

**学习目标**：理解Modern CMake与传统CMake的区别

**阅读材料**：
- [ ] 《Professional CMake: A Practical Guide》第1-5章
- [ ] CMake官方教程 (cmake.org/cmake/help/latest/guide/tutorial)
- [ ] "Effective Modern CMake" by Daniel Pfeifer (YouTube演讲)

**核心概念**：

```cmake
# ==========================================
# 传统CMake（反模式，不推荐）
# ==========================================
cmake_minimum_required(VERSION 2.8)
project(OldStyle)

# 全局变量污染
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(/usr/local/lib)

add_executable(myapp main.cpp)
target_link_libraries(myapp boost_system pthread)

# ==========================================
# Modern CMake（推荐方式）
# ==========================================
cmake_minimum_required(VERSION 3.16)
project(ModernStyle
    VERSION 1.0.0
    DESCRIPTION "A modern CMake example"
    LANGUAGES CXX
)

# 设置C++标准（目标级别）
add_executable(myapp main.cpp)
target_compile_features(myapp PRIVATE cxx_std_17)
target_compile_options(myapp PRIVATE -Wall -Wextra)
target_include_directories(myapp PRIVATE ${PROJECT_SOURCE_DIR}/include)

# 使用imported targets
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
target_link_libraries(myapp PRIVATE Boost::system Threads::Threads)
```

**关键原则**：
1. **Target-centric**：所有属性都应该绑定到target上
2. **No global state**：避免使用全局变量和全局命令
3. **Explicit dependencies**：明确声明依赖关系
4. **Config files over Find modules**：优先使用现代的Config文件

### 第二周：依赖传播与可见性

**学习目标**：深入理解PUBLIC/PRIVATE/INTERFACE

**阅读材料**：
- [ ] CMake文档：target_link_libraries
- [ ] CMake文档：Transitive Usage Requirements

```cmake
# ==========================================
# 理解 PUBLIC / PRIVATE / INTERFACE
# ==========================================

# 假设我们有一个库的层次结构
#
#   app
#    |
#    v
#   mylib (uses json internally, exposes fmt in API)
#    |
#    v
#   json (internal), fmt (exposed in headers)

# --- mylib/CMakeLists.txt ---
add_library(mylib
    src/mylib.cpp
)

# PRIVATE: 只在编译mylib时需要，不传播给使用者
target_link_libraries(mylib PRIVATE nlohmann_json::nlohmann_json)

# PUBLIC: 编译mylib需要，且传播给使用者（头文件中暴露了fmt）
target_link_libraries(mylib PUBLIC fmt::fmt)

# INTERFACE: mylib本身不需要，但使用者需要
target_compile_definitions(mylib INTERFACE MYLIB_USER)

# 头文件目录的可见性
target_include_directories(mylib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- app/CMakeLists.txt ---
add_executable(app main.cpp)
target_link_libraries(app PRIVATE mylib)
# app 自动获得：
# - fmt::fmt（因为mylib PUBLIC链接）
# - include目录（因为mylib PUBLIC导出）
# - MYLIB_USER定义（因为mylib INTERFACE设置）
# app 不会获得：
# - nlohmann_json（因为mylib PRIVATE链接）
```

**Generator Expressions深入**：

```cmake
# ==========================================
# Generator Expressions（生成器表达式）
# ==========================================

# 基本语法: $<CONDITION:VALUE> 或 $<EXPRESSION>

# 1. 条件表达式
target_compile_definitions(mylib
    PRIVATE
        $<$<CONFIG:Debug>:DEBUG_MODE>
        $<$<CONFIG:Release>:NDEBUG>
)

# 2. 编译器判断
target_compile_options(mylib
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

# 3. 平台判断
target_compile_definitions(mylib
    PRIVATE
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Linux>:LINUX_PLATFORM>
        $<$<PLATFORM_ID:Darwin>:MACOS_PLATFORM>
)

# 4. BUILD_INTERFACE vs INSTALL_INTERFACE
target_include_directories(mylib
    PUBLIC
        # 构建时使用源码目录
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # 安装后使用安装目录
        $<INSTALL_INTERFACE:include>
)

# 5. 复合条件
target_compile_options(mylib
    PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Debug>>:-O0 -g3>
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-O3 -DNDEBUG>
)

# 6. 目标属性查询
target_link_libraries(myapp
    PRIVATE
        $<$<TARGET_EXISTS:OpenSSL::SSL>:OpenSSL::SSL>
)
```

### 第三周：CMake模块与函数

**学习目标**：编写可复用的CMake代码

**阅读材料**：
- [ ] CMake文档：cmake-developer
- [ ] 《Professional CMake》第17-20章

```cmake
# ==========================================
# cmake/CompilerWarnings.cmake
# ==========================================
function(set_project_warnings target_name)
    set(MSVC_WARNINGS
        /W4     # 基础警告级别
        /w14242 # 类型转换警告
        /w14254 # 位操作警告
        /w14263 # 虚函数隐藏
        /w14265 # 虚析构函数
        /w14287 # 无符号/负数比较
        /we4289 # 循环变量作用域
        /w14296 # 表达式永远为false
        /w14311 # 指针截断
        /w14545 # 逗号表达式
        /w14546 # 函数调用前缺少参数
        /w14547 # 逗号前的操作符无效
        /w14549 # 逗号前的操作符无效
        /w14555 # 表达式无副作用
        /w14619 # pragma warning未知
        /w14640 # 线程不安全静态初始化
        /w14826 # 有符号扩展转换
        /w14905 # LPSTR转LPWSTR
        /w14906 # LPWSTR转LPSTR
        /w14928 # 非法的拷贝初始化
        /permissive-
    )

    set(CLANG_WARNINGS
        -Wall
        -Wextra
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )

    set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    if(MSVC)
        set(PROJECT_WARNINGS ${MSVC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set(PROJECT_WARNINGS ${CLANG_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(PROJECT_WARNINGS ${GCC_WARNINGS})
    endif()

    target_compile_options(${target_name} PRIVATE ${PROJECT_WARNINGS})
endfunction()

# ==========================================
# cmake/Sanitizers.cmake
# ==========================================
function(enable_sanitizers target_name)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
        option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
        option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)

        set(SANITIZERS "")

        if(ENABLE_ASAN)
            list(APPEND SANITIZERS "address")
        endif()

        if(ENABLE_UBSAN)
            list(APPEND SANITIZERS "undefined")
        endif()

        if(ENABLE_TSAN)
            if(ENABLE_ASAN)
                message(WARNING "TSAN and ASAN cannot be used together")
            else()
                list(APPEND SANITIZERS "thread")
            endif()
        endif()

        if(SANITIZERS)
            list(JOIN SANITIZERS "," SANITIZERS_STR)
            target_compile_options(${target_name} PRIVATE -fsanitize=${SANITIZERS_STR})
            target_link_options(${target_name} PRIVATE -fsanitize=${SANITIZERS_STR})
        endif()
    endif()
endfunction()

# ==========================================
# cmake/StaticAnalyzers.cmake
# ==========================================
function(enable_clang_tidy target_name)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set_target_properties(${target_name}
            PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=*,-fuchsia-*"
        )
    else()
        message(WARNING "clang-tidy not found")
    endif()
endfunction()

function(enable_cppcheck target_name)
    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        set_target_properties(${target_name}
            PROPERTIES CXX_CPPCHECK "${CPPCHECK_EXE};--enable=all;--suppress=missingInclude"
        )
    else()
        message(WARNING "cppcheck not found")
    endif()
endfunction()
```

### 第四周：跨平台项目结构与安装

**学习目标**：构建专业的项目结构，支持安装和导出

**阅读材料**：
- [ ] CMake文档：cmake-packages
- [ ] 《Professional CMake》第25-28章

```cmake
# ==========================================
# 项目根目录 CMakeLists.txt
# ==========================================
cmake_minimum_required(VERSION 3.16)

project(MyProject
    VERSION 1.2.3
    DESCRIPTION "A professional C++ project"
    HOMEPAGE_URL "https://github.com/user/myproject"
    LANGUAGES CXX
)

# 防止in-source构建
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed")
endif()

# 标准项目选项
option(MYPROJECT_BUILD_TESTS "Build tests" ON)
option(MYPROJECT_BUILD_DOCS "Build documentation" OFF)
option(MYPROJECT_BUILD_EXAMPLES "Build examples" ON)
option(MYPROJECT_INSTALL "Generate install target" ON)

# C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 包含自定义模块
include(CompilerWarnings)
include(Sanitizers)

# 子目录
add_subdirectory(src)
add_subdirectory(apps)

if(MYPROJECT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MYPROJECT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ==========================================
# src/CMakeLists.txt - 库定义
# ==========================================
add_library(myproject
    core/engine.cpp
    core/config.cpp
    utils/logger.cpp
    utils/string_utils.cpp
)
add_library(MyProject::myproject ALIAS myproject)

target_include_directories(myproject
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(myproject PUBLIC cxx_std_17)

# 应用警告设置
set_project_warnings(myproject)

# 查找依赖
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

target_link_libraries(myproject
    PUBLIC
        fmt::fmt
    PRIVATE
        spdlog::spdlog
)

# 版本信息
set_target_properties(myproject PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ==========================================
# cmake/MyProjectConfig.cmake.in
# ==========================================
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

find_dependency(fmt)

include("${CMAKE_CURRENT_LIST_DIR}/MyProjectTargets.cmake")

check_required_components(MyProject)

# ==========================================
# 安装规则（在根CMakeLists.txt或单独文件）
# ==========================================
if(MYPROJECT_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # 安装库文件
    install(TARGETS myproject
        EXPORT MyProjectTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # 安装头文件
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # 安装导出目标
    install(EXPORT MyProjectTargets
        FILE MyProjectTargets.cmake
        NAMESPACE MyProject::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyProject
    )

    # 生成版本文件
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/MyProjectConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # 配置Config文件
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MyProjectConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/MyProjectConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyProject
    )

    # 安装Config文件
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/MyProjectConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/MyProjectConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyProject
    )
endif()
```

---

## 源码阅读任务

### 本月源码阅读

1. **CMake官方示例**
   - 仓库：https://github.com/Kitware/CMake
   - 重点：`Tests/Tutorial` 目录
   - 学习目标：理解官方推荐的项目结构

2. **fmtlib的CMake配置**
   - 仓库：https://github.com/fmtlib/fmt
   - 重点：根目录`CMakeLists.txt`和`support/cmake`目录
   - 学习目标：学习高质量库的CMake实践

3. **spdlog的CMake配置**
   - 仓库：https://github.com/gabime/spdlog
   - 重点：CMake配置和header-only模式处理
   - 学习目标：理解header-only库的导出方式

---

## 实践项目

### 项目：构建专业级C++项目模板

创建一个可复用的C++项目模板，包含完整的Modern CMake配置。

```
myproject/
├── CMakeLists.txt
├── cmake/
│   ├── CompilerWarnings.cmake
│   ├── Sanitizers.cmake
│   ├── StaticAnalyzers.cmake
│   └── MyProjectConfig.cmake.in
├── include/
│   └── myproject/
│       ├── myproject.hpp
│       ├── core/
│       │   └── engine.hpp
│       └── utils/
│           └── string_utils.hpp
├── src/
│   ├── CMakeLists.txt
│   ├── core/
│   │   └── engine.cpp
│   └── utils/
│       └── string_utils.cpp
├── apps/
│   ├── CMakeLists.txt
│   └── main.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_engine.cpp
└── examples/
    ├── CMakeLists.txt
    └── basic_example.cpp
```

**include/myproject/myproject.hpp**：

```cpp
#pragma once

// 主头文件，包含所有公共API
#include "myproject/core/engine.hpp"
#include "myproject/utils/string_utils.hpp"

// 版本信息（由CMake生成）
#define MYPROJECT_VERSION_MAJOR @PROJECT_VERSION_MAJOR@
#define MYPROJECT_VERSION_MINOR @PROJECT_VERSION_MINOR@
#define MYPROJECT_VERSION_PATCH @PROJECT_VERSION_PATCH@

namespace myproject {

inline constexpr const char* version() {
    return "@PROJECT_VERSION@";
}

} // namespace myproject
```

**include/myproject/core/engine.hpp**：

```cpp
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace myproject::core {

// 前向声明
class EngineImpl;

/**
 * @brief 核心引擎类
 *
 * 使用PImpl模式隐藏实现细节
 */
class Engine {
public:
    struct Config {
        std::string name = "default";
        size_t thread_count = 4;
        bool enable_logging = true;
    };

    explicit Engine(Config config = {});
    ~Engine();

    // 禁用拷贝
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // 允许移动
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    // 核心API
    void start();
    void stop();
    bool is_running() const;

    // 任务调度
    using Task = std::function<void()>;
    void submit(Task task);
    void wait_all();

    // 状态查询
    size_t pending_tasks() const;
    const Config& config() const;

private:
    std::unique_ptr<EngineImpl> impl_;
};

} // namespace myproject::core
```

**include/myproject/utils/string_utils.hpp**：

```cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <sstream>

namespace myproject::utils {

/**
 * @brief 字符串工具函数集合
 */
class StringUtils {
public:
    // 删除默认构造（纯静态类）
    StringUtils() = delete;

    /**
     * @brief 去除字符串两端空白
     */
    static std::string trim(std::string_view str) {
        auto start = str.find_first_not_of(" \t\n\r");
        if (start == std::string_view::npos) return "";
        auto end = str.find_last_not_of(" \t\n\r");
        return std::string(str.substr(start, end - start + 1));
    }

    /**
     * @brief 分割字符串
     */
    static std::vector<std::string> split(std::string_view str, char delimiter) {
        std::vector<std::string> result;
        std::stringstream ss(std::string(str));
        std::string item;
        while (std::getline(ss, item, delimiter)) {
            if (!item.empty()) {
                result.push_back(item);
            }
        }
        return result;
    }

    /**
     * @brief 连接字符串
     */
    template<typename Container>
    static std::string join(const Container& parts, std::string_view separator) {
        std::string result;
        bool first = true;
        for (const auto& part : parts) {
            if (!first) result += separator;
            result += part;
            first = false;
        }
        return result;
    }

    /**
     * @brief 转小写
     */
    static std::string to_lower(std::string_view str) {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    /**
     * @brief 转大写
     */
    static std::string to_upper(std::string_view str) {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    /**
     * @brief 检查前缀
     */
    static bool starts_with(std::string_view str, std::string_view prefix) {
        return str.size() >= prefix.size() &&
               str.substr(0, prefix.size()) == prefix;
    }

    /**
     * @brief 检查后缀
     */
    static bool ends_with(std::string_view str, std::string_view suffix) {
        return str.size() >= suffix.size() &&
               str.substr(str.size() - suffix.size()) == suffix;
    }

    /**
     * @brief 替换所有匹配项
     */
    static std::string replace_all(std::string_view str,
                                    std::string_view from,
                                    std::string_view to) {
        std::string result(str);
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
        return result;
    }
};

} // namespace myproject::utils
```

**src/core/engine.cpp**：

```cpp
#include "myproject/core/engine.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace myproject::core {

class EngineImpl {
public:
    explicit EngineImpl(Engine::Config config)
        : config_(std::move(config))
        , running_(false) {}

    ~EngineImpl() {
        stop();
    }

    void start() {
        if (running_.exchange(true)) return;

        for (size_t i = 0; i < config_.thread_count; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    void stop() {
        if (!running_.exchange(false)) return;

        cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    bool is_running() const {
        return running_.load();
    }

    void submit(Engine::Task task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    void wait_all() {
        std::unique_lock<std::mutex> lock(mutex_);
        done_cv_.wait(lock, [this] {
            return tasks_.empty() && active_tasks_ == 0;
        });
    }

    size_t pending_tasks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

    const Engine::Config& config() const {
        return config_;
    }

private:
    void worker_loop() {
        while (running_.load()) {
            Engine::Task task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] {
                    return !tasks_.empty() || !running_.load();
                });

                if (!running_.load() && tasks_.empty()) return;

                task = std::move(tasks_.front());
                tasks_.pop();
                ++active_tasks_;
            }

            task();

            {
                std::lock_guard<std::mutex> lock(mutex_);
                --active_tasks_;
            }
            done_cv_.notify_all();
        }
    }

    Engine::Config config_;
    std::atomic<bool> running_;
    std::vector<std::thread> workers_;
    std::queue<Engine::Task> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable done_cv_;
    size_t active_tasks_ = 0;
};

// Engine实现
Engine::Engine(Config config)
    : impl_(std::make_unique<EngineImpl>(std::move(config))) {}

Engine::~Engine() = default;

Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

void Engine::start() { impl_->start(); }
void Engine::stop() { impl_->stop(); }
bool Engine::is_running() const { return impl_->is_running(); }
void Engine::submit(Task task) { impl_->submit(std::move(task)); }
void Engine::wait_all() { impl_->wait_all(); }
size_t Engine::pending_tasks() const { return impl_->pending_tasks(); }
const Engine::Config& Engine::config() const { return impl_->config(); }

} // namespace myproject::core
```

**tests/CMakeLists.txt**：

```cmake
# 查找测试框架
find_package(GTest REQUIRED)
include(GoogleTest)

# 创建测试可执行文件
add_executable(myproject_tests
    test_engine.cpp
    test_string_utils.cpp
)

target_link_libraries(myproject_tests
    PRIVATE
        myproject
        GTest::gtest
        GTest::gtest_main
)

# 发现并注册测试
gtest_discover_tests(myproject_tests)
```

**tests/test_engine.cpp**：

```cpp
#include <gtest/gtest.h>
#include <myproject/core/engine.hpp>
#include <atomic>
#include <chrono>

using namespace myproject::core;

class EngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.name = "test_engine";
        config_.thread_count = 2;
        config_.enable_logging = false;
    }

    Engine::Config config_;
};

TEST_F(EngineTest, DefaultConstruction) {
    Engine engine;
    EXPECT_FALSE(engine.is_running());
}

TEST_F(EngineTest, StartStop) {
    Engine engine(config_);

    engine.start();
    EXPECT_TRUE(engine.is_running());

    engine.stop();
    EXPECT_FALSE(engine.is_running());
}

TEST_F(EngineTest, SubmitTask) {
    Engine engine(config_);
    engine.start();

    std::atomic<int> counter{0};

    for (int i = 0; i < 10; ++i) {
        engine.submit([&counter] {
            ++counter;
        });
    }

    engine.wait_all();
    EXPECT_EQ(counter.load(), 10);

    engine.stop();
}

TEST_F(EngineTest, MoveSemantics) {
    Engine engine1(config_);
    engine1.start();

    Engine engine2 = std::move(engine1);
    EXPECT_TRUE(engine2.is_running());

    engine2.stop();
}
```

---

## 检验标准

- [ ] 理解Modern CMake的Target-centric理念
- [ ] 掌握PUBLIC/PRIVATE/INTERFACE的使用场景
- [ ] 能编写Generator Expressions处理跨平台需求
- [ ] 能创建可复用的CMake模块和函数
- [ ] 能配置完整的项目安装和导出
- [ ] 项目能在Windows/Linux/macOS上正确构建

### 知识检验问题

1. `target_link_libraries`中PUBLIC和PRIVATE的区别是什么？
2. 为什么要用`$<BUILD_INTERFACE:...>`和`$<INSTALL_INTERFACE:...>`？
3. 如何让find_package找到自己的库？
4. CMake中如何实现跨编译器的警告配置？

---

## 输出物清单

1. **项目模板**
   - `myproject/` - 完整的项目结构
   - 可在GitHub上作为template使用

2. **CMake模块库**
   - `cmake/CompilerWarnings.cmake`
   - `cmake/Sanitizers.cmake`
   - `cmake/StaticAnalyzers.cmake`

3. **文档**
   - `notes/month37_cmake.md` - 学习笔记
   - `notes/cmake_cheatsheet.md` - 常用命令速查表

4. **示例配置**
   - 多平台构建脚本
   - VSCode/CLion配置文件

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | Modern CMake基础 | 15h | 15h | 5h |
| 第2周 | 依赖传播机制 | 12h | 18h | 5h |
| 第3周 | 模块与函数 | 10h | 20h | 5h |
| 第4周 | 项目结构与安装 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 38将学习**vcpkg包管理器**，掌握微软开源的跨平台C++包管理工具，实现依赖的自动化管理。
