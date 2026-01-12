# Month 38: vcpkg包管理器——微软开源的跨平台依赖管理

## 本月主题概述

本月深入学习vcpkg，这是微软开源的C/C++包管理器。vcpkg支持Windows、Linux和macOS，能够自动下载、编译和安装第三方库。学习如何将vcpkg集成到项目中，管理依赖版本，以及创建自定义的port。

**学习目标**：
- 掌握vcpkg的安装、配置和基本使用
- 理解manifest模式和classic模式的区别
- 学会创建自定义port发布自己的库
- 实现vcpkg与CMake的无缝集成

---

## 理论学习内容

### 第一周：vcpkg基础入门

**学习目标**：安装vcpkg并理解基本概念

**阅读材料**：
- [ ] vcpkg官方文档 (vcpkg.io/en/getting-started)
- [ ] Microsoft Learn: vcpkg入门教程
- [ ] vcpkg GitHub仓库README

**核心概念**：

```bash
# ==========================================
# vcpkg安装（跨平台）
# ==========================================

# 克隆仓库
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# Linux/macOS
./bootstrap-vcpkg.sh

# Windows
.\bootstrap-vcpkg.bat

# 设置环境变量（推荐）
export VCPKG_ROOT=/path/to/vcpkg
export PATH=$VCPKG_ROOT:$PATH

# ==========================================
# 基本命令
# ==========================================

# 搜索包
vcpkg search json
vcpkg search boost

# 安装包（Classic模式）
vcpkg install fmt
vcpkg install spdlog
vcpkg install boost-asio

# 指定triplet（目标平台）
vcpkg install fmt:x64-windows
vcpkg install fmt:x64-linux
vcpkg install fmt:x64-osx
vcpkg install fmt:arm64-osx

# 查看已安装包
vcpkg list

# 移除包
vcpkg remove fmt

# 更新vcpkg自身
git pull
./bootstrap-vcpkg.sh

# 更新所有包
vcpkg upgrade --no-dry-run
```

**Triplet详解**：

```bash
# ==========================================
# Triplet = 架构-系统-链接方式
# ==========================================

# 常用triplet
# x64-windows        - Windows 64位 动态链接
# x64-windows-static - Windows 64位 静态链接
# x86-windows        - Windows 32位 动态链接
# x64-linux          - Linux 64位
# x64-osx            - macOS x64
# arm64-osx          - macOS Apple Silicon
# arm64-linux        - Linux ARM64

# 查看所有可用triplet
vcpkg help triplet

# 自定义triplet文件
# triplets/x64-linux-custom.cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)  # 静态库
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# C++标准
set(VCPKG_CXX_FLAGS "-std=c++17")
set(VCPKG_C_FLAGS "")

# 编译器设置
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "/path/to/toolchain.cmake")
```

### 第二周：Manifest模式（推荐）

**学习目标**：使用vcpkg.json管理项目依赖

**阅读材料**：
- [ ] vcpkg文档：Manifest Mode
- [ ] vcpkg.json规范

```json
// ==========================================
// vcpkg.json - 项目依赖清单
// ==========================================
{
  "name": "my-application",
  "version-string": "1.0.0",
  "description": "My awesome C++ application",
  "homepage": "https://github.com/user/my-application",
  "license": "MIT",
  "supports": "!uwp",
  "dependencies": [
    "fmt",
    "spdlog",
    {
      "name": "boost-asio",
      "version>=": "1.81.0"
    },
    {
      "name": "openssl",
      "platform": "!windows"
    },
    {
      "name": "catch2",
      "host": true
    }
  ],
  "builtin-baseline": "a34c873a9717a888f58dc05268dea15592c2f0ff",
  "overrides": [
    {
      "name": "fmt",
      "version": "9.1.0"
    }
  ],
  "features": {
    "tests": {
      "description": "Build tests",
      "dependencies": [
        "gtest"
      ]
    },
    "benchmarks": {
      "description": "Build benchmarks",
      "dependencies": [
        "benchmark"
      ]
    }
  },
  "default-features": []
}
```

```json
// ==========================================
// vcpkg-configuration.json - 高级配置
// ==========================================
{
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "a34c873a9717a888f58dc05268dea15592c2f0ff"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/mycompany/vcpkg-registry",
      "baseline": "abc123...",
      "packages": ["my-internal-lib", "another-lib"]
    }
  ],
  "overlay-ports": ["./custom-ports"],
  "overlay-triplets": ["./custom-triplets"]
}
```

**CMake集成**：

```cmake
# ==========================================
# CMakeLists.txt - vcpkg集成
# ==========================================
cmake_minimum_required(VERSION 3.16)

# 方法1：通过CMake命令行设置toolchain
# cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# 方法2：在CMakeLists.txt中设置（需在project之前）
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(MyApplication VERSION 1.0.0 LANGUAGES CXX)

# vcpkg manifest模式会自动安装vcpkg.json中的依赖
# 然后可以正常使用find_package

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system asio)

add_executable(myapp main.cpp)

target_link_libraries(myapp
    PRIVATE
        fmt::fmt
        spdlog::spdlog
        Boost::system
        Boost::asio
)

# 设置C++标准
target_compile_features(myapp PRIVATE cxx_std_17)
```

### 第三周：创建自定义Port

**学习目标**：为自己的库或第三方库创建vcpkg port

**阅读材料**：
- [ ] vcpkg文档：Creating Ports
- [ ] vcpkg文档：Portfile Functions

```cmake
# ==========================================
# ports/mylib/portfile.cmake
# ==========================================

# 从GitHub下载源码
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO username/mylib
    REF v${VERSION}
    SHA512 abc123...
    HEAD_REF main
)

# 或者从其他来源下载
# vcpkg_download_distfile(ARCHIVE
#     URLS "https://example.com/mylib-${VERSION}.tar.gz"
#     FILENAME "mylib-${VERSION}.tar.gz"
#     SHA512 abc123...
# )
# vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE ${ARCHIVE})

# 配置CMake项目
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DMYLIB_BUILD_TESTS=OFF
        -DMYLIB_BUILD_EXAMPLES=OFF
)

# 构建
vcpkg_cmake_build()

# 安装
vcpkg_cmake_install()

# 修复CMake配置文件路径
vcpkg_cmake_config_fixup(
    PACKAGE_NAME mylib
    CONFIG_PATH lib/cmake/mylib
)

# 处理版权文件
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# 移除空目录
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

# 生成使用说明
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
```

```json
// ports/mylib/vcpkg.json
{
  "name": "mylib",
  "version": "1.2.3",
  "port-version": 0,
  "description": "My awesome library",
  "homepage": "https://github.com/username/mylib",
  "license": "MIT",
  "supports": "!uwp",
  "dependencies": [
    "fmt",
    {
      "name": "vcpkg-cmake",
      "host": true
    },
    {
      "name": "vcpkg-cmake-config",
      "host": true
    }
  ],
  "features": {
    "ssl": {
      "description": "Enable SSL support",
      "dependencies": ["openssl"]
    }
  }
}
```

```
# ports/mylib/usage
mylib provides CMake targets:

    find_package(mylib CONFIG REQUIRED)
    target_link_libraries(main PRIVATE mylib::mylib)
```

### 第四周：高级特性与最佳实践

**学习目标**：掌握vcpkg的高级用法

**阅读材料**：
- [ ] vcpkg文档：Binary Caching
- [ ] vcpkg文档：Asset Caching
- [ ] vcpkg文档：Registries

```bash
# ==========================================
# 二进制缓存（加速CI/CD）
# ==========================================

# 文件系统缓存（默认）
export VCPKG_BINARY_SOURCES="clear;files,/path/to/cache,readwrite"

# NuGet缓存
export VCPKG_BINARY_SOURCES="clear;nuget,https://nuget.example.com/index.json,readwrite"

# GitHub Packages
export VCPKG_BINARY_SOURCES="clear;nuget,GitHub,readwrite"

# AWS S3
export VCPKG_BINARY_SOURCES="clear;x-aws,s3://my-bucket/vcpkg-cache/,readwrite"

# Azure Blob Storage
export VCPKG_BINARY_SOURCES="clear;x-azblob,https://myaccount.blob.core.windows.net/vcpkg,readwrite"

# 多个缓存源（优先级从左到右）
export VCPKG_BINARY_SOURCES="clear;files,/local/cache,read;nuget,https://remote/,readwrite"

# ==========================================
# 资产缓存（下载源码缓存）
# ==========================================

# 使用镜像加速下载
export X_VCPKG_ASSET_SOURCES="x-azurl,https://mirror.example.com/;x-block-origin"

# ==========================================
# 版本控制
# ==========================================

# 查看包的所有可用版本
vcpkg x-history fmt

# 使用特定版本（在vcpkg.json中）
{
  "dependencies": [
    {
      "name": "fmt",
      "version>=": "9.0.0"
    }
  ],
  "overrides": [
    {
      "name": "fmt",
      "version": "9.1.0"
    }
  ]
}
```

```cmake
# ==========================================
# CMake预设与vcpkg集成
# ==========================================
# CMakePresets.json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "vcpkg-base",
      "hidden": true,
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": {
          "type": "FILEPATH",
          "value": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        }
      }
    },
    {
      "name": "debug",
      "displayName": "Debug",
      "inherits": "vcpkg-base",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "release",
      "displayName": "Release",
      "inherits": "vcpkg-base",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "windows-x64",
      "displayName": "Windows x64",
      "inherits": "vcpkg-base",
      "binaryDir": "${sourceDir}/build/windows",
      "cacheVariables": {
        "VCPKG_TARGET_TRIPLET": "x64-windows"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "debug",
      "configurePreset": "debug"
    },
    {
      "name": "release",
      "configurePreset": "release"
    }
  ]
}
```

---

## 源码阅读任务

### 本月源码阅读

1. **vcpkg核心代码**
   - 仓库：https://github.com/microsoft/vcpkg-tool
   - 重点：`src/vcpkg/` 目录
   - 学习目标：理解包管理器的实现原理

2. **热门port示例**
   - 仓库：https://github.com/microsoft/vcpkg
   - 重点：`ports/fmt`、`ports/spdlog`、`ports/boost`
   - 学习目标：学习高质量port的编写方式

3. **vcpkg-cmake辅助函数**
   - 路径：`scripts/cmake/`
   - 重点：`vcpkg_from_github.cmake`、`vcpkg_cmake_configure.cmake`
   - 学习目标：理解portfile辅助函数的实现

---

## 实践项目

### 项目：vcpkg集成的跨平台网络库

创建一个使用vcpkg管理依赖的网络工具库。

**项目结构**：

```
network-toolkit/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── vcpkg-configuration.json
├── include/
│   └── nettool/
│       ├── http_client.hpp
│       ├── dns_resolver.hpp
│       └── url_parser.hpp
├── src/
│   ├── CMakeLists.txt
│   ├── http_client.cpp
│   ├── dns_resolver.cpp
│   └── url_parser.cpp
├── apps/
│   ├── CMakeLists.txt
│   └── http_get.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_url_parser.cpp
└── ports/
    └── nettool/
        ├── portfile.cmake
        ├── vcpkg.json
        └── usage
```

**vcpkg.json**：

```json
{
  "name": "network-toolkit",
  "version": "1.0.0",
  "description": "A cross-platform network toolkit",
  "homepage": "https://github.com/user/network-toolkit",
  "license": "MIT",
  "dependencies": [
    "fmt",
    "spdlog",
    {
      "name": "openssl",
      "version>=": "3.0.0"
    },
    {
      "name": "boost-asio",
      "version>=": "1.81.0"
    },
    {
      "name": "boost-beast",
      "version>=": "1.81.0"
    },
    {
      "name": "nlohmann-json",
      "version>=": "3.11.0"
    }
  ],
  "builtin-baseline": "a34c873a9717a888f58dc05268dea15592c2f0ff",
  "features": {
    "tests": {
      "description": "Build tests",
      "dependencies": [
        {
          "name": "gtest",
          "version>=": "1.12.0"
        }
      ]
    }
  }
}
```

**CMakeLists.txt**（根目录）：

```cmake
cmake_minimum_required(VERSION 3.16)

project(NetworkToolkit
    VERSION 1.0.0
    DESCRIPTION "A cross-platform network toolkit"
    LANGUAGES CXX
)

# C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 选项
option(NETTOOL_BUILD_TESTS "Build tests" ON)
option(NETTOOL_BUILD_APPS "Build applications" ON)

# 查找依赖（vcpkg会自动处理）
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(nlohmann_json CONFIG REQUIRED)

# 子目录
add_subdirectory(src)

if(NETTOOL_BUILD_APPS)
    add_subdirectory(apps)
endif()

if(NETTOOL_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
```

**include/nettool/url_parser.hpp**：

```cpp
#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <stdexcept>

namespace nettool {

/**
 * @brief URL解析结果
 */
struct Url {
    std::string scheme;      // http, https
    std::string host;        // www.example.com
    uint16_t port = 0;       // 80, 443
    std::string path;        // /path/to/resource
    std::string query;       // key=value&foo=bar
    std::string fragment;    // section1

    // 获取完整URL
    std::string to_string() const;

    // 获取带端口的host
    std::string host_with_port() const;

    // 检查是否为HTTPS
    bool is_secure() const { return scheme == "https"; }

    // 获取默认端口
    static uint16_t default_port(std::string_view scheme);
};

/**
 * @brief URL解析异常
 */
class UrlParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief URL解析器
 */
class UrlParser {
public:
    /**
     * @brief 解析URL字符串
     * @param url_string URL字符串
     * @return 解析结果
     * @throws UrlParseError 解析失败时抛出
     */
    static Url parse(std::string_view url_string);

    /**
     * @brief 尝试解析URL字符串
     * @param url_string URL字符串
     * @return 解析结果，失败返回nullopt
     */
    static std::optional<Url> try_parse(std::string_view url_string) noexcept;

    /**
     * @brief URL编码
     */
    static std::string encode(std::string_view str);

    /**
     * @brief URL解码
     */
    static std::string decode(std::string_view str);

private:
    static bool is_unreserved(char c);
};

} // namespace nettool
```

**src/url_parser.cpp**：

```cpp
#include "nettool/url_parser.hpp"
#include <fmt/format.h>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <regex>

namespace nettool {

std::string Url::to_string() const {
    std::string result = scheme + "://" + host;

    if (port != 0 && port != default_port(scheme)) {
        result += ":" + std::to_string(port);
    }

    result += path.empty() ? "/" : path;

    if (!query.empty()) {
        result += "?" + query;
    }

    if (!fragment.empty()) {
        result += "#" + fragment;
    }

    return result;
}

std::string Url::host_with_port() const {
    uint16_t p = port != 0 ? port : default_port(scheme);
    return fmt::format("{}:{}", host, p);
}

uint16_t Url::default_port(std::string_view scheme) {
    if (scheme == "http") return 80;
    if (scheme == "https") return 443;
    if (scheme == "ftp") return 21;
    if (scheme == "ssh") return 22;
    return 0;
}

Url UrlParser::parse(std::string_view url_string) {
    auto result = try_parse(url_string);
    if (!result) {
        throw UrlParseError(fmt::format("Invalid URL: {}", url_string));
    }
    return *result;
}

std::optional<Url> UrlParser::try_parse(std::string_view url_string) noexcept {
    try {
        // 使用正则表达式解析URL
        // scheme://[user:pass@]host[:port][/path][?query][#fragment]
        static const std::regex url_regex(
            R"(^([a-zA-Z][a-zA-Z0-9+.-]*):\/\/)"  // scheme
            R"((?:[^:@]+(?::[^@]*)?@)?)"           // user:pass@ (optional)
            R"(([^:/?#]+))"                        // host
            R"((?::(\d+))?)"                       // :port (optional)
            R"((\/[^?#]*)?)"                       // /path (optional)
            R"((?:\?([^#]*))?)"                    // ?query (optional)
            R"((?:#(.*))?$)"                       // #fragment (optional)
        );

        std::string url_str(url_string);
        std::smatch match;

        if (!std::regex_match(url_str, match, url_regex)) {
            return std::nullopt;
        }

        Url url;
        url.scheme = match[1].str();

        // 转小写scheme
        std::transform(url.scheme.begin(), url.scheme.end(),
                       url.scheme.begin(), ::tolower);

        url.host = match[2].str();

        // 解析端口
        if (match[3].matched && !match[3].str().empty()) {
            url.port = static_cast<uint16_t>(std::stoi(match[3].str()));
        } else {
            url.port = Url::default_port(url.scheme);
        }

        url.path = match[4].matched ? match[4].str() : "/";
        url.query = match[5].matched ? match[5].str() : "";
        url.fragment = match[6].matched ? match[6].str() : "";

        return url;
    } catch (...) {
        return std::nullopt;
    }
}

bool UrlParser::is_unreserved(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

std::string UrlParser::encode(std::string_view str) {
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;

    for (char c : str) {
        if (is_unreserved(c)) {
            encoded << c;
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return encoded.str();
}

std::string UrlParser::decode(std::string_view str) {
    std::string decoded;
    decoded.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int value;
            std::istringstream iss(std::string(str.substr(i + 1, 2)));
            if (iss >> std::hex >> value) {
                decoded += static_cast<char>(value);
                i += 2;
                continue;
            }
        } else if (str[i] == '+') {
            decoded += ' ';
            continue;
        }
        decoded += str[i];
    }

    return decoded;
}

} // namespace nettool
```

**include/nettool/http_client.hpp**：

```cpp
#pragma once

#include "nettool/url_parser.hpp"
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <chrono>
#include <variant>

namespace nettool {

/**
 * @brief HTTP方法
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS
};

/**
 * @brief HTTP响应
 */
struct HttpResponse {
    int status_code = 0;
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::string body;

    bool is_success() const { return status_code >= 200 && status_code < 300; }
    bool is_redirect() const { return status_code >= 300 && status_code < 400; }
    bool is_client_error() const { return status_code >= 400 && status_code < 500; }
    bool is_server_error() const { return status_code >= 500; }
};

/**
 * @brief HTTP请求配置
 */
struct HttpRequest {
    HttpMethod method = HttpMethod::GET;
    Url url;
    std::map<std::string, std::string> headers;
    std::string body;

    // 超时设置
    std::chrono::seconds connect_timeout{10};
    std::chrono::seconds read_timeout{30};

    // 是否跟随重定向
    bool follow_redirects = true;
    int max_redirects = 5;

    // 是否验证SSL证书
    bool verify_ssl = true;
};

/**
 * @brief HTTP客户端接口
 */
class HttpClient {
public:
    virtual ~HttpClient() = default;

    /**
     * @brief 发送HTTP请求
     */
    virtual HttpResponse send(const HttpRequest& request) = 0;

    /**
     * @brief 便捷GET请求
     */
    HttpResponse get(const std::string& url);
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers);

    /**
     * @brief 便捷POST请求
     */
    HttpResponse post(const std::string& url, const std::string& body);
    HttpResponse post(const std::string& url, const std::string& body,
                      const std::map<std::string, std::string>& headers);

    /**
     * @brief 创建默认HTTP客户端
     */
    static std::unique_ptr<HttpClient> create();
};

/**
 * @brief 基于Boost.Beast的HTTP客户端实现
 */
class BeastHttpClient : public HttpClient {
public:
    BeastHttpClient();
    ~BeastHttpClient() override;

    HttpResponse send(const HttpRequest& request) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nettool
```

**tests/test_url_parser.cpp**：

```cpp
#include <gtest/gtest.h>
#include <nettool/url_parser.hpp>

using namespace nettool;

class UrlParserTest : public ::testing::Test {};

TEST_F(UrlParserTest, ParseSimpleUrl) {
    auto url = UrlParser::parse("https://www.example.com/path");

    EXPECT_EQ(url.scheme, "https");
    EXPECT_EQ(url.host, "www.example.com");
    EXPECT_EQ(url.port, 443);
    EXPECT_EQ(url.path, "/path");
    EXPECT_TRUE(url.query.empty());
    EXPECT_TRUE(url.fragment.empty());
}

TEST_F(UrlParserTest, ParseUrlWithPort) {
    auto url = UrlParser::parse("http://localhost:8080/api/v1");

    EXPECT_EQ(url.scheme, "http");
    EXPECT_EQ(url.host, "localhost");
    EXPECT_EQ(url.port, 8080);
    EXPECT_EQ(url.path, "/api/v1");
}

TEST_F(UrlParserTest, ParseUrlWithQuery) {
    auto url = UrlParser::parse("https://api.example.com/search?q=hello&page=1");

    EXPECT_EQ(url.host, "api.example.com");
    EXPECT_EQ(url.path, "/search");
    EXPECT_EQ(url.query, "q=hello&page=1");
}

TEST_F(UrlParserTest, ParseUrlWithFragment) {
    auto url = UrlParser::parse("https://docs.example.com/guide#section-1");

    EXPECT_EQ(url.path, "/guide");
    EXPECT_EQ(url.fragment, "section-1");
}

TEST_F(UrlParserTest, ParseCompleteUrl) {
    auto url = UrlParser::parse(
        "https://api.example.com:8443/v2/users?active=true#top");

    EXPECT_EQ(url.scheme, "https");
    EXPECT_EQ(url.host, "api.example.com");
    EXPECT_EQ(url.port, 8443);
    EXPECT_EQ(url.path, "/v2/users");
    EXPECT_EQ(url.query, "active=true");
    EXPECT_EQ(url.fragment, "top");
}

TEST_F(UrlParserTest, InvalidUrl) {
    EXPECT_THROW(UrlParser::parse("not-a-url"), UrlParseError);
    EXPECT_THROW(UrlParser::parse("://missing-scheme.com"), UrlParseError);
}

TEST_F(UrlParserTest, TryParse) {
    auto result = UrlParser::try_parse("invalid");
    EXPECT_FALSE(result.has_value());

    result = UrlParser::try_parse("https://valid.com");
    EXPECT_TRUE(result.has_value());
}

TEST_F(UrlParserTest, UrlEncode) {
    EXPECT_EQ(UrlParser::encode("hello world"), "hello%20world");
    EXPECT_EQ(UrlParser::encode("foo=bar&baz"), "foo%3Dbar%26baz");
    EXPECT_EQ(UrlParser::encode("test_value-123"), "test_value-123");
}

TEST_F(UrlParserTest, UrlDecode) {
    EXPECT_EQ(UrlParser::decode("hello%20world"), "hello world");
    EXPECT_EQ(UrlParser::decode("hello+world"), "hello world");
    EXPECT_EQ(UrlParser::decode("foo%3Dbar%26baz"), "foo=bar&baz");
}

TEST_F(UrlParserTest, UrlToString) {
    Url url;
    url.scheme = "https";
    url.host = "example.com";
    url.port = 443;
    url.path = "/api";
    url.query = "key=value";

    EXPECT_EQ(url.to_string(), "https://example.com/api?key=value");

    url.port = 8443;
    EXPECT_EQ(url.to_string(), "https://example.com:8443/api?key=value");
}

TEST_F(UrlParserTest, DefaultPorts) {
    EXPECT_EQ(Url::default_port("http"), 80);
    EXPECT_EQ(Url::default_port("https"), 443);
    EXPECT_EQ(Url::default_port("ftp"), 21);
    EXPECT_EQ(Url::default_port("unknown"), 0);
}
```

---

## 检验标准

- [ ] 能够安装和配置vcpkg
- [ ] 理解Classic模式和Manifest模式的区别
- [ ] 能够使用vcpkg.json管理项目依赖
- [ ] 能够创建自定义port
- [ ] 能够配置二进制缓存加速构建
- [ ] 能够在CI/CD中使用vcpkg

### 知识检验问题

1. vcpkg的triplet是什么？如何创建自定义triplet？
2. vcpkg.json中的builtin-baseline有什么作用？
3. 如何在CI环境中使用vcpkg的二进制缓存？
4. VCPKG_TARGET_TRIPLET和VCPKG_HOST_TRIPLET的区别是什么？

---

## 输出物清单

1. **项目代码**
   - `network-toolkit/` - 完整的示例项目
   - vcpkg.json配置文件
   - CMakePresets.json

2. **自定义Port**
   - `ports/nettool/` - 自己库的vcpkg port

3. **文档**
   - `notes/month38_vcpkg.md` - 学习笔记
   - `notes/vcpkg_cheatsheet.md` - 常用命令速查

4. **脚本**
   - `scripts/setup-vcpkg.sh` - vcpkg安装脚本
   - `scripts/build.sh` - 跨平台构建脚本

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | vcpkg基础入门 | 15h | 15h | 5h |
| 第2周 | Manifest模式 | 12h | 18h | 5h |
| 第3周 | 创建自定义Port | 10h | 20h | 5h |
| 第4周 | 高级特性 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 39将学习**Conan包管理器**，掌握另一个流行的C++包管理工具，对比vcpkg理解不同的设计理念。
