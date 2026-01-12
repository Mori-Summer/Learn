# Month 58: std::execution与异步执行框架

## 本月主题概述

std::execution是C++26的重要提案，定义了一个统一的异步编程框架。它基于Sender/Receiver概念，提供了组合异步操作的标准化方式。本月将深入学习这个框架的设计理念、核心概念和实际应用，并通过构建异步任务系统来实践所学知识。

### 学习目标
- 理解Sender/Receiver编程模型
- 掌握执行上下文和调度器概念
- 学习结构化并发设计模式
- 理解取消机制的实现
- 构建可组合的异步任务系统

---

## 理论学习内容

### 第一周：Sender/Receiver模型

#### 阅读材料
1. P2300: std::execution提案
2. libunifex文档
3. Structured Concurrency论文
4. Eric Niebler的相关博客

#### 核心概念

**异步编程模型对比**
```
┌─────────────────────────────────────────────────────────┐
│              异步编程模型演进                            │
└─────────────────────────────────────────────────────────┘

1. 回调 (Callbacks):
   asyncOp(input, [](Result r) {
       asyncOp2(r, [](Result r2) {
           // 回调地狱...
       });
   });
   问题：嵌套、错误处理困难、难以取消

2. Promise/Future:
   auto future = asyncOp(input);
   auto future2 = future.then([](Result r) {
       return asyncOp2(r);
   });
   问题：仍需堆分配、取消困难、组合有限

3. 协程 (Coroutines):
   co_await asyncOp(input);
   co_await asyncOp2(result);
   优点：线性代码流、异常传播
   问题：堆分配、难以组合

4. Sender/Receiver (std::execution):
   auto sender = asyncOp(input)
               | then([](auto r) { return asyncOp2(r); })
               | on(scheduler);
   优点：零开销抽象、可组合、支持取消
```

**Sender/Receiver概念**
```cpp
// Sender: 描述异步操作的惰性值
// - 不会立即执行
// - 可以被连接到Receiver
// - 支持组合

// Receiver: 接收异步操作结果的处理器
// - set_value(Values...): 成功完成
// - set_error(Error): 错误完成
// - set_stopped(): 被取消

// Operation State: Sender连接到Receiver后的状态
// - 持有操作所需的所有资源
// - start()方法启动操作

// 概念定义（简化）
template<typename S>
concept Sender = requires(S&& s) {
    { get_completion_signatures(s) };
};

template<typename R, typename... Values>
concept Receiver = requires(R&& r, Values&&... values) {
    { std::move(r).set_value(std::forward<Values>(values)...) };
    { std::move(r).set_error(std::exception_ptr{}) };
    { std::move(r).set_stopped() };
};

// 基本流程
//
// Sender ──connect──▶ Receiver
//           │
//           ▼
//     OperationState ──start──▶ 执行
//                                │
//              ┌─────────────────┼─────────────────┐
//              ▼                 ▼                 ▼
//        set_value()      set_error()       set_stopped()
```

**基本使用示例**
```cpp
#include <execution>  // C++26

namespace ex = std::execution;

void basicExample() {
    // 1. just: 创建立即完成的sender
    auto s1 = ex::just(42);

    // 2. then: 变换sender的值
    auto s2 = s1 | ex::then([](int x) { return x * 2; });

    // 3. 同步等待结果
    int result = ex::sync_wait(s2).value();  // 84

    // 4. 链式调用
    auto pipeline = ex::just(10)
                  | ex::then([](int x) { return x + 5; })
                  | ex::then([](int x) { return std::to_string(x); });

    std::string str = ex::sync_wait(pipeline).value();  // "15"

    // 5. 错误处理
    auto mayFail = ex::just()
                 | ex::then([]() -> int {
                       throw std::runtime_error("oops");
                   })
                 | ex::upon_error([](std::exception_ptr ep) {
                       return -1;  // 错误时返回-1
                   });
}
```

### 第二周：调度器和执行上下文

#### 阅读材料
1. 线程池设计模式
2. Work Stealing算法
3. io_uring异步IO
4. Windows IOCP文档

#### 核心概念

**调度器（Scheduler）**
```cpp
// 调度器决定操作在哪里执行

namespace ex = std::execution;

// 获取调度器
auto inline_scheduler = ex::inline_scheduler{};  // 立即执行
auto thread_pool = /* ... */;
auto pool_scheduler = thread_pool.get_scheduler();

// 使用调度器
auto work = ex::schedule(pool_scheduler)   // 在池中调度
          | ex::then([]() { return 42; }); // 在池中执行

// transfer: 切换执行上下文
auto pipeline = ex::just(42)
              | ex::then([](int x) { return x * 2; })  // 在当前上下文
              | ex::transfer(pool_scheduler)           // 切换到池
              | ex::then([](int x) {                   // 在池中执行
                    return heavyComputation(x);
                })
              | ex::transfer(main_scheduler);          // 回到主线程

// on: 指定整个sender的执行上下文
auto work2 = ex::on(pool_scheduler,
                   ex::just(42) | ex::then(process));

// continues_on: 后续操作切换上下文
auto work3 = ex::just(42)
           | ex::continues_on(pool_scheduler)
           | ex::then(process);  // 在池中
```

**执行上下文类型**
```cpp
// 1. 内联执行器：立即在当前线程执行
struct InlineScheduler {
    struct Sender {
        template<typename Receiver>
        auto connect(Receiver&& r) {
            return OperationState{std::forward<Receiver>(r)};
        }
    };

    auto schedule() noexcept { return Sender{}; }
};

// 2. 线程池：在池中的线程执行
class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    class Scheduler {
        ThreadPool* pool_;
    public:
        // schedule()返回一个sender
        auto schedule() noexcept;
    };

    Scheduler get_scheduler() { return Scheduler{this}; }

    void submit(std::function<void()> task) {
        {
            std::lock_guard lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }
};

// 3. IO执行器：用于异步IO
class IOContext {
    // 基于io_uring或IOCP
    auto get_scheduler();
};

// 4. 定时器执行器
class TimerContext {
    auto schedule_after(std::chrono::duration d);
    auto schedule_at(std::chrono::time_point t);
};
```

### 第三周：组合操作

#### 阅读材料
1. 算法组合模式
2. 异步流处理
3. 反应式编程概念
4. RxCpp文档

#### 核心概念

**并发组合**
```cpp
namespace ex = std::execution;

// when_all: 等待所有sender完成
auto s1 = ex::just(1);
auto s2 = ex::just(2);
auto s3 = ex::just(3);

auto all = ex::when_all(s1, s2, s3)
         | ex::then([](int a, int b, int c) {
               return a + b + c;  // 6
           });

// when_any: 等待任一完成（取消其他）
auto fastest = ex::when_any(
    longComputation1(),
    longComputation2()
);

// let_value: 动态创建后续sender
auto dynamic = ex::just(42)
             | ex::let_value([](int x) {
                   if (x > 0) {
                       return ex::just(x * 2);
                   } else {
                       return ex::just(0);
                   }
               });

// bulk: 并行执行多次
auto parallel = ex::just(std::vector<int>{1, 2, 3, 4, 5})
              | ex::bulk(5, [](size_t i, std::vector<int>& v) {
                    v[i] *= 2;  // 并行处理每个元素
                });

// split: 将sender分裂为可多次使用
auto shared = ex::just(expensiveComputation())
            | ex::split();  // 可以连接多个receiver

auto use1 = shared | ex::then(process1);
auto use2 = shared | ex::then(process2);
```

**错误处理**
```cpp
namespace ex = std::execution;

// upon_error: 处理错误
auto handled = ex::just()
             | ex::then([]() -> int {
                   throw std::runtime_error("error");
               })
             | ex::upon_error([](std::exception_ptr ep) {
                   return -1;  // 错误时返回默认值
               });

// let_error: 错误时创建新sender
auto recovered = mayFail()
               | ex::let_error([](std::exception_ptr ep) {
                     // 尝试恢复
                     return fallbackOperation();
                 });

// stopped_as_optional: 取消变为empty optional
auto optional = interruptible()
              | ex::stopped_as_optional();

// stopped_as_error: 取消变为错误
auto error = interruptible()
           | ex::stopped_as_error<std::runtime_error>();
```

### 第四周：取消和结构化并发

#### 阅读材料
1. 取消令牌设计
2. 结构化并发原则
3. Trio库文档
4. Swift结构化并发

#### 核心概念

**停止令牌（Stop Token）**
```cpp
// stop_source: 发起取消请求
// stop_token: 检查是否被请求取消
// stop_callback: 注册取消回调

std::stop_source source;
std::stop_token token = source.get_token();

// 注册回调
std::stop_callback callback(token, []() {
    std::cout << "Cancelled!\n";
});

// 请求取消
source.request_stop();

// 检查取消状态
if (token.stop_requested()) {
    // 处理取消
}

// 在sender中使用
namespace ex = std::execution;

auto cancellable = ex::just()
                 | ex::let_value([](auto&&) {
                       return ex::get_stop_token()
                            | ex::then([](auto token) {
                                  while (!token.stop_requested()) {
                                      // 可取消的循环
                                  }
                              });
                   });
```

**结构化并发**
```cpp
namespace ex = std::execution;

// 结构化并发原则：
// 1. 所有并发操作有明确的作用域
// 2. 子操作的生命周期不超过父操作
// 3. 错误和取消正确传播

// async_scope: 管理一组异步操作
ex::async_scope scope;

// 在scope中启动操作
scope.spawn(asyncOperation1());
scope.spawn(asyncOperation2());

// 等待所有完成
ex::sync_wait(scope.on_empty());

// nest: 嵌套sender的生命周期
auto nested = ex::nest(
    scope.get_scheduler(),
    ex::when_all(op1, op2, op3)
);

// 使用with确保资源清理
auto withResource = ex::just()
                  | ex::let_value([&]() {
                        return ex::use(
                            acquireResource(),
                            [](auto& resource) {
                                return useResource(resource);
                            }
                        );  // 自动释放resource
                    });
```

---

## 源码阅读任务

### 必读项目

1. **libunifex** (https://github.com/facebookexperimental/libunifex)
   - Facebook的std::execution实现
   - 学习目标：理解实际实现
   - 阅读时间：15小时

2. **stdexec** (https://github.com/NVIDIA/stdexec)
   - NVIDIA的参考实现
   - 学习目标：理解最新设计
   - 阅读时间：12小时

3. **Seastar** (https://github.com/scylladb/seastar)
   - 高性能异步框架
   - 学习目标：理解工业级设计
   - 阅读时间：8小时

---

## 实践项目：异步任务系统

### 项目概述
实现一个简化的std::execution兼容的异步任务系统，支持基本的组合操作。

### 完整代码实现

#### 1. 基础类型定义 (execution/types.hpp)

```cpp
#pragma once

#include <concepts>
#include <type_traits>
#include <exception>
#include <optional>
#include <variant>

namespace exec {

// 完成信号标记
struct set_value_t {
    template<typename R, typename... Vs>
    void operator()(R&& r, Vs&&... vs) const {
        std::forward<R>(r).set_value(std::forward<Vs>(vs)...);
    }
};

struct set_error_t {
    template<typename R, typename E>
    void operator()(R&& r, E&& e) const {
        std::forward<R>(r).set_error(std::forward<E>(e));
    }
};

struct set_stopped_t {
    template<typename R>
    void operator()(R&& r) const {
        std::forward<R>(r).set_stopped();
    }
};

inline constexpr set_value_t set_value{};
inline constexpr set_error_t set_error{};
inline constexpr set_stopped_t set_stopped{};

// Receiver概念
template<typename R>
concept Receiver = requires(R&& r) {
    { std::move(r).set_stopped() } noexcept;
};

template<typename R, typename E>
concept ReceiverOf = Receiver<R> && requires(R&& r, E&& e) {
    { std::move(r).set_error(std::forward<E>(e)) } noexcept;
};

// Operation State概念
template<typename O>
concept OperationState = requires(O& o) {
    { o.start() } noexcept;
};

// Sender概念（简化）
template<typename S>
concept Sender = requires(S&& s) {
    typename std::decay_t<S>::template value_types<std::tuple, std::variant>;
};

// 连接sender和receiver
struct connect_t {
    template<typename S, typename R>
    auto operator()(S&& s, R&& r) const {
        return std::forward<S>(s).connect(std::forward<R>(r));
    }
};

inline constexpr connect_t connect{};

// 启动操作
struct start_t {
    template<typename O>
    void operator()(O& o) const noexcept {
        o.start();
    }
};

inline constexpr start_t start{};

} // namespace exec
```

#### 2. 停止令牌 (execution/stop_token.hpp)

```cpp
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <functional>

namespace exec {

class StopSource;
class StopToken;

// 停止回调基类
class StopCallbackBase {
public:
    virtual ~StopCallbackBase() = default;
    virtual void call() noexcept = 0;

    StopCallbackBase* next_ = nullptr;
    StopCallbackBase* prev_ = nullptr;
};

// 停止状态
class StopState {
    friend class StopSource;
    friend class StopToken;

    std::atomic<bool> stopped_{false};
    std::mutex mutex_;
    StopCallbackBase* callbacks_ = nullptr;

public:
    bool requestStop() noexcept {
        if (stopped_.exchange(true)) {
            return false;  // 已经停止
        }

        // 调用所有回调
        std::lock_guard lock(mutex_);
        while (callbacks_) {
            auto cb = callbacks_;
            callbacks_ = cb->next_;
            cb->call();
        }
        return true;
    }

    bool stopRequested() const noexcept {
        return stopped_.load();
    }

    void registerCallback(StopCallbackBase* cb) {
        std::lock_guard lock(mutex_);
        if (stopped_.load()) {
            cb->call();
        } else {
            cb->next_ = callbacks_;
            if (callbacks_) callbacks_->prev_ = cb;
            callbacks_ = cb;
        }
    }

    void unregisterCallback(StopCallbackBase* cb) {
        std::lock_guard lock(mutex_);
        if (cb->prev_) cb->prev_->next_ = cb->next_;
        if (cb->next_) cb->next_->prev_ = cb->prev_;
        if (callbacks_ == cb) callbacks_ = cb->next_;
    }
};

class StopToken {
    std::shared_ptr<StopState> state_;

public:
    StopToken() = default;
    explicit StopToken(std::shared_ptr<StopState> state)
        : state_(std::move(state)) {}

    bool stopRequested() const noexcept {
        return state_ && state_->stopRequested();
    }

    bool stopPossible() const noexcept {
        return state_ != nullptr;
    }

    friend class StopSource;
    friend class StopCallback;
};

class StopSource {
    std::shared_ptr<StopState> state_;

public:
    StopSource() : state_(std::make_shared<StopState>()) {}

    bool requestStop() noexcept {
        return state_->requestStop();
    }

    StopToken getToken() const noexcept {
        return StopToken{state_};
    }

    bool stopRequested() const noexcept {
        return state_->stopRequested();
    }
};

template<typename F>
class StopCallback : private StopCallbackBase {
    F callback_;
    std::shared_ptr<StopState> state_;

    void call() noexcept override {
        callback_();
    }

public:
    StopCallback(StopToken token, F&& f)
        : callback_(std::forward<F>(f)), state_(token.state_) {
        if (state_) {
            state_->registerCallback(this);
        }
    }

    ~StopCallback() {
        if (state_) {
            state_->unregisterCallback(this);
        }
    }
};

} // namespace exec
```

#### 3. 基本Sender (execution/senders.hpp)

```cpp
#pragma once

#include "types.hpp"
#include "stop_token.hpp"
#include <tuple>
#include <utility>

namespace exec {

// ========== just: 立即完成的sender ==========

template<typename... Vs>
class JustSender {
    std::tuple<Vs...> values_;

public:
    template<template<typename...> class Tuple, template<typename...> class Variant>
    using value_types = Variant<Tuple<Vs...>>;

    template<typename... Args>
    explicit JustSender(Args&&... args)
        : values_(std::forward<Args>(args)...) {}

    template<typename R>
    class OperationState {
        R receiver_;
        std::tuple<Vs...> values_;

    public:
        OperationState(R r, std::tuple<Vs...> v)
            : receiver_(std::move(r)), values_(std::move(v)) {}

        void start() noexcept {
            std::apply([this](auto&&... vs) {
                set_value(std::move(receiver_), std::forward<decltype(vs)>(vs)...);
            }, std::move(values_));
        }
    };

    template<typename R>
    auto connect(R&& r) {
        return OperationState<std::decay_t<R>>(
            std::forward<R>(r), std::move(values_));
    }
};

template<typename... Vs>
auto just(Vs&&... vs) {
    return JustSender<std::decay_t<Vs>...>(std::forward<Vs>(vs)...);
}

inline auto just() {
    return JustSender<>();
}

// ========== then: 变换sender的值 ==========

template<typename S, typename F>
class ThenSender {
    S sender_;
    F func_;

public:
    template<template<typename...> class Tuple, template<typename...> class Variant>
    using value_types = Variant<Tuple<std::invoke_result_t<F, /* sender value types */>>>;

    ThenSender(S s, F f) : sender_(std::move(s)), func_(std::move(f)) {}

    template<typename R>
    class Receiver {
        R downstream_;
        F func_;

    public:
        Receiver(R r, F f) : downstream_(std::move(r)), func_(std::move(f)) {}

        template<typename... Vs>
        void set_value(Vs&&... vs) {
            try {
                if constexpr (std::is_void_v<std::invoke_result_t<F, Vs...>>) {
                    std::invoke(func_, std::forward<Vs>(vs)...);
                    exec::set_value(std::move(downstream_));
                } else {
                    exec::set_value(std::move(downstream_),
                                   std::invoke(func_, std::forward<Vs>(vs)...));
                }
            } catch (...) {
                exec::set_error(std::move(downstream_), std::current_exception());
            }
        }

        void set_error(std::exception_ptr e) noexcept {
            exec::set_error(std::move(downstream_), std::move(e));
        }

        void set_stopped() noexcept {
            exec::set_stopped(std::move(downstream_));
        }
    };

    template<typename R>
    auto connect(R&& r) {
        return exec::connect(
            std::move(sender_),
            Receiver<std::decay_t<R>>(std::forward<R>(r), std::move(func_))
        );
    }
};

template<typename F>
struct ThenAdaptor {
    F func;

    template<typename S>
    auto operator()(S&& s) const {
        return ThenSender<std::decay_t<S>, F>(std::forward<S>(s), func);
    }
};

template<typename F>
auto then(F&& f) {
    return ThenAdaptor<std::decay_t<F>>{std::forward<F>(f)};
}

// 管道运算符
template<typename S, typename A>
auto operator|(S&& s, A&& a) {
    return std::forward<A>(a)(std::forward<S>(s));
}

// ========== upon_error: 错误处理 ==========

template<typename S, typename F>
class UponErrorSender {
    S sender_;
    F func_;

public:
    UponErrorSender(S s, F f) : sender_(std::move(s)), func_(std::move(f)) {}

    template<typename R>
    class Receiver {
        R downstream_;
        F func_;

    public:
        Receiver(R r, F f) : downstream_(std::move(r)), func_(std::move(f)) {}

        template<typename... Vs>
        void set_value(Vs&&... vs) {
            exec::set_value(std::move(downstream_), std::forward<Vs>(vs)...);
        }

        void set_error(std::exception_ptr e) noexcept {
            try {
                exec::set_value(std::move(downstream_), std::invoke(func_, std::move(e)));
            } catch (...) {
                exec::set_error(std::move(downstream_), std::current_exception());
            }
        }

        void set_stopped() noexcept {
            exec::set_stopped(std::move(downstream_));
        }
    };

    template<typename R>
    auto connect(R&& r) {
        return exec::connect(
            std::move(sender_),
            Receiver<std::decay_t<R>>(std::forward<R>(r), std::move(func_))
        );
    }
};

template<typename F>
struct UponErrorAdaptor {
    F func;

    template<typename S>
    auto operator()(S&& s) const {
        return UponErrorSender<std::decay_t<S>, F>(std::forward<S>(s), func);
    }
};

template<typename F>
auto upon_error(F&& f) {
    return UponErrorAdaptor<std::decay_t<F>>{std::forward<F>(f)};
}

} // namespace exec
```

#### 4. 并发组合 (execution/combinators.hpp)

```cpp
#pragma once

#include "senders.hpp"
#include <tuple>
#include <atomic>
#include <memory>

namespace exec {

// ========== when_all: 等待所有sender完成 ==========

template<typename... Ss>
class WhenAllSender {
    std::tuple<Ss...> senders_;

public:
    explicit WhenAllSender(Ss... ss) : senders_(std::move(ss)...) {}

    template<typename R>
    class SharedState {
    public:
        R receiver;
        std::atomic<size_t> remaining{sizeof...(Ss)};
        std::atomic<bool> hadError{false};
        std::exception_ptr error;
        std::mutex mutex;

        // 存储每个sender的结果
        // 简化：假设所有sender返回相同类型

        explicit SharedState(R r) : receiver(std::move(r)) {}

        void complete() {
            if (remaining.fetch_sub(1) == 1) {
                if (hadError.load()) {
                    set_error(std::move(receiver), std::move(error));
                } else {
                    set_value(std::move(receiver));  // 简化
                }
            }
        }

        void setError(std::exception_ptr e) {
            bool expected = false;
            if (hadError.compare_exchange_strong(expected, true)) {
                error = std::move(e);
            }
            complete();
        }
    };

    template<typename R, size_t I>
    class ElementReceiver {
        std::shared_ptr<SharedState<R>> state_;

    public:
        explicit ElementReceiver(std::shared_ptr<SharedState<R>> state)
            : state_(std::move(state)) {}

        template<typename... Vs>
        void set_value(Vs&&...) {
            state_->complete();
        }

        void set_error(std::exception_ptr e) noexcept {
            state_->setError(std::move(e));
        }

        void set_stopped() noexcept {
            state_->complete();  // 简化：忽略取消
        }
    };

    template<typename R>
    class OperationState {
        using State = SharedState<R>;
        std::shared_ptr<State> state_;
        std::tuple</* operation states */> ops_;  // 简化

    public:
        OperationState(std::tuple<Ss...> senders, R r)
            : state_(std::make_shared<State>(std::move(r))) {
            // 连接每个sender
        }

        void start() noexcept {
            // 启动所有操作
        }
    };

    template<typename R>
    auto connect(R&& r) {
        return OperationState<std::decay_t<R>>(
            std::move(senders_), std::forward<R>(r));
    }
};

template<typename... Ss>
auto when_all(Ss&&... ss) {
    return WhenAllSender<std::decay_t<Ss>...>(std::forward<Ss>(ss)...);
}

// ========== let_value: 动态创建sender ==========

template<typename S, typename F>
class LetValueSender {
    S sender_;
    F func_;

public:
    LetValueSender(S s, F f) : sender_(std::move(s)), func_(std::move(f)) {}

    template<typename R>
    class Receiver {
        R downstream_;
        F func_;
        // 需要存储内部操作状态

    public:
        Receiver(R r, F f) : downstream_(std::move(r)), func_(std::move(f)) {}

        template<typename... Vs>
        void set_value(Vs&&... vs) {
            try {
                auto innerSender = std::invoke(func_, std::forward<Vs>(vs)...);
                // 连接并启动内部sender
                auto op = exec::connect(std::move(innerSender),
                                       std::move(downstream_));
                exec::start(op);
            } catch (...) {
                exec::set_error(std::move(downstream_), std::current_exception());
            }
        }

        void set_error(std::exception_ptr e) noexcept {
            exec::set_error(std::move(downstream_), std::move(e));
        }

        void set_stopped() noexcept {
            exec::set_stopped(std::move(downstream_));
        }
    };

    template<typename R>
    auto connect(R&& r) {
        return exec::connect(
            std::move(sender_),
            Receiver<std::decay_t<R>>(std::forward<R>(r), std::move(func_))
        );
    }
};

template<typename F>
struct LetValueAdaptor {
    F func;

    template<typename S>
    auto operator()(S&& s) const {
        return LetValueSender<std::decay_t<S>, F>(std::forward<S>(s), func);
    }
};

template<typename F>
auto let_value(F&& f) {
    return LetValueAdaptor<std::decay_t<F>>{std::forward<F>(f)};
}

} // namespace exec
```

#### 5. 线程池调度器 (execution/thread_pool.hpp)

```cpp
#pragma once

#include "senders.hpp"
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace exec {

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};

    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) return;

                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            t.join();
        }
    }

    void submit(std::function<void()> task) {
        {
            std::lock_guard lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    // 调度器
    class Scheduler {
        ThreadPool* pool_;

    public:
        explicit Scheduler(ThreadPool* pool) : pool_(pool) {}

        class ScheduleSender {
            ThreadPool* pool_;

        public:
            template<template<typename...> class Tuple,
                     template<typename...> class Variant>
            using value_types = Variant<Tuple<>>;

            explicit ScheduleSender(ThreadPool* pool) : pool_(pool) {}

            template<typename R>
            class OperationState {
                ThreadPool* pool_;
                R receiver_;

            public:
                OperationState(ThreadPool* pool, R r)
                    : pool_(pool), receiver_(std::move(r)) {}

                void start() noexcept {
                    pool_->submit([r = std::move(receiver_)]() mutable {
                        set_value(std::move(r));
                    });
                }
            };

            template<typename R>
            auto connect(R&& r) {
                return OperationState<std::decay_t<R>>(
                    pool_, std::forward<R>(r));
            }
        };

        auto schedule() {
            return ScheduleSender{pool_};
        }
    };

    Scheduler getScheduler() {
        return Scheduler{this};
    }
};

// on: 在指定调度器上执行sender
template<typename Scheduler, typename S>
class OnSender {
    Scheduler scheduler_;
    S sender_;

public:
    OnSender(Scheduler sched, S s)
        : scheduler_(std::move(sched)), sender_(std::move(s)) {}

    template<typename R>
    auto connect(R&& r) {
        // 先在调度器上调度，然后执行sender
        return exec::connect(
            scheduler_.schedule()
            | then([s = std::move(sender_)]() mutable { return std::move(s); })
            | let_value([](auto&& inner) { return std::move(inner); }),
            std::forward<R>(r)
        );
    }
};

template<typename Scheduler, typename S>
auto on(Scheduler sched, S&& s) {
    return OnSender<Scheduler, std::decay_t<S>>(
        std::move(sched), std::forward<S>(s));
}

} // namespace exec
```

#### 6. 同步等待 (execution/sync_wait.hpp)

```cpp
#pragma once

#include "senders.hpp"
#include <optional>
#include <mutex>
#include <condition_variable>
#include <variant>

namespace exec {

template<typename... Ts>
class SyncWaitReceiver {
    std::mutex& mutex_;
    std::condition_variable& cv_;
    std::variant<std::monostate, std::tuple<Ts...>, std::exception_ptr>& result_;
    bool& done_;

public:
    SyncWaitReceiver(
        std::mutex& m,
        std::condition_variable& cv,
        std::variant<std::monostate, std::tuple<Ts...>, std::exception_ptr>& result,
        bool& done
    ) : mutex_(m), cv_(cv), result_(result), done_(done) {}

    template<typename... Vs>
    void set_value(Vs&&... vs) {
        {
            std::lock_guard lock(mutex_);
            result_.template emplace<1>(std::forward<Vs>(vs)...);
            done_ = true;
        }
        cv_.notify_one();
    }

    void set_error(std::exception_ptr e) noexcept {
        {
            std::lock_guard lock(mutex_);
            result_.template emplace<2>(std::move(e));
            done_ = true;
        }
        cv_.notify_one();
    }

    void set_stopped() noexcept {
        {
            std::lock_guard lock(mutex_);
            done_ = true;
        }
        cv_.notify_one();
    }
};

template<typename S>
auto sync_wait(S&& s) {
    // 简化：假设sender返回单个值或void
    using ResultType = std::tuple<>;  // 需要从sender提取

    std::mutex mutex;
    std::condition_variable cv;
    std::variant<std::monostate, ResultType, std::exception_ptr> result;
    bool done = false;

    auto op = connect(
        std::forward<S>(s),
        SyncWaitReceiver<>(mutex, cv, result, done)
    );

    start(op);

    {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return done; });
    }

    if (result.index() == 2) {
        std::rethrow_exception(std::get<2>(result));
    }

    if (result.index() == 1) {
        return std::optional{std::get<1>(result)};
    }

    return std::optional<ResultType>{};
}

} // namespace exec
```

#### 7. 使用示例 (main.cpp)

```cpp
#include "execution/senders.hpp"
#include "execution/combinators.hpp"
#include "execution/thread_pool.hpp"
#include "execution/sync_wait.hpp"
#include <iostream>
#include <chrono>

using namespace exec;

void basicExample() {
    std::cout << "=== Basic Example ===\n\n";

    // 简单的值传递
    auto result = sync_wait(
        just(42)
        | then([](int x) {
            std::cout << "Got: " << x << "\n";
            return x * 2;
        })
        | then([](int x) {
            std::cout << "Doubled: " << x << "\n";
            return x;
        })
    );

    std::cout << "Final result: " << std::get<0>(*result) << "\n\n";
}

void errorHandlingExample() {
    std::cout << "=== Error Handling Example ===\n\n";

    auto result = sync_wait(
        just(42)
        | then([](int x) -> int {
            if (x > 0) {
                throw std::runtime_error("x is positive!");
            }
            return x;
        })
        | upon_error([](std::exception_ptr ep) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                std::cout << "Caught error: " << e.what() << "\n";
            }
            return -1;
        })
    );

    std::cout << "Result after error: " << std::get<0>(*result) << "\n\n";
}

void threadPoolExample() {
    std::cout << "=== Thread Pool Example ===\n\n";

    ThreadPool pool(4);
    auto scheduler = pool.getScheduler();

    // 在线程池中执行
    sync_wait(
        scheduler.schedule()
        | then([]() {
            std::cout << "Running on thread: "
                      << std::this_thread::get_id() << "\n";
            return 42;
        })
        | then([](int x) {
            std::cout << "Processing " << x << " on thread: "
                      << std::this_thread::get_id() << "\n";
            return x * 2;
        })
    );

    std::cout << "\n";
}

void stopTokenExample() {
    std::cout << "=== Stop Token Example ===\n\n";

    StopSource source;
    auto token = source.getToken();

    StopCallback callback(token, []() {
        std::cout << "Stop requested!\n";
    });

    std::cout << "Stop requested before: " << token.stopRequested() << "\n";
    source.requestStop();
    std::cout << "Stop requested after: " << token.stopRequested() << "\n\n";
}

void pipelineExample() {
    std::cout << "=== Pipeline Example ===\n\n";

    // 模拟数据处理管道
    auto fetch = []() {
        return just(std::vector<int>{1, 2, 3, 4, 5});
    };

    auto process = [](std::vector<int> data) {
        for (auto& x : data) x *= 2;
        return data;
    };

    auto summarize = [](std::vector<int> data) {
        int sum = 0;
        for (auto x : data) sum += x;
        return sum;
    };

    auto result = sync_wait(
        fetch()
        | then(process)
        | then(summarize)
        | then([](int sum) {
            std::cout << "Sum: " << sum << "\n";
            return sum;
        })
    );

    std::cout << "\n";
}

int main() {
    basicExample();
    errorHandlingExample();
    threadPoolExample();
    stopTokenExample();
    pipelineExample();

    std::cout << "=== All examples completed ===\n";
    return 0;
}
```

---

## 检验标准

### 知识检验
1. [ ] 能够解释Sender/Receiver模型的设计理念
2. [ ] 理解调度器的作用和实现
3. [ ] 掌握异步操作的组合方式
4. [ ] 理解取消机制的设计
5. [ ] 能够解释结构化并发的原则

### 实践检验
1. [ ] 完成基本Sender实现
2. [ ] 实现then、upon_error等组合器
3. [ ] 实现线程池调度器
4. [ ] 实现sync_wait
5. [ ] 编写完整的示例程序

### 代码质量
1. [ ] 代码符合概念约束
2. [ ] 异常安全
3. [ ] 线程安全
4. [ ] 有完整的测试

---

## 输出物清单

1. **学习笔记**
   - [ ] std::execution设计分析
   - [ ] Sender/Receiver模型详解
   - [ ] 源码阅读笔记

2. **代码产出**
   - [ ] 异步任务系统实现
   - [ ] 基本组合器库
   - [ ] 示例应用

3. **文档产出**
   - [ ] API参考文档
   - [ ] 使用指南
   - [ ] 设计文档

---

## 时间分配表

| 周次 | 理论学习 | 源码阅读 | 项目实践 | 总计 |
|------|----------|----------|----------|------|
| Week 1 | 18h | 10h | 7h | 35h |
| Week 2 | 12h | 12h | 11h | 35h |
| Week 3 | 10h | 8h | 17h | 35h |
| Week 4 | 5h | 5h | 25h | 35h |
| **总计** | **45h** | **35h** | **60h** | **140h** |

---

## 下月预告

**Month 59: 开源项目贡献实践**

下个月将学习如何参与开源项目：
- 开源社区文化与规范
- 如何选择项目和找到贡献点
- Git工作流和PR流程
- 代码审查最佳实践
- 实践项目：为真实开源项目贡献代码

建议提前：
1. 在GitHub上关注感兴趣的C++项目
2. 阅读一些项目的贡献指南
3. 熟悉Git高级操作
