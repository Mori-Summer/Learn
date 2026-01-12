# Month 50: 插件化系统设计 (Plugin System Design)

## 本月主题概述

插件化系统是实现软件可扩展性的核心技术，允许在不修改主程序的情况下动态添加、更新和移除功能。本月将深入学习插件系统的设计模式、动态加载机制、版本管理以及安全隔离策略，并构建一个生产级的插件框架。

### 学习目标
- 掌握动态链接库的加载与符号解析机制
- 理解插件生命周期管理与依赖解析
- 实现跨平台的插件加载框架
- 掌握插件热更新与版本兼容性策略
- 了解插件沙箱与安全隔离技术

---

## 理论学习内容

### 第一周：动态链接基础

#### 阅读材料
1. 《程序员的自我修养：链接、装载与库》- 动态链接章节
2. 《Computer Systems: A Programmer's Perspective》- Linking章节
3. Linux dlopen/dlsym手册
4. Windows LoadLibrary文档

#### 核心概念

**动态链接流程**
```
┌─────────────────────────────────────────────────────────┐
│                    程序启动                              │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│           动态链接器 (ld.so / dyld)                      │
│  1. 加载可执行文件                                       │
│  2. 解析依赖的共享库                                     │
│  3. 递归加载所有依赖                                     │
│  4. 符号重定位                                          │
│  5. 执行初始化函数                                       │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                   运行时加载                             │
│  dlopen() ─────▶ 加载共享库到进程空间                    │
│  dlsym()  ─────▶ 查找并返回符号地址                      │
│  dlclose() ────▶ 卸载共享库                             │
└─────────────────────────────────────────────────────────┘
```

**符号可见性控制**
```cpp
// 控制符号导出（Linux/macOS）
#if defined(__GNUC__) || defined(__clang__)
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
    #define PLUGIN_LOCAL  __attribute__((visibility("hidden")))
#elif defined(_MSC_VER)
    #define PLUGIN_EXPORT __declspec(dllexport)
    #define PLUGIN_LOCAL
#endif

// 编译时使用 -fvisibility=hidden 隐藏所有符号
// 只有标记 PLUGIN_EXPORT 的符号才会导出
```

### 第二周：插件架构模式

#### 阅读材料
1. 《Plugin Architecture》- Martin Fowler
2. Eclipse插件架构文档
3. VSCode Extension API设计
4. Qt插件系统文档

#### 核心概念

**插件架构分层**
```
┌─────────────────────────────────────────────────────────┐
│                    应用层 (Application)                  │
│    使用插件提供的功能，不直接依赖具体插件实现              │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  插件管理器 (Plugin Manager)             │
│    发现、加载、卸载、版本管理、依赖解析                   │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  插件接口层 (Plugin Interface)           │
│    定义插件契约，稳定的API边界                           │
└─────────────────────────────────────────────────────────┘
                          │
         ┌────────────────┼────────────────┐
         ▼                ▼                ▼
┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│  Plugin A   │   │  Plugin B   │   │  Plugin C   │
│  (.so/.dll) │   │  (.so/.dll) │   │  (.so/.dll) │
└─────────────┘   └─────────────┘   └─────────────┘
```

**插件元数据设计**
```json
{
    "id": "com.example.myplugin",
    "name": "My Plugin",
    "version": "1.2.0",
    "apiVersion": "2.0",
    "description": "A sample plugin",
    "author": "Developer Name",
    "license": "MIT",
    "entryPoint": "libmyplugin.so",
    "dependencies": [
        {"id": "com.example.core", "version": ">=1.0.0"},
        {"id": "com.example.utils", "version": "^2.0.0", "optional": true}
    ],
    "extensionPoints": [
        {"id": "editor.syntax", "handler": "SyntaxHandler"}
    ],
    "activationEvents": [
        "onCommand:myplugin.start",
        "onLanguage:python"
    ]
}
```

### 第三周：版本管理与兼容性

#### 阅读材料
1. Semantic Versioning 2.0.0规范
2. API版本化最佳实践
3. ABI稳定性指南
4. COM接口版本化策略

#### 核心概念

**语义化版本**
```
MAJOR.MINOR.PATCH

MAJOR: 不兼容的API变更
MINOR: 向后兼容的功能新增
PATCH: 向后兼容的问题修复

版本约束示例：
  "^1.2.3"  匹配 >=1.2.3, <2.0.0
  "~1.2.3"  匹配 >=1.2.3, <1.3.0
  ">=1.0.0" 匹配 >=1.0.0
  "1.2.x"   匹配 >=1.2.0, <1.3.0
```

**ABI兼容性检查清单**
```cpp
// ABI破坏性变更示例
class Widget {
    // ❌ 添加虚函数会改变vtable布局
    // virtual void newMethod();

    // ❌ 改变成员变量会改变对象大小
    // int newMember_;

    // ❌ 改变虚函数顺序
    // virtual void methodB();  // 原来methodA在前
    // virtual void methodA();

    // ✅ 安全：添加非虚函数
    void safeMethod();

    // ✅ 安全：添加静态成员
    static void staticMethod();

    // ✅ 安全：添加友元
    friend class Helper;
};

// PIMPL模式保持ABI稳定
class StableWidget {
public:
    StableWidget();
    ~StableWidget();

    void doSomething();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;  // 实现细节隐藏
};
```

### 第四周：安全隔离与沙箱

#### 阅读材料
1. Chromium沙箱设计文档
2. WebAssembly安全模型
3. Java SecurityManager架构
4. Linux namespaces/cgroups文档

#### 核心概念

**插件隔离策略**
```
安全级别从低到高：

Level 0: 无隔离（同进程）
┌─────────────────────────────────┐
│         主进程                   │
│  ┌────────┐ ┌────────┐         │
│  │Plugin A│ │Plugin B│  共享内存│
│  └────────┘ └────────┘         │
└─────────────────────────────────┘

Level 1: 线程隔离
┌─────────────────────────────────┐
│         主进程                   │
│  Thread 1    Thread 2           │
│  ┌────────┐ ┌────────┐         │
│  │Plugin A│ │Plugin B│ 独立栈   │
│  └────────┘ └────────┘         │
└─────────────────────────────────┘

Level 2: 进程隔离
┌──────────────┐  IPC  ┌──────────────┐
│   主进程     │◄────►│ 插件进程      │
│              │       │ ┌──────────┐ │
│              │       │ │ Plugin A │ │
│              │       │ └──────────┘ │
└──────────────┘       └──────────────┘

Level 3: 沙箱进程
┌──────────────┐       ┌──────────────┐
│   主进程     │       │ 沙箱进程      │
│              │  IPC  │ ┌──────────┐ │
│              │◄────►│ │ Plugin   │ │
│              │       │ │ 受限权限  │ │
│              │       │ └──────────┘ │
└──────────────┘       └──────────────┘
                       • 无文件系统访问
                       • 无网络访问
                       • 受限内存
```

---

## 源码阅读任务

### 必读项目

1. **LLVM Plugin系统** (https://github.com/llvm/llvm-project)
   - 重点文件：`llvm/include/llvm/Pass.h`, `llvm/lib/Passes/`
   - 学习目标：理解编译器插件架构
   - 阅读时间：10小时

2. **Qt插件系统** (https://github.com/qt/qtbase)
   - 重点文件：`src/corelib/plugin/`
   - 学习目标：理解跨平台插件加载
   - 阅读时间：8小时

3. **Neovim插件系统** (https://github.com/neovim/neovim)
   - 重点：RPC插件通信机制
   - 学习目标：理解进程隔离插件
   - 阅读时间：6小时

---

## 实践项目：跨平台插件框架

### 项目概述
构建一个功能完整的跨平台插件框架，支持动态加载、版本管理、热更新和安全隔离。

### 完整代码实现

#### 1. 跨平台动态库加载器 (plugin/loader/dynamic_library.hpp)

```cpp
#pragma once

#include <string>
#include <stdexcept>
#include <filesystem>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    using LibraryHandle = HMODULE;
#else
    #include <dlfcn.h>
    using LibraryHandle = void*;
#endif

namespace plugin {

class DynamicLibraryError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DynamicLibrary {
private:
    LibraryHandle handle_{nullptr};
    std::filesystem::path path_;

    static std::string getLastError() {
#ifdef _WIN32
        DWORD error = GetLastError();
        if (error == 0) return "";

        LPSTR buffer = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&buffer, 0, NULL
        );

        std::string message(buffer, size);
        LocalFree(buffer);
        return message;
#else
        const char* error = dlerror();
        return error ? error : "";
#endif
    }

public:
    DynamicLibrary() = default;

    explicit DynamicLibrary(const std::filesystem::path& path) {
        load(path);
    }

    ~DynamicLibrary() {
        unload();
    }

    // 禁止拷贝
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // 允许移动
    DynamicLibrary(DynamicLibrary&& other) noexcept
        : handle_(other.handle_), path_(std::move(other.path_)) {
        other.handle_ = nullptr;
    }

    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept {
        if (this != &other) {
            unload();
            handle_ = other.handle_;
            path_ = std::move(other.path_);
            other.handle_ = nullptr;
        }
        return *this;
    }

    void load(const std::filesystem::path& path) {
        if (handle_) {
            throw DynamicLibraryError("Library already loaded");
        }

        path_ = path;

#ifdef _WIN32
        handle_ = LoadLibraryW(path.wstring().c_str());
#else
        handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif

        if (!handle_) {
            throw DynamicLibraryError(
                "Failed to load library: " + path.string() +
                " - " + getLastError()
            );
        }
    }

    void unload() {
        if (handle_) {
#ifdef _WIN32
            FreeLibrary(handle_);
#else
            dlclose(handle_);
#endif
            handle_ = nullptr;
        }
    }

    template<typename T>
    T getSymbol(const std::string& name) const {
        if (!handle_) {
            throw DynamicLibraryError("Library not loaded");
        }

#ifdef _WIN32
        void* symbol = reinterpret_cast<void*>(
            GetProcAddress(handle_, name.c_str())
        );
#else
        void* symbol = dlsym(handle_, name.c_str());
#endif

        if (!symbol) {
            throw DynamicLibraryError(
                "Symbol not found: " + name + " - " + getLastError()
            );
        }

        return reinterpret_cast<T>(symbol);
    }

    template<typename T>
    std::optional<T> tryGetSymbol(const std::string& name) const noexcept {
        try {
            return getSymbol<T>(name);
        } catch (...) {
            return std::nullopt;
        }
    }

    bool isLoaded() const { return handle_ != nullptr; }
    const std::filesystem::path& getPath() const { return path_; }
};

// 获取平台特定的库文件扩展名
inline std::string getLibraryExtension() {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

// 获取平台特定的库文件前缀
inline std::string getLibraryPrefix() {
#ifdef _WIN32
    return "";
#else
    return "lib";
#endif
}

} // namespace plugin
```

#### 2. 插件接口定义 (plugin/core/plugin_interface.hpp)

```cpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <any>
#include <functional>
#include <optional>

namespace plugin {

// 版本结构
struct Version {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    std::string prerelease;

    std::string toString() const {
        std::string result = std::to_string(major) + "." +
                            std::to_string(minor) + "." +
                            std::to_string(patch);
        if (!prerelease.empty()) {
            result += "-" + prerelease;
        }
        return result;
    }

    static Version parse(const std::string& str) {
        Version v;
        std::sscanf(str.c_str(), "%u.%u.%u", &v.major, &v.minor, &v.patch);
        auto pos = str.find('-');
        if (pos != std::string::npos) {
            v.prerelease = str.substr(pos + 1);
        }
        return v;
    }

    bool operator<(const Version& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        if (patch != other.patch) return patch < other.patch;
        if (prerelease.empty() && !other.prerelease.empty()) return false;
        if (!prerelease.empty() && other.prerelease.empty()) return true;
        return prerelease < other.prerelease;
    }

    bool operator==(const Version& other) const {
        return major == other.major && minor == other.minor &&
               patch == other.patch && prerelease == other.prerelease;
    }

    bool operator<=(const Version& other) const {
        return *this < other || *this == other;
    }

    bool isCompatibleWith(const Version& required) const {
        // 主版本必须匹配，次版本必须>=
        return major == required.major &&
               (minor > required.minor ||
                (minor == required.minor && patch >= required.patch));
    }
};

// 依赖描述
struct Dependency {
    std::string id;
    Version minVersion;
    Version maxVersion;
    bool optional = false;
};

// 插件元数据
struct PluginMetadata {
    std::string id;
    std::string name;
    Version version;
    Version apiVersion;
    std::string description;
    std::string author;
    std::string license;
    std::vector<Dependency> dependencies;
    std::vector<std::string> providedExtensions;
};

// 前向声明
class IPluginContext;

// 插件接口 - 所有插件必须实现
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // 获取插件元数据
    virtual const PluginMetadata& getMetadata() const = 0;

    // 生命周期方法
    virtual bool initialize(IPluginContext* context) = 0;
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;
    virtual void dispose() = 0;

    // 配置
    virtual bool configure(const std::any& config) = 0;

    // 健康检查
    virtual bool isHealthy() const = 0;
};

// 插件上下文 - 提供给插件的宿主服务
class IPluginContext {
public:
    virtual ~IPluginContext() = default;

    // 日志
    virtual void log(const std::string& level, const std::string& message) = 0;

    // 获取其他插件
    virtual IPlugin* getPlugin(const std::string& id) = 0;

    // 扩展点注册
    virtual void registerExtension(const std::string& extensionPoint,
                                   const std::string& id,
                                   std::any extension) = 0;

    // 获取扩展
    virtual std::vector<std::any> getExtensions(
        const std::string& extensionPoint) = 0;

    // 事件发布
    virtual void publishEvent(const std::string& event,
                             const std::any& data) = 0;

    // 事件订阅
    virtual uint64_t subscribeEvent(const std::string& event,
        std::function<void(const std::any&)> handler) = 0;

    virtual void unsubscribeEvent(uint64_t subscriptionId) = 0;

    // 配置访问
    virtual std::optional<std::any> getConfig(const std::string& key) = 0;
};

// 插件工厂函数类型
using PluginFactory = IPlugin* (*)();
using PluginDestroyer = void (*)(IPlugin*);

// 插件导出宏
#define DECLARE_PLUGIN(PluginClass) \
    extern "C" { \
        PLUGIN_EXPORT ::plugin::IPlugin* createPlugin() { \
            return new PluginClass(); \
        } \
        PLUGIN_EXPORT void destroyPlugin(::plugin::IPlugin* plugin) { \
            delete plugin; \
        } \
        PLUGIN_EXPORT const char* getPluginApiVersion() { \
            return "2.0.0"; \
        } \
    }

} // namespace plugin
```

#### 3. 插件管理器 (plugin/core/plugin_manager.hpp)

```cpp
#pragma once

#include "plugin_interface.hpp"
#include "../loader/dynamic_library.hpp"
#include <map>
#include <set>
#include <queue>
#include <shared_mutex>
#include <filesystem>

namespace plugin {

// 插件状态
enum class PluginState {
    Discovered,
    Loaded,
    Initialized,
    Active,
    Inactive,
    Error,
    Unloaded
};

// 加载的插件信息
struct LoadedPlugin {
    std::unique_ptr<DynamicLibrary> library;
    std::unique_ptr<IPlugin, PluginDestroyer> instance;
    PluginState state = PluginState::Discovered;
    std::string errorMessage;
    std::filesystem::file_time_type lastModified;
};

// 插件管理器配置
struct PluginManagerConfig {
    std::vector<std::filesystem::path> searchPaths;
    Version hostApiVersion{2, 0, 0};
    bool enableHotReload = false;
    std::chrono::milliseconds hotReloadCheckInterval{5000};
};

class PluginManager : public IPluginContext {
private:
    PluginManagerConfig config_;
    std::map<std::string, LoadedPlugin> plugins_;
    mutable std::shared_mutex pluginsMutex_;

    // 扩展点注册表
    std::map<std::string, std::map<std::string, std::any>> extensions_;
    mutable std::shared_mutex extensionsMutex_;

    // 事件系统
    struct EventSubscription {
        uint64_t id;
        std::function<void(const std::any&)> handler;
    };
    std::map<std::string, std::vector<EventSubscription>> eventSubscriptions_;
    std::atomic<uint64_t> subscriptionIdGen_{0};
    mutable std::mutex eventMutex_;

    // 配置存储
    std::map<std::string, std::any> configStore_;
    mutable std::shared_mutex configMutex_;

    // 热重载监视线程
    std::atomic<bool> watcherRunning_{false};
    std::thread watcherThread_;

    // 发现插件
    std::vector<std::filesystem::path> discoverPlugins() {
        std::vector<std::filesystem::path> found;

        for (const auto& searchPath : config_.searchPaths) {
            if (!std::filesystem::exists(searchPath)) continue;

            for (const auto& entry :
                 std::filesystem::directory_iterator(searchPath)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();
                if (ext == getLibraryExtension()) {
                    found.push_back(entry.path());
                }
            }
        }

        return found;
    }

    // 加载单个插件
    bool loadPlugin(const std::filesystem::path& path) {
        try {
            auto library = std::make_unique<DynamicLibrary>(path);

            // 检查API版本
            auto getApiVersion = library->tryGetSymbol<const char*(*)()>(
                "getPluginApiVersion");
            if (getApiVersion) {
                Version pluginApiVersion = Version::parse((*getApiVersion)());
                if (!config_.hostApiVersion.isCompatibleWith(pluginApiVersion)) {
                    std::cerr << "Plugin API version mismatch: " << path
                              << " (requires " << pluginApiVersion.toString()
                              << ", host provides "
                              << config_.hostApiVersion.toString() << ")"
                              << std::endl;
                    return false;
                }
            }

            // 获取工厂函数
            auto factory = library->getSymbol<PluginFactory>("createPlugin");
            auto destroyer = library->getSymbol<PluginDestroyer>("destroyPlugin");

            // 创建插件实例
            IPlugin* rawPlugin = factory();
            if (!rawPlugin) {
                throw std::runtime_error("Plugin factory returned null");
            }

            std::unique_ptr<IPlugin, PluginDestroyer> plugin(rawPlugin, destroyer);

            const auto& metadata = plugin->getMetadata();

            std::unique_lock lock(pluginsMutex_);

            // 检查是否已加载
            if (plugins_.count(metadata.id)) {
                std::cerr << "Plugin already loaded: " << metadata.id << std::endl;
                return false;
            }

            LoadedPlugin loaded;
            loaded.library = std::move(library);
            loaded.instance = std::move(plugin);
            loaded.state = PluginState::Loaded;
            loaded.lastModified = std::filesystem::last_write_time(path);

            plugins_[metadata.id] = std::move(loaded);

            std::cout << "Plugin loaded: " << metadata.name
                      << " v" << metadata.version.toString() << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Failed to load plugin " << path << ": "
                      << e.what() << std::endl;
            return false;
        }
    }

    // 依赖解析 - 拓扑排序
    std::vector<std::string> resolveDependencies() {
        std::map<std::string, std::set<std::string>> graph;
        std::map<std::string, int> inDegree;

        std::shared_lock lock(pluginsMutex_);

        // 构建依赖图
        for (const auto& [id, loaded] : plugins_) {
            if (!graph.count(id)) {
                graph[id] = {};
                inDegree[id] = 0;
            }

            for (const auto& dep : loaded.instance->getMetadata().dependencies) {
                if (!dep.optional || plugins_.count(dep.id)) {
                    graph[dep.id].insert(id);
                    inDegree[id]++;
                }
            }
        }

        // Kahn's算法
        std::queue<std::string> queue;
        for (const auto& [id, degree] : inDegree) {
            if (degree == 0) {
                queue.push(id);
            }
        }

        std::vector<std::string> result;
        while (!queue.empty()) {
            auto id = queue.front();
            queue.pop();
            result.push_back(id);

            for (const auto& dependent : graph[id]) {
                if (--inDegree[dependent] == 0) {
                    queue.push(dependent);
                }
            }
        }

        if (result.size() != plugins_.size()) {
            throw std::runtime_error("Circular dependency detected");
        }

        return result;
    }

    // 热重载监视循环
    void watcherLoop() {
        while (watcherRunning_) {
            std::this_thread::sleep_for(config_.hotReloadCheckInterval);

            std::vector<std::string> toReload;

            {
                std::shared_lock lock(pluginsMutex_);
                for (const auto& [id, loaded] : plugins_) {
                    if (!loaded.library) continue;

                    auto path = loaded.library->getPath();
                    if (!std::filesystem::exists(path)) continue;

                    auto currentModTime = std::filesystem::last_write_time(path);
                    if (currentModTime > loaded.lastModified) {
                        toReload.push_back(id);
                    }
                }
            }

            for (const auto& id : toReload) {
                std::cout << "Hot reloading plugin: " << id << std::endl;
                reloadPlugin(id);
            }
        }
    }

public:
    explicit PluginManager(PluginManagerConfig config = {})
        : config_(std::move(config)) {

        if (config_.searchPaths.empty()) {
            config_.searchPaths.push_back("./plugins");
        }
    }

    ~PluginManager() {
        stopHotReload();
        unloadAll();
    }

    // 发现并加载所有插件
    void loadAll() {
        auto paths = discoverPlugins();
        for (const auto& path : paths) {
            loadPlugin(path);
        }
    }

    // 初始化并激活所有插件
    bool activateAll() {
        try {
            auto order = resolveDependencies();

            // 初始化
            for (const auto& id : order) {
                std::unique_lock lock(pluginsMutex_);
                auto& loaded = plugins_[id];

                if (loaded.state != PluginState::Loaded) continue;

                lock.unlock();

                if (!loaded.instance->initialize(this)) {
                    std::cerr << "Failed to initialize plugin: " << id << std::endl;
                    loaded.state = PluginState::Error;
                    loaded.errorMessage = "Initialization failed";
                    continue;
                }

                loaded.state = PluginState::Initialized;
            }

            // 激活
            for (const auto& id : order) {
                std::unique_lock lock(pluginsMutex_);
                auto& loaded = plugins_[id];

                if (loaded.state != PluginState::Initialized) continue;

                lock.unlock();

                if (!loaded.instance->activate()) {
                    std::cerr << "Failed to activate plugin: " << id << std::endl;
                    loaded.state = PluginState::Error;
                    loaded.errorMessage = "Activation failed";
                    continue;
                }

                loaded.state = PluginState::Active;
                std::cout << "Plugin activated: " << id << std::endl;
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Failed to activate plugins: " << e.what() << std::endl;
            return false;
        }
    }

    // 停用并卸载所有插件
    void unloadAll() {
        std::vector<std::string> order;

        try {
            order = resolveDependencies();
            std::reverse(order.begin(), order.end());  // 逆序卸载
        } catch (...) {
            // 如果有循环依赖，按任意顺序卸载
            std::shared_lock lock(pluginsMutex_);
            for (const auto& [id, _] : plugins_) {
                order.push_back(id);
            }
        }

        for (const auto& id : order) {
            unloadPlugin(id);
        }
    }

    // 卸载单个插件
    bool unloadPlugin(const std::string& id) {
        std::unique_lock lock(pluginsMutex_);

        auto it = plugins_.find(id);
        if (it == plugins_.end()) return false;

        auto& loaded = it->second;

        // 检查是否有其他插件依赖此插件
        for (const auto& [otherId, other] : plugins_) {
            if (otherId == id) continue;

            for (const auto& dep : other.instance->getMetadata().dependencies) {
                if (dep.id == id && !dep.optional &&
                    other.state == PluginState::Active) {
                    std::cerr << "Cannot unload " << id
                              << ": plugin " << otherId
                              << " depends on it" << std::endl;
                    return false;
                }
            }
        }

        // 停用
        if (loaded.state == PluginState::Active) {
            loaded.instance->deactivate();
        }

        // 清理
        loaded.instance->dispose();

        // 移除
        plugins_.erase(it);

        std::cout << "Plugin unloaded: " << id << std::endl;
        return true;
    }

    // 重载插件
    bool reloadPlugin(const std::string& id) {
        std::filesystem::path path;

        {
            std::shared_lock lock(pluginsMutex_);
            auto it = plugins_.find(id);
            if (it == plugins_.end()) return false;
            path = it->second.library->getPath();
        }

        if (!unloadPlugin(id)) return false;

        // 稍等文件系统
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!loadPlugin(path)) return false;

        // 重新初始化和激活
        {
            std::unique_lock lock(pluginsMutex_);
            auto& loaded = plugins_[id];

            lock.unlock();

            if (!loaded.instance->initialize(this)) {
                loaded.state = PluginState::Error;
                return false;
            }
            loaded.state = PluginState::Initialized;

            if (!loaded.instance->activate()) {
                loaded.state = PluginState::Error;
                return false;
            }
            loaded.state = PluginState::Active;
        }

        publishEvent("plugin.reloaded", id);
        return true;
    }

    // 启动热重载
    void startHotReload() {
        if (!config_.enableHotReload || watcherRunning_) return;

        watcherRunning_ = true;
        watcherThread_ = std::thread(&PluginManager::watcherLoop, this);
    }

    // 停止热重载
    void stopHotReload() {
        if (!watcherRunning_) return;

        watcherRunning_ = false;
        if (watcherThread_.joinable()) {
            watcherThread_.join();
        }
    }

    // IPluginContext 实现
    void log(const std::string& level, const std::string& message) override {
        std::cout << "[" << level << "] " << message << std::endl;
    }

    IPlugin* getPlugin(const std::string& id) override {
        std::shared_lock lock(pluginsMutex_);
        auto it = plugins_.find(id);
        return it != plugins_.end() ? it->second.instance.get() : nullptr;
    }

    void registerExtension(const std::string& extensionPoint,
                          const std::string& id,
                          std::any extension) override {
        std::unique_lock lock(extensionsMutex_);
        extensions_[extensionPoint][id] = std::move(extension);
    }

    std::vector<std::any> getExtensions(
        const std::string& extensionPoint) override {
        std::shared_lock lock(extensionsMutex_);
        std::vector<std::any> result;

        auto it = extensions_.find(extensionPoint);
        if (it != extensions_.end()) {
            for (const auto& [_, ext] : it->second) {
                result.push_back(ext);
            }
        }

        return result;
    }

    void publishEvent(const std::string& event, const std::any& data) override {
        std::vector<std::function<void(const std::any&)>> handlers;

        {
            std::lock_guard lock(eventMutex_);
            auto it = eventSubscriptions_.find(event);
            if (it != eventSubscriptions_.end()) {
                for (const auto& sub : it->second) {
                    handlers.push_back(sub.handler);
                }
            }
        }

        for (const auto& handler : handlers) {
            try {
                handler(data);
            } catch (const std::exception& e) {
                log("ERROR", "Event handler error: " + std::string(e.what()));
            }
        }
    }

    uint64_t subscribeEvent(const std::string& event,
        std::function<void(const std::any&)> handler) override {
        std::lock_guard lock(eventMutex_);
        uint64_t id = ++subscriptionIdGen_;
        eventSubscriptions_[event].push_back({id, std::move(handler)});
        return id;
    }

    void unsubscribeEvent(uint64_t subscriptionId) override {
        std::lock_guard lock(eventMutex_);
        for (auto& [_, subs] : eventSubscriptions_) {
            subs.erase(
                std::remove_if(subs.begin(), subs.end(),
                    [subscriptionId](const EventSubscription& s) {
                        return s.id == subscriptionId;
                    }),
                subs.end()
            );
        }
    }

    std::optional<std::any> getConfig(const std::string& key) override {
        std::shared_lock lock(configMutex_);
        auto it = configStore_.find(key);
        return it != configStore_.end() ?
               std::optional(it->second) : std::nullopt;
    }

    // 设置配置
    void setConfig(const std::string& key, std::any value) {
        std::unique_lock lock(configMutex_);
        configStore_[key] = std::move(value);
    }

    // 获取插件状态
    std::map<std::string, PluginState> getPluginStates() const {
        std::shared_lock lock(pluginsMutex_);
        std::map<std::string, PluginState> states;
        for (const auto& [id, loaded] : plugins_) {
            states[id] = loaded.state;
        }
        return states;
    }
};

} // namespace plugin
```

#### 4. 示例插件实现 (plugins/sample_plugin/)

```cpp
// sample_plugin.hpp
#pragma once

#include "plugin/core/plugin_interface.hpp"
#include <iostream>
#include <thread>
#include <atomic>

// 必须定义导出宏
#if defined(_WIN32)
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace sample {

// 自定义扩展接口
class IGreeter {
public:
    virtual ~IGreeter() = default;
    virtual std::string greet(const std::string& name) = 0;
};

class SamplePlugin : public plugin::IPlugin {
private:
    plugin::PluginMetadata metadata_;
    plugin::IPluginContext* context_{nullptr};
    std::atomic<bool> running_{false};
    std::thread workerThread_;
    uint64_t eventSubscription_{0};

    class Greeter : public IGreeter {
    public:
        std::string greet(const std::string& name) override {
            return "Hello, " + name + "! From SamplePlugin.";
        }
    };

    std::shared_ptr<Greeter> greeter_;

    void workerLoop() {
        int counter = 0;
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (running_ && context_) {
                context_->publishEvent("sample.heartbeat", ++counter);
            }
        }
    }

public:
    SamplePlugin() {
        metadata_.id = "com.example.sample";
        metadata_.name = "Sample Plugin";
        metadata_.version = {1, 0, 0};
        metadata_.apiVersion = {2, 0, 0};
        metadata_.description = "A sample plugin demonstrating the plugin API";
        metadata_.author = "Developer";
        metadata_.license = "MIT";
        metadata_.providedExtensions = {"greeter"};
    }

    const plugin::PluginMetadata& getMetadata() const override {
        return metadata_;
    }

    bool initialize(plugin::IPluginContext* context) override {
        context_ = context;
        greeter_ = std::make_shared<Greeter>();

        context_->log("INFO", "SamplePlugin initializing...");

        // 订阅事件
        eventSubscription_ = context_->subscribeEvent("app.shutdown",
            [this](const std::any&) {
                context_->log("INFO", "SamplePlugin received shutdown event");
            });

        return true;
    }

    bool activate() override {
        if (!context_) return false;

        context_->log("INFO", "SamplePlugin activating...");

        // 注册扩展
        context_->registerExtension("greeter", metadata_.id, greeter_);

        // 启动后台工作
        running_ = true;
        workerThread_ = std::thread(&SamplePlugin::workerLoop, this);

        context_->log("INFO", "SamplePlugin activated");
        return true;
    }

    bool deactivate() override {
        if (context_) {
            context_->log("INFO", "SamplePlugin deactivating...");
        }

        running_ = false;
        if (workerThread_.joinable()) {
            workerThread_.join();
        }

        return true;
    }

    void dispose() override {
        if (context_ && eventSubscription_) {
            context_->unsubscribeEvent(eventSubscription_);
        }
        greeter_.reset();
        context_ = nullptr;
    }

    bool configure(const std::any& config) override {
        // 处理配置更新
        return true;
    }

    bool isHealthy() const override {
        return running_ && context_ != nullptr;
    }
};

} // namespace sample

// 声明插件导出
DECLARE_PLUGIN(sample::SamplePlugin)
```

#### 5. 主程序 (main.cpp)

```cpp
#include "plugin/core/plugin_manager.hpp"
#include <iostream>
#include <csignal>

std::unique_ptr<plugin::PluginManager> g_manager;

void signalHandler(int signal) {
    std::cout << "\nShutting down..." << std::endl;
    if (g_manager) {
        g_manager->publishEvent("app.shutdown", true);
        g_manager->stopHotReload();
        g_manager->unloadAll();
    }
    std::exit(0);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 配置插件管理器
    plugin::PluginManagerConfig config;
    config.searchPaths = {"./plugins", "/usr/local/lib/myapp/plugins"};
    config.enableHotReload = true;
    config.hotReloadCheckInterval = std::chrono::milliseconds(3000);

    g_manager = std::make_unique<plugin::PluginManager>(config);

    // 加载所有插件
    std::cout << "Discovering and loading plugins..." << std::endl;
    g_manager->loadAll();

    // 激活插件
    std::cout << "Activating plugins..." << std::endl;
    if (!g_manager->activateAll()) {
        std::cerr << "Some plugins failed to activate" << std::endl;
    }

    // 启动热重载
    g_manager->startHotReload();
    std::cout << "Hot reload enabled" << std::endl;

    // 显示已加载的插件
    std::cout << "\nLoaded plugins:" << std::endl;
    for (const auto& [id, state] : g_manager->getPluginStates()) {
        std::cout << "  - " << id << ": ";
        switch (state) {
            case plugin::PluginState::Active:
                std::cout << "Active"; break;
            case plugin::PluginState::Error:
                std::cout << "Error"; break;
            default:
                std::cout << "Other"; break;
        }
        std::cout << std::endl;
    }

    // 使用扩展
    auto extensions = g_manager->getExtensions("greeter");
    for (const auto& ext : extensions) {
        try {
            auto greeter = std::any_cast<
                std::shared_ptr<sample::IGreeter>>(ext);
            std::cout << greeter->greet("World") << std::endl;
        } catch (...) {}
    }

    // 主循环
    std::cout << "\nPress Ctrl+C to exit" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

---

## 检验标准

### 知识检验
1. [ ] 能够解释动态链接的工作原理
2. [ ] 理解符号可见性与导出控制
3. [ ] 掌握插件版本兼容性策略
4. [ ] 理解ABI稳定性的重要性及保持方法
5. [ ] 了解插件沙箱隔离的不同级别

### 实践检验
1. [ ] 完成跨平台插件加载器实现
2. [ ] 插件能够正确加载、初始化和卸载
3. [ ] 依赖解析正确处理循环依赖
4. [ ] 热重载功能正常工作
5. [ ] 实现至少2个功能插件

### 代码质量
1. [ ] 跨Windows/Linux/macOS编译通过
2. [ ] 无内存泄漏和资源泄漏
3. [ ] 异常安全，错误处理完善
4. [ ] 接口设计清晰，文档完整

---

## 输出物清单

1. **学习笔记**
   - [ ] 动态链接原理笔记
   - [ ] 各平台动态库对比文档
   - [ ] 源码阅读笔记

2. **代码产出**
   - [ ] 跨平台插件框架
   - [ ] 示例插件集合
   - [ ] 单元测试

3. **文档产出**
   - [ ] 插件开发指南
   - [ ] API参考文档
   - [ ] 版本兼容性指南

---

## 时间分配表

| 周次 | 理论学习 | 源码阅读 | 项目实践 | 总计 |
|------|----------|----------|----------|------|
| Week 1 | 15h | 8h | 12h | 35h |
| Week 2 | 12h | 8h | 15h | 35h |
| Week 3 | 10h | 6h | 19h | 35h |
| Week 4 | 8h | 2h | 25h | 35h |
| **总计** | **45h** | **24h** | **71h** | **140h** |

---

## 下月预告

**Month 51: 面向数据设计（DOD）基础**

下个月将学习一种与面向对象截然不同的设计范式：
- 数据局部性与缓存优化
- AoS vs SoA数据布局
- 热/冷数据分离
- 批处理与SIMD友好设计
- 实践项目：高性能粒子系统

建议提前：
1. 复习计算机体系结构缓存知识
2. 了解CPU缓存行的工作原理
3. 学习基本的性能分析工具（perf, VTune）
