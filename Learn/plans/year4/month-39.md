# Month 39: Conan包管理器——去中心化的C++依赖管理

## 本月主题概述

本月学习Conan，这是一个去中心化的C/C++包管理器。与vcpkg不同，Conan支持私有仓库、灵活的构建配置和更强大的版本控制。学习Conan的核心概念、配置文件编写，以及如何发布自己的包到ConanCenter或私有仓库。

**学习目标**：
- 掌握Conan 2.x的安装、配置和基本使用
- 理解conanfile.py和conanfile.txt的编写
- 学会创建和发布Conan包
- 对比vcpkg和Conan，理解各自的适用场景

---

## 理论学习内容

### 第一周：Conan基础入门

**学习目标**：安装Conan并理解基本概念

**阅读材料**：
- [ ] Conan官方文档 (docs.conan.io)
- [ ] Conan 2.0迁移指南
- [ ] ConanCenter仓库浏览 (conan.io/center)

**核心概念**：

```bash
# ==========================================
# Conan 2.x 安装
# ==========================================

# 使用pip安装（推荐）
pip install conan

# 或使用pipx（隔离环境）
pipx install conan

# 验证安装
conan --version

# 初始化配置（首次使用）
conan profile detect

# ==========================================
# 基本命令
# ==========================================

# 搜索包（本地缓存）
conan search "*"

# 搜索远程仓库
conan search fmt -r conancenter

# 安装依赖
conan install . --output-folder=build --build=missing

# 创建包
conan create .

# 上传包到远程仓库
conan upload mylib/1.0.0 -r myremote

# 查看包信息
conan inspect fmt/10.0.0

# 列出远程仓库
conan remote list

# 添加远程仓库
conan remote add myremote https://my.conan.server/artifactory/api/conan/conan-local

# ==========================================
# Profile（构建配置）
# ==========================================

# 查看默认profile
conan profile show

# 创建新profile
conan profile detect --name=gcc12

# Profile文件位置
# ~/.conan2/profiles/default

# profile示例
cat > ~/.conan2/profiles/linux-release << 'EOF'
[settings]
arch=x86_64
build_type=Release
compiler=gcc
compiler.cppstd=17
compiler.libcxx=libstdc++11
compiler.version=12
os=Linux

[buildenv]
CC=gcc-12
CXX=g++-12
EOF
```

**Profile详解**：

```ini
# ==========================================
# ~/.conan2/profiles/default
# ==========================================

[settings]
arch=x86_64
build_type=Release
compiler=gcc
compiler.cppstd=17
compiler.libcxx=libstdc++11
compiler.version=12
os=Linux

[options]
mylib/*:shared=True
*:fPIC=True

[buildenv]
CC=gcc
CXX=g++
CFLAGS=-march=native
CXXFLAGS=-march=native

[runenv]
LD_LIBRARY_PATH=/custom/lib

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.build:jobs=8
tools.system.package_manager:mode=install
tools.system.package_manager:sudo=True

# ==========================================
# 跨平台profile示例
# ==========================================

# Windows MSVC
[settings]
arch=x86_64
build_type=Release
compiler=msvc
compiler.cppstd=17
compiler.runtime=dynamic
compiler.version=193
os=Windows

# macOS Clang
[settings]
arch=armv8
build_type=Release
compiler=apple-clang
compiler.cppstd=17
compiler.libcxx=libc++
compiler.version=14
os=Macos

# 交叉编译
[settings]
arch=armv8
build_type=Release
compiler=gcc
compiler.version=11
os=Linux

[buildenv]
CC=aarch64-linux-gnu-gcc
CXX=aarch64-linux-gnu-g++
```

### 第二周：conanfile编写

**学习目标**：掌握conanfile.txt和conanfile.py的编写

**阅读材料**：
- [ ] Conan文档：conanfile.py
- [ ] Conan文档：Generators

```txt
# ==========================================
# conanfile.txt - 简单消费者
# ==========================================
[requires]
fmt/10.0.0
spdlog/1.12.0
boost/1.82.0
nlohmann_json/3.11.2

[generators]
CMakeDeps
CMakeToolchain

[options]
boost/*:without_locale=True
boost/*:without_log=True
spdlog/*:header_only=True

[layout]
cmake_layout
```

```python
# ==========================================
# conanfile.py - 消费者（推荐）
# ==========================================
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain


class MyAppRecipe(ConanFile):
    name = "myapp"
    version = "1.0.0"

    # 源码设置
    settings = "os", "compiler", "build_type", "arch"

    # 依赖
    requires = (
        "fmt/10.0.0",
        "spdlog/1.12.0",
        "boost/1.82.0",
        "nlohmann_json/3.11.2",
    )

    # 构建依赖
    tool_requires = "cmake/3.27.0"

    # 选项
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "boost/*:without_locale": True,
        "boost/*:without_log": True,
    }

    # 依赖的选项传播
    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        # 条件依赖
        if self.settings.os == "Windows":
            self.requires("winsock2/cci.20180802")

    def build_requirements(self):
        self.tool_requires("ninja/1.11.1")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.variables["MY_OPTION"] = "value"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["myapp"]
```

**CMake集成**：

```cmake
# ==========================================
# CMakeLists.txt - 使用Conan生成的文件
# ==========================================
cmake_minimum_required(VERSION 3.16)
project(MyApp VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Conan生成的依赖
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(myapp src/main.cpp)

target_link_libraries(myapp
    PRIVATE
        fmt::fmt
        spdlog::spdlog
        Boost::boost
        nlohmann_json::nlohmann_json
)
```

```bash
# ==========================================
# 构建流程
# ==========================================

# 安装依赖并生成CMake配置
conan install . --output-folder=build --build=missing

# 配置CMake（使用Conan生成的toolchain）
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build build

# 或者使用CMake preset（推荐）
cmake --preset conan-release
cmake --build --preset conan-release
```

### 第三周：创建和发布Conan包

**学习目标**：学会创建完整的Conan包

**阅读材料**：
- [ ] Conan文档：Creating Packages
- [ ] Conan文档：Packaging Approaches
- [ ] ConanCenter贡献指南

```python
# ==========================================
# conanfile.py - 库包定义
# ==========================================
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy, get, rmdir, save, load
from conan.tools.build import check_min_cppstd
import os


class MyLibRecipe(ConanFile):
    name = "mylib"
    version = "1.0.0"
    license = "MIT"
    author = "Your Name <your.email@example.com>"
    url = "https://github.com/username/mylib"
    homepage = "https://github.com/username/mylib"
    description = "A modern C++ library"
    topics = ("cpp", "library", "modern")

    # 包设置
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_ssl": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_ssl": True,
    }

    # 导出源码和CMakeLists
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*"

    def validate(self):
        check_min_cppstd(self, "17")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("fmt/10.0.0")
        self.requires("spdlog/1.12.0")
        if self.options.with_ssl:
            self.requires("openssl/3.1.0")

    def build_requirements(self):
        self.tool_requires("cmake/3.27.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.variables["MYLIB_BUILD_SHARED"] = self.options.shared
        tc.variables["MYLIB_WITH_SSL"] = self.options.with_ssl
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "*.hpp", src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"))

        cmake = CMake(self)
        cmake.install()

        # 清理不需要的文件
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.libs = ["mylib"]

        # 定义宏
        self.cpp_info.defines = ["MYLIB_VERSION={}".format(self.version)]

        if self.options.with_ssl:
            self.cpp_info.defines.append("MYLIB_WITH_SSL")

        # 设置组件（可选，用于更细粒度的依赖）
        self.cpp_info.set_property("cmake_file_name", "MyLib")
        self.cpp_info.set_property("cmake_target_name", "MyLib::MyLib")

        # 系统库依赖
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread", "m"]
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = ["ws2_32"]
```

```python
# ==========================================
# 从远程源码获取的包
# ==========================================
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy
import os


class ExternalLibRecipe(ConanFile):
    name = "externallib"
    version = "2.0.0"
    license = "BSD-3-Clause"
    url = "https://github.com/original/externallib"

    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["externallib"]
```

```yaml
# ==========================================
# conandata.yml - 源码信息
# ==========================================
sources:
  "2.0.0":
    url: "https://github.com/original/externallib/archive/v2.0.0.tar.gz"
    sha256: "abc123..."
  "1.9.0":
    url: "https://github.com/original/externallib/archive/v1.9.0.tar.gz"
    sha256: "def456..."

patches:
  "2.0.0":
    - patch_file: "patches/fix_windows.patch"
      patch_description: "Fix Windows build"
      patch_type: "portability"
```

### 第四周：高级特性与最佳实践

**学习目标**：掌握Conan的高级用法

**阅读材料**：
- [ ] Conan文档：Lockfiles
- [ ] Conan文档：Graph Analysis
- [ ] Conan vs vcpkg比较文章

```python
# ==========================================
# 测试包（test_package/conanfile.py）
# ==========================================
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.build import can_run
import os


class MyLibTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            cmd = os.path.join(self.cpp.build.bindir, "example")
            self.run(cmd, env="conanrun")
```

```cpp
// test_package/src/example.cpp
#include <mylib/mylib.hpp>
#include <iostream>

int main() {
    std::cout << "MyLib version: " << mylib::version() << std::endl;
    return 0;
}
```

```bash
# ==========================================
# Lockfile管理（版本锁定）
# ==========================================

# 生成lockfile
conan lock create .

# 使用lockfile安装
conan install . --lockfile=conan.lock

# 更新特定依赖
conan lock create . --lockfile=conan.lock --lockfile-out=conan.lock --update

# 查看依赖图
conan graph info . --format=html > graph.html

# ==========================================
# 私有仓库设置
# ==========================================

# 添加Artifactory仓库
conan remote add artifactory https://company.jfrog.io/artifactory/api/conan/conan-local
conan remote login artifactory user -p password

# 上传包
conan create .
conan upload mylib/1.0.0 -r artifactory --all

# ==========================================
# 多配置构建
# ==========================================

# 同时构建多个配置
conan install . -s build_type=Debug --output-folder=build-debug --build=missing
conan install . -s build_type=Release --output-folder=build-release --build=missing

# 使用profile文件
conan install . -pr:b=default -pr:h=linux-arm64 --output-folder=build-arm64
```

```python
# ==========================================
# 高级conanfile.py特性
# ==========================================
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import copy, save
from conan.tools.scm import Git
import os


class AdvancedRecipe(ConanFile):
    name = "advanced"
    version = "1.0.0"

    settings = "os", "compiler", "build_type", "arch"

    def export(self):
        # 导出时执行的操作
        git = Git(self)
        scm_url, scm_commit = git.get_url_and_commit()
        save(self, os.path.join(self.export_folder, "scm_info.txt"),
             f"{scm_url}\n{scm_commit}")

    def set_version(self):
        # 动态设置版本号
        git = Git(self)
        self.version = git.run("describe --tags --abbrev=0").strip()

    def export_sources(self):
        # 导出源码时执行
        copy(self, "*", src=self.recipe_folder,
             dst=self.export_sources_folder,
             excludes=["build*", ".git*", "*.pyc"])

    def generate(self):
        # 生成版本头文件
        version_header = f'''
#pragma once
#define ADVANCED_VERSION_MAJOR {self.version.split(".")[0]}
#define ADVANCED_VERSION_MINOR {self.version.split(".")[1]}
#define ADVANCED_VERSION_PATCH {self.version.split(".")[2]}
#define ADVANCED_VERSION "{self.version}"
'''
        save(self, os.path.join(self.build_folder, "version.hpp"), version_header)

    def package_id(self):
        # 自定义包ID计算
        # 忽略某些选项
        self.info.options.rm_safe("with_tests")

        # 兼容性设置
        if self.info.settings.compiler == "gcc":
            if self.info.settings.compiler.version >= "10":
                self.info.settings.compiler.version = "10+"

    def compatibility(self):
        # 定义包兼容性
        return [
            {"settings": [("compiler.version", v) for v in ("11", "12", "13")]}
        ]
```

---

## 源码阅读任务

### 本月源码阅读

1. **Conan客户端源码**
   - 仓库：https://github.com/conan-io/conan
   - 重点：`conan/tools/cmake/` 目录
   - 学习目标：理解CMake集成的实现

2. **ConanCenter索引**
   - 仓库：https://github.com/conan-io/conan-center-index
   - 重点：`recipes/fmt`、`recipes/boost`
   - 学习目标：学习高质量recipe的编写

3. **Conan与vcpkg对比**
   - 分析两者的设计理念差异
   - 理解各自的优缺点

---

## 实践项目

### 项目：跨平台配置管理库

创建一个使用Conan管理的配置管理库，支持YAML、JSON、TOML格式。

**项目结构**：

```
config-lib/
├── conanfile.py
├── conandata.yml
├── CMakeLists.txt
├── include/
│   └── configlib/
│       ├── config.hpp
│       ├── parser.hpp
│       ├── json_parser.hpp
│       ├── yaml_parser.hpp
│       └── toml_parser.hpp
├── src/
│   ├── CMakeLists.txt
│   ├── config.cpp
│   ├── json_parser.cpp
│   ├── yaml_parser.cpp
│   └── toml_parser.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_config.cpp
└── test_package/
    ├── conanfile.py
    ├── CMakeLists.txt
    └── src/
        └── example.cpp
```

**conanfile.py**：

```python
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
from conan.tools.build import check_min_cppstd
import os


class ConfigLibRecipe(ConanFile):
    name = "configlib"
    version = "1.0.0"
    license = "MIT"
    author = "Your Name <your.email@example.com>"
    url = "https://github.com/username/configlib"
    description = "A cross-platform configuration library supporting JSON, YAML, and TOML"
    topics = ("config", "json", "yaml", "toml", "modern-cpp")

    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_yaml": [True, False],
        "with_toml": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_yaml": True,
        "with_toml": True,
    }

    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*"

    def validate(self):
        check_min_cppstd(self, "17")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("nlohmann_json/3.11.2")
        self.requires("fmt/10.0.0")
        if self.options.with_yaml:
            self.requires("yaml-cpp/0.8.0")
        if self.options.with_toml:
            self.requires("toml11/3.7.1")

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.variables["CONFIGLIB_BUILD_SHARED"] = self.options.shared
        tc.variables["CONFIGLIB_WITH_YAML"] = self.options.with_yaml
        tc.variables["CONFIGLIB_WITH_TOML"] = self.options.with_toml
        tc.variables["CONFIGLIB_BUILD_TESTS"] = not self.conf.get(
            "tools.build:skip_test", default=False)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake.test()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["configlib"]

        self.cpp_info.set_property("cmake_file_name", "ConfigLib")
        self.cpp_info.set_property("cmake_target_name", "ConfigLib::ConfigLib")

        if self.options.with_yaml:
            self.cpp_info.defines.append("CONFIGLIB_WITH_YAML")
        if self.options.with_toml:
            self.cpp_info.defines.append("CONFIGLIB_WITH_TOML")
```

**include/configlib/config.hpp**：

```cpp
#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <variant>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <filesystem>

namespace configlib {

// 前向声明
class ConfigNode;
class ConfigParser;

/**
 * @brief 配置值类型
 */
using ConfigValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<ConfigNode>,
    std::map<std::string, ConfigNode>
>;

/**
 * @brief 配置节点
 */
class ConfigNode {
public:
    ConfigNode() : value_(nullptr) {}
    ConfigNode(ConfigValue value) : value_(std::move(value)) {}

    // 类型检查
    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_double() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    // 值获取（带默认值）
    template<typename T>
    T get(T default_value = T{}) const;

    // 值获取（可能抛出异常）
    template<typename T>
    T as() const;

    // 数组访问
    const ConfigNode& operator[](size_t index) const;
    size_t size() const;

    // 对象访问
    const ConfigNode& operator[](std::string_view key) const;
    bool contains(std::string_view key) const;
    std::vector<std::string> keys() const;

    // 获取可选值
    template<typename T>
    std::optional<T> get_optional() const;

    // 路径访问（支持点分隔）
    const ConfigNode& at(std::string_view path) const;
    std::optional<std::reference_wrapper<const ConfigNode>>
        find(std::string_view path) const;

private:
    ConfigValue value_;
    static const ConfigNode null_node_;
};

/**
 * @brief 配置异常
 */
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigParseError : public ConfigError {
public:
    ConfigParseError(const std::string& msg, size_t line = 0, size_t column = 0)
        : ConfigError(msg), line_(line), column_(column) {}

    size_t line() const { return line_; }
    size_t column() const { return column_; }

private:
    size_t line_;
    size_t column_;
};

class ConfigTypeError : public ConfigError {
public:
    using ConfigError::ConfigError;
};

/**
 * @brief 配置格式
 */
enum class ConfigFormat {
    JSON,
    YAML,
    TOML,
    AUTO  // 根据文件扩展名自动检测
};

/**
 * @brief 配置类
 */
class Config {
public:
    Config() = default;

    /**
     * @brief 从文件加载配置
     */
    static Config load(const std::filesystem::path& path,
                       ConfigFormat format = ConfigFormat::AUTO);

    /**
     * @brief 从字符串解析配置
     */
    static Config parse(std::string_view content, ConfigFormat format);

    /**
     * @brief 保存配置到文件
     */
    void save(const std::filesystem::path& path,
              ConfigFormat format = ConfigFormat::AUTO) const;

    /**
     * @brief 序列化为字符串
     */
    std::string serialize(ConfigFormat format) const;

    /**
     * @brief 获取根节点
     */
    const ConfigNode& root() const { return root_; }
    ConfigNode& root() { return root_; }

    /**
     * @brief 便捷访问
     */
    const ConfigNode& operator[](std::string_view key) const {
        return root_[key];
    }

    template<typename T>
    T get(std::string_view path, T default_value = T{}) const {
        auto node = root_.find(path);
        if (node) {
            return node->get().get<T>(std::move(default_value));
        }
        return default_value;
    }

    /**
     * @brief 合并另一个配置
     */
    void merge(const Config& other);

    /**
     * @brief 设置值
     */
    template<typename T>
    void set(std::string_view path, T value);

private:
    ConfigNode root_;

    static ConfigFormat detect_format(const std::filesystem::path& path);
    static std::unique_ptr<ConfigParser> create_parser(ConfigFormat format);
};

/**
 * @brief 配置解析器接口
 */
class ConfigParser {
public:
    virtual ~ConfigParser() = default;
    virtual ConfigNode parse(std::string_view content) = 0;
    virtual std::string serialize(const ConfigNode& node) = 0;
};

} // namespace configlib
```

**src/config.cpp**：

```cpp
#include "configlib/config.hpp"
#include "configlib/json_parser.hpp"

#ifdef CONFIGLIB_WITH_YAML
#include "configlib/yaml_parser.hpp"
#endif

#ifdef CONFIGLIB_WITH_TOML
#include "configlib/toml_parser.hpp"
#endif

#include <fstream>
#include <sstream>
#include <algorithm>

namespace configlib {

const ConfigNode ConfigNode::null_node_{};

bool ConfigNode::is_null() const {
    return std::holds_alternative<std::nullptr_t>(value_);
}

bool ConfigNode::is_bool() const {
    return std::holds_alternative<bool>(value_);
}

bool ConfigNode::is_int() const {
    return std::holds_alternative<int64_t>(value_);
}

bool ConfigNode::is_double() const {
    return std::holds_alternative<double>(value_);
}

bool ConfigNode::is_string() const {
    return std::holds_alternative<std::string>(value_);
}

bool ConfigNode::is_array() const {
    return std::holds_alternative<std::vector<ConfigNode>>(value_);
}

bool ConfigNode::is_object() const {
    return std::holds_alternative<std::map<std::string, ConfigNode>>(value_);
}

template<>
bool ConfigNode::get<bool>(bool default_value) const {
    if (auto* val = std::get_if<bool>(&value_)) {
        return *val;
    }
    return default_value;
}

template<>
int64_t ConfigNode::get<int64_t>(int64_t default_value) const {
    if (auto* val = std::get_if<int64_t>(&value_)) {
        return *val;
    }
    return default_value;
}

template<>
int ConfigNode::get<int>(int default_value) const {
    return static_cast<int>(get<int64_t>(default_value));
}

template<>
double ConfigNode::get<double>(double default_value) const {
    if (auto* val = std::get_if<double>(&value_)) {
        return *val;
    }
    if (auto* val = std::get_if<int64_t>(&value_)) {
        return static_cast<double>(*val);
    }
    return default_value;
}

template<>
std::string ConfigNode::get<std::string>(std::string default_value) const {
    if (auto* val = std::get_if<std::string>(&value_)) {
        return *val;
    }
    return default_value;
}

const ConfigNode& ConfigNode::operator[](size_t index) const {
    if (auto* arr = std::get_if<std::vector<ConfigNode>>(&value_)) {
        if (index < arr->size()) {
            return (*arr)[index];
        }
    }
    return null_node_;
}

size_t ConfigNode::size() const {
    if (auto* arr = std::get_if<std::vector<ConfigNode>>(&value_)) {
        return arr->size();
    }
    if (auto* obj = std::get_if<std::map<std::string, ConfigNode>>(&value_)) {
        return obj->size();
    }
    return 0;
}

const ConfigNode& ConfigNode::operator[](std::string_view key) const {
    if (auto* obj = std::get_if<std::map<std::string, ConfigNode>>(&value_)) {
        auto it = obj->find(std::string(key));
        if (it != obj->end()) {
            return it->second;
        }
    }
    return null_node_;
}

bool ConfigNode::contains(std::string_view key) const {
    if (auto* obj = std::get_if<std::map<std::string, ConfigNode>>(&value_)) {
        return obj->find(std::string(key)) != obj->end();
    }
    return false;
}

std::vector<std::string> ConfigNode::keys() const {
    std::vector<std::string> result;
    if (auto* obj = std::get_if<std::map<std::string, ConfigNode>>(&value_)) {
        result.reserve(obj->size());
        for (const auto& [key, _] : *obj) {
            result.push_back(key);
        }
    }
    return result;
}

const ConfigNode& ConfigNode::at(std::string_view path) const {
    auto result = find(path);
    if (!result) {
        throw ConfigError("Path not found: " + std::string(path));
    }
    return result->get();
}

std::optional<std::reference_wrapper<const ConfigNode>>
ConfigNode::find(std::string_view path) const {
    const ConfigNode* current = this;

    size_t pos = 0;
    while (pos < path.size()) {
        size_t dot = path.find('.', pos);
        if (dot == std::string_view::npos) {
            dot = path.size();
        }

        std::string_view key = path.substr(pos, dot - pos);

        if (!current->is_object()) {
            return std::nullopt;
        }

        if (!current->contains(key)) {
            return std::nullopt;
        }

        current = &((*current)[key]);
        pos = dot + 1;
    }

    return std::cref(*current);
}

// Config implementation

Config Config::load(const std::filesystem::path& path, ConfigFormat format) {
    if (format == ConfigFormat::AUTO) {
        format = detect_format(path);
    }

    std::ifstream file(path);
    if (!file) {
        throw ConfigError("Cannot open file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return parse(buffer.str(), format);
}

Config Config::parse(std::string_view content, ConfigFormat format) {
    auto parser = create_parser(format);
    Config config;
    config.root_ = parser->parse(content);
    return config;
}

void Config::save(const std::filesystem::path& path, ConfigFormat format) const {
    if (format == ConfigFormat::AUTO) {
        format = detect_format(path);
    }

    std::ofstream file(path);
    if (!file) {
        throw ConfigError("Cannot open file for writing: " + path.string());
    }

    file << serialize(format);
}

std::string Config::serialize(ConfigFormat format) const {
    auto parser = create_parser(format);
    return parser->serialize(root_);
}

ConfigFormat Config::detect_format(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".json") return ConfigFormat::JSON;
    if (ext == ".yaml" || ext == ".yml") return ConfigFormat::YAML;
    if (ext == ".toml") return ConfigFormat::TOML;

    throw ConfigError("Unknown file format: " + path.string());
}

std::unique_ptr<ConfigParser> Config::create_parser(ConfigFormat format) {
    switch (format) {
        case ConfigFormat::JSON:
            return std::make_unique<JsonParser>();
#ifdef CONFIGLIB_WITH_YAML
        case ConfigFormat::YAML:
            return std::make_unique<YamlParser>();
#endif
#ifdef CONFIGLIB_WITH_TOML
        case ConfigFormat::TOML:
            return std::make_unique<TomlParser>();
#endif
        default:
            throw ConfigError("Unsupported format");
    }
}

} // namespace configlib
```

**tests/test_config.cpp**：

```cpp
#include <gtest/gtest.h>
#include <configlib/config.hpp>
#include <fstream>

using namespace configlib;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        json_content_ = R"({
            "name": "test",
            "version": 1,
            "enabled": true,
            "ratio": 3.14,
            "tags": ["a", "b", "c"],
            "database": {
                "host": "localhost",
                "port": 5432
            }
        })";
    }

    std::string json_content_;
};

TEST_F(ConfigTest, ParseJson) {
    auto config = Config::parse(json_content_, ConfigFormat::JSON);

    EXPECT_EQ(config.get<std::string>("name"), "test");
    EXPECT_EQ(config.get<int>("version"), 1);
    EXPECT_EQ(config.get<bool>("enabled"), true);
    EXPECT_NEAR(config.get<double>("ratio"), 3.14, 0.001);
}

TEST_F(ConfigTest, AccessNestedValues) {
    auto config = Config::parse(json_content_, ConfigFormat::JSON);

    EXPECT_EQ(config.get<std::string>("database.host"), "localhost");
    EXPECT_EQ(config.get<int>("database.port"), 5432);
}

TEST_F(ConfigTest, AccessArray) {
    auto config = Config::parse(json_content_, ConfigFormat::JSON);
    const auto& tags = config.root()["tags"];

    EXPECT_EQ(tags.size(), 3);
    EXPECT_EQ(tags[0].get<std::string>(), "a");
    EXPECT_EQ(tags[1].get<std::string>(), "b");
    EXPECT_EQ(tags[2].get<std::string>(), "c");
}

TEST_F(ConfigTest, DefaultValues) {
    auto config = Config::parse(json_content_, ConfigFormat::JSON);

    EXPECT_EQ(config.get<std::string>("missing", "default"), "default");
    EXPECT_EQ(config.get<int>("missing", 42), 42);
}

TEST_F(ConfigTest, TypeChecks) {
    auto config = Config::parse(json_content_, ConfigFormat::JSON);
    const auto& root = config.root();

    EXPECT_TRUE(root["name"].is_string());
    EXPECT_TRUE(root["version"].is_int());
    EXPECT_TRUE(root["enabled"].is_bool());
    EXPECT_TRUE(root["ratio"].is_double());
    EXPECT_TRUE(root["tags"].is_array());
    EXPECT_TRUE(root["database"].is_object());
}

TEST_F(ConfigTest, Contains) {
    auto config = Config::parse(json_content_, ConfigFormat::JSON);

    EXPECT_TRUE(config.root().contains("name"));
    EXPECT_TRUE(config.root().contains("database"));
    EXPECT_FALSE(config.root().contains("nonexistent"));
}

TEST_F(ConfigTest, Keys) {
    auto config = Config::parse(json_content_, ConfigFormat::JSON);
    auto keys = config.root().keys();

    EXPECT_EQ(keys.size(), 6);
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "name") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "database") != keys.end());
}

TEST_F(ConfigTest, InvalidJson) {
    EXPECT_THROW(Config::parse("invalid json", ConfigFormat::JSON), ConfigParseError);
}

#ifdef CONFIGLIB_WITH_YAML
TEST_F(ConfigTest, ParseYaml) {
    std::string yaml = R"(
name: test
version: 1
enabled: true
database:
  host: localhost
  port: 5432
)";

    auto config = Config::parse(yaml, ConfigFormat::YAML);

    EXPECT_EQ(config.get<std::string>("name"), "test");
    EXPECT_EQ(config.get<int>("database.port"), 5432);
}
#endif

#ifdef CONFIGLIB_WITH_TOML
TEST_F(ConfigTest, ParseToml) {
    std::string toml = R"(
name = "test"
version = 1
enabled = true

[database]
host = "localhost"
port = 5432
)";

    auto config = Config::parse(toml, ConfigFormat::TOML);

    EXPECT_EQ(config.get<std::string>("name"), "test");
    EXPECT_EQ(config.get<int>("database.port"), 5432);
}
#endif
```

---

## 检验标准

- [ ] 能够安装和配置Conan 2.x
- [ ] 理解conanfile.txt和conanfile.py的区别
- [ ] 能够编写完整的Conan包配方
- [ ] 能够使用profile进行多平台构建
- [ ] 能够配置私有仓库
- [ ] 理解vcpkg和Conan的差异和适用场景

### 知识检验问题

1. Conan的profile和vcpkg的triplet有什么区别？
2. conanfile.py中的`package_id()`有什么作用？
3. 如何在Conan中处理可选依赖？
4. Conan的lockfile机制解决了什么问题？

### vcpkg vs Conan对比

| 特性 | vcpkg | Conan |
|------|-------|-------|
| 架构 | 中心化 | 去中心化 |
| 配置语言 | CMake | Python |
| 构建系统 | 主要CMake | 多种（CMake、Meson等） |
| 私有仓库 | 支持（registry） | 原生支持 |
| 二进制缓存 | 支持 | 原生支持 |
| 版本控制 | baseline机制 | 完整版本语义 |
| 学习曲线 | 较低 | 较高 |
| 包数量 | ~2000 | ~1500 |

---

## 输出物清单

1. **项目代码**
   - `config-lib/` - 完整的配置管理库
   - conanfile.py和CMake配置

2. **文档**
   - `notes/month39_conan.md` - 学习笔记
   - `notes/vcpkg_vs_conan.md` - 对比分析

3. **模板**
   - Conan包模板
   - 多平台profile配置

4. **脚本**
   - `scripts/build-all.sh` - 多配置构建脚本
   - `scripts/upload.sh` - 包上传脚本

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | Conan基础入门 | 15h | 15h | 5h |
| 第2周 | conanfile编写 | 12h | 18h | 5h |
| 第3周 | 创建发布包 | 10h | 20h | 5h |
| 第4周 | 高级特性 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 40将学习**CI/CD流水线（GitHub Actions）**，实现代码的自动化构建、测试和部署。
