# Month 11: 时间库与chrono——精确的时间处理

## 本月主题概述

时间处理是系统编程的重要部分，C++11引入的chrono库提供了类型安全的时间抽象。本月将深入理解duration、time_point、clock的设计，掌握高精度计时、超时处理和时区转换。

---

## 理论学习内容

### 第一周：chrono基础设施

**学习目标**：理解chrono的核心抽象

**阅读材料**：
- [ ] 《C++ Concurrency in Action》时间相关章节
- [ ] CppCon演讲："A <chrono> Tutorial"
- [ ] cppreference chrono文档

**核心概念**：

#### duration（时长）
```cpp
#include <chrono>

// duration模板：duration<Rep, Period>
// Rep: 表示周期数的类型
// Period: ratio类型，表示一个周期占多少秒

// 预定义duration类型
std::chrono::nanoseconds   // duration<至少64位整数, ratio<1, 1000000000>>
std::chrono::microseconds  // ratio<1, 1000000>
std::chrono::milliseconds  // ratio<1, 1000>
std::chrono::seconds       // ratio<1, 1>
std::chrono::minutes       // ratio<60, 1>
std::chrono::hours         // ratio<3600, 1>
std::chrono::days          // C++20, ratio<86400, 1>

// 创建duration
auto d1 = std::chrono::seconds(5);
auto d2 = std::chrono::milliseconds(500);
auto d3 = 5s;    // C++14 字面量
auto d4 = 500ms;
auto d5 = 2.5s;  // 浮点数也可以

// 算术运算
auto sum = d1 + d2;  // 5500ms（自动转换为更精细的单位）
auto diff = d1 - d2; // 4500ms
auto product = d1 * 2;  // 10s
auto quotient = d1 / 2;  // 2s

// duration_cast（显式转换，可能丢失精度）
auto secs = std::chrono::duration_cast<std::chrono::seconds>(d2);  // 0s
```

#### ratio（编译期有理数）
```cpp
// ratio<Num, Denom> 表示 Num/Denom
using kilo  = std::ratio<1000, 1>;    // 1000
using milli = std::ratio<1, 1000>;    // 0.001
using mega  = std::ratio<1000000, 1>;

// 编译期算术
using product = std::ratio_multiply<kilo, milli>;  // ratio<1, 1>
using sum = std::ratio_add<std::ratio<1, 3>, std::ratio<1, 6>>;  // ratio<1, 2>
```

### 第二周：时钟（Clock）

**学习目标**：理解不同时钟的特性和用途

```cpp
// 时钟类型
// 1. system_clock - 系统时钟（可能被调整）
//    - now() 返回系统当前时间
//    - 可以转换为time_t
//    - 受系统时间调整影响（NTP同步、手动调整）

// 2. steady_clock - 单调时钟
//    - 保证不会后退
//    - 适合测量时间间隔
//    - 不受系统时间调整影响

// 3. high_resolution_clock
//    - 可用的最高精度时钟
//    - 可能是system_clock或steady_clock的别名

// 4. utc_clock, tai_clock, gps_clock - C++20
//    - 特定时间系统的时钟

// 时钟的静态成员
using Clock = std::chrono::steady_clock;
auto now = Clock::now();  // 返回 time_point<Clock>
constexpr bool is_steady = Clock::is_steady;  // true for steady_clock
using duration = Clock::duration;
using rep = Clock::rep;
using period = Clock::period;
```

**测量时间**：
```cpp
#include <chrono>
#include <iostream>

template <typename Func>
auto measure(Func&& f) {
    auto start = std::chrono::steady_clock::now();
    std::forward<Func>(f)();
    auto end = std::chrono::steady_clock::now();
    return end - start;
}

// 使用
auto duration = measure([]() {
    // 要测量的代码
});
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
std::cout << "Took " << ms.count() << " ms\n";
```

### 第三周：time_point（时间点）

**学习目标**：理解时间点的表示和运算

```cpp
// time_point<Clock, Duration>
// 表示特定时钟的某个时间点

using namespace std::chrono;

// 获取当前时间点
auto now = system_clock::now();

// 时间点运算
auto later = now + hours(1);  // 1小时后
auto duration = later - now;  // 得到duration

// 转换为time_t（与C接口交互）
std::time_t t = system_clock::to_time_t(now);
std::cout << std::ctime(&t);

// 从time_t转换
auto tp = system_clock::from_time_t(t);

// C++20: calendar和time zones
// year_month_day, zoned_time等
auto ymd = year_month_day{year{2025}, month{1}, day{15}};
auto sys_days = sys_days(ymd);  // 转换为system_clock的time_point
```

### 第四周：实际应用

**学习目标**：在实际场景中应用chrono

#### 超时处理
```cpp
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

// 条件变量超时等待
std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void wait_with_timeout() {
    std::unique_lock<std::mutex> lock(mtx);

    // 使用duration
    if (cv.wait_for(lock, std::chrono::seconds(5),
                    []{ return ready; })) {
        std::cout << "Condition met\n";
    } else {
        std::cout << "Timeout!\n";
    }

    // 使用time_point（绝对时间）
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(5);
    if (cv.wait_until(lock, deadline,
                      []{ return ready; })) {
        std::cout << "Condition met\n";
    } else {
        std::cout << "Timeout!\n";
    }
}
```

#### 定时器实现
```cpp
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>

class Timer {
    std::atomic<bool> running_{false};
    std::thread thread_;

public:
    template <typename Func, typename Rep, typename Period>
    void start(std::chrono::duration<Rep, Period> interval, Func&& f) {
        running_ = true;
        thread_ = std::thread([this, interval, f = std::forward<Func>(f)]() {
            auto next = std::chrono::steady_clock::now() + interval;
            while (running_) {
                std::this_thread::sleep_until(next);
                if (running_) {
                    f();
                    next += interval;
                }
            }
        });
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    ~Timer() { stop(); }
};
```

#### 性能测量工具
```cpp
#include <chrono>
#include <string>
#include <iostream>
#include <map>

class Profiler {
    using Clock = std::chrono::high_resolution_clock;

    struct Measurement {
        Clock::duration total{0};
        size_t count = 0;
    };

    std::map<std::string, Measurement> measurements_;

public:
    class ScopedTimer {
        Profiler& profiler_;
        std::string name_;
        Clock::time_point start_;

    public:
        ScopedTimer(Profiler& p, std::string name)
            : profiler_(p), name_(std::move(name)), start_(Clock::now()) {}

        ~ScopedTimer() {
            auto duration = Clock::now() - start_;
            profiler_.measurements_[name_].total += duration;
            profiler_.measurements_[name_].count++;
        }
    };

    ScopedTimer measure(std::string name) {
        return ScopedTimer(*this, std::move(name));
    }

    void report() const {
        for (const auto& [name, m] : measurements_) {
            auto avg = m.total / m.count;
            auto avg_us = std::chrono::duration_cast<
                std::chrono::microseconds>(avg);
            std::cout << name << ": "
                      << "count=" << m.count
                      << ", avg=" << avg_us.count() << "us\n";
        }
    }
};

// 使用
Profiler profiler;
{
    auto timer = profiler.measure("sort");
    // 排序代码
}
profiler.report();
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `std::chrono::duration`实现
- [ ] `std::ratio`编译期算术
- [ ] `std::chrono::steady_clock`平台实现（Linux/macOS/Windows）
- [ ] `std::this_thread::sleep_for/sleep_until`实现

---

## 实践项目

### 项目：实现时间处理库

#### Part 1: mini_duration
```cpp
// mini_chrono.hpp
#pragma once
#include <ratio>
#include <type_traits>
#include <limits>

namespace mini::chrono {

// duration
template <typename Rep, typename Period = std::ratio<1>>
class duration {
public:
    using rep = Rep;
    using period = Period;

private:
    rep count_;

public:
    constexpr duration() = default;

    template <typename Rep2,
              typename = std::enable_if_t<
                  std::is_convertible_v<Rep2, Rep> &&
                  (std::is_floating_point_v<Rep> ||
                   !std::is_floating_point_v<Rep2>)>>
    constexpr explicit duration(const Rep2& r) : count_(static_cast<Rep>(r)) {}

    // 从另一个duration转换
    template <typename Rep2, typename Period2,
              typename = std::enable_if_t<
                  std::is_floating_point_v<Rep> ||
                  (std::ratio_divide<Period2, Period>::den == 1 &&
                   !std::is_floating_point_v<Rep2>)>>
    constexpr duration(const duration<Rep2, Period2>& d)
        : count_(static_cast<Rep>(d.count() *
                 std::ratio_divide<Period2, Period>::num /
                 std::ratio_divide<Period2, Period>::den)) {}

    constexpr rep count() const { return count_; }

    // 算术运算
    constexpr duration operator+() const { return *this; }
    constexpr duration operator-() const { return duration(-count_); }

    constexpr duration& operator++() { ++count_; return *this; }
    constexpr duration operator++(int) { return duration(count_++); }
    constexpr duration& operator--() { --count_; return *this; }
    constexpr duration operator--(int) { return duration(count_--); }

    constexpr duration& operator+=(const duration& d) {
        count_ += d.count();
        return *this;
    }

    constexpr duration& operator-=(const duration& d) {
        count_ -= d.count();
        return *this;
    }

    constexpr duration& operator*=(const rep& r) {
        count_ *= r;
        return *this;
    }

    constexpr duration& operator/=(const rep& r) {
        count_ /= r;
        return *this;
    }

    // 特殊值
    static constexpr duration zero() { return duration(0); }
    static constexpr duration min() { return duration(std::numeric_limits<Rep>::lowest()); }
    static constexpr duration max() { return duration(std::numeric_limits<Rep>::max()); }
};

// duration算术运算（非成员）
template <typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr auto operator+(const duration<Rep1, Period1>& lhs,
                         const duration<Rep2, Period2>& rhs) {
    using common_type = std::common_type_t<duration<Rep1, Period1>,
                                            duration<Rep2, Period2>>;
    return common_type(common_type(lhs).count() + common_type(rhs).count());
}

template <typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr auto operator-(const duration<Rep1, Period1>& lhs,
                         const duration<Rep2, Period2>& rhs) {
    using common_type = std::common_type_t<duration<Rep1, Period1>,
                                            duration<Rep2, Period2>>;
    return common_type(common_type(lhs).count() - common_type(rhs).count());
}

template <typename Rep1, typename Period, typename Rep2>
constexpr auto operator*(const duration<Rep1, Period>& d, const Rep2& s) {
    using common_rep = std::common_type_t<Rep1, Rep2>;
    return duration<common_rep, Period>(
        static_cast<common_rep>(d.count()) * static_cast<common_rep>(s));
}

template <typename Rep1, typename Period, typename Rep2>
constexpr auto operator/(const duration<Rep1, Period>& d, const Rep2& s) {
    using common_rep = std::common_type_t<Rep1, Rep2>;
    return duration<common_rep, Period>(
        static_cast<common_rep>(d.count()) / static_cast<common_rep>(s));
}

// 比较运算
template <typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr bool operator==(const duration<Rep1, Period1>& lhs,
                          const duration<Rep2, Period2>& rhs) {
    using common_type = std::common_type_t<duration<Rep1, Period1>,
                                            duration<Rep2, Period2>>;
    return common_type(lhs).count() == common_type(rhs).count();
}

template <typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr bool operator<(const duration<Rep1, Period1>& lhs,
                         const duration<Rep2, Period2>& rhs) {
    using common_type = std::common_type_t<duration<Rep1, Period1>,
                                            duration<Rep2, Period2>>;
    return common_type(lhs).count() < common_type(rhs).count();
}

// duration_cast
template <typename ToDuration, typename Rep, typename Period>
constexpr ToDuration duration_cast(const duration<Rep, Period>& d) {
    using ratio = std::ratio_divide<Period, typename ToDuration::period>;
    using common_rep = std::common_type_t<typename ToDuration::rep, Rep>;
    return ToDuration(static_cast<typename ToDuration::rep>(
        static_cast<common_rep>(d.count()) *
        static_cast<common_rep>(ratio::num) /
        static_cast<common_rep>(ratio::den)));
}

// 预定义类型
using nanoseconds  = duration<long long, std::nano>;
using microseconds = duration<long long, std::micro>;
using milliseconds = duration<long long, std::milli>;
using seconds      = duration<long long>;
using minutes      = duration<int, std::ratio<60>>;
using hours        = duration<int, std::ratio<3600>>;

} // namespace mini::chrono
```

#### Part 2: mini_time_point
```cpp
// mini_time_point.hpp (续)
namespace mini::chrono {

template <typename Clock, typename Duration = typename Clock::duration>
class time_point {
public:
    using clock = Clock;
    using duration = Duration;
    using rep = typename Duration::rep;
    using period = typename Duration::period;

private:
    Duration d_;

public:
    constexpr time_point() : d_(Duration::zero()) {}

    constexpr explicit time_point(const Duration& d) : d_(d) {}

    template <typename Duration2,
              typename = std::enable_if_t<
                  std::is_convertible_v<Duration2, Duration>>>
    constexpr time_point(const time_point<Clock, Duration2>& t)
        : d_(t.time_since_epoch()) {}

    constexpr Duration time_since_epoch() const { return d_; }

    constexpr time_point& operator+=(const Duration& d) {
        d_ += d;
        return *this;
    }

    constexpr time_point& operator-=(const Duration& d) {
        d_ -= d;
        return *this;
    }

    static constexpr time_point min() {
        return time_point(Duration::min());
    }

    static constexpr time_point max() {
        return time_point(Duration::max());
    }
};

// time_point算术
template <typename Clock, typename Duration1, typename Rep, typename Period>
constexpr auto operator+(const time_point<Clock, Duration1>& tp,
                         const duration<Rep, Period>& d) {
    using common_dur = std::common_type_t<Duration1, duration<Rep, Period>>;
    return time_point<Clock, common_dur>(tp.time_since_epoch() + d);
}

template <typename Clock, typename Duration1, typename Duration2>
constexpr auto operator-(const time_point<Clock, Duration1>& lhs,
                         const time_point<Clock, Duration2>& rhs) {
    return lhs.time_since_epoch() - rhs.time_since_epoch();
}

// 比较
template <typename Clock, typename Duration1, typename Duration2>
constexpr bool operator==(const time_point<Clock, Duration1>& lhs,
                          const time_point<Clock, Duration2>& rhs) {
    return lhs.time_since_epoch() == rhs.time_since_epoch();
}

template <typename Clock, typename Duration1, typename Duration2>
constexpr bool operator<(const time_point<Clock, Duration1>& lhs,
                         const time_point<Clock, Duration2>& rhs) {
    return lhs.time_since_epoch() < rhs.time_since_epoch();
}

} // namespace mini::chrono
```

#### Part 3: 简单时钟实现
```cpp
// mini_clock.hpp
#pragma once
#include "mini_chrono.hpp"
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace mini::chrono {

struct steady_clock {
    using duration = nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = mini::chrono::time_point<steady_clock>;
    static constexpr bool is_steady = true;

    static time_point now() noexcept {
#ifdef _WIN32
        LARGE_INTEGER freq, count;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&count);
        return time_point(nanoseconds(
            count.QuadPart * 1000000000LL / freq.QuadPart));
#elif defined(__APPLE__)
        // macOS: mach_absolute_time
        // 简化：使用gettimeofday
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return time_point(nanoseconds(
            tv.tv_sec * 1000000000LL + tv.tv_usec * 1000LL));
#else
        // Linux: clock_gettime
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return time_point(nanoseconds(
            ts.tv_sec * 1000000000LL + ts.tv_nsec));
#endif
    }
};

struct system_clock {
    using duration = nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = mini::chrono::time_point<system_clock>;
    static constexpr bool is_steady = false;

    static time_point now() noexcept {
#ifdef _WIN32
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        // FILETIME是100纳秒单位，从1601年开始
        // 转换为Unix纪元
        return time_point(nanoseconds(
            (uli.QuadPart - 116444736000000000ULL) * 100));
#else
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return time_point(nanoseconds(
            ts.tv_sec * 1000000000LL + ts.tv_nsec));
#endif
    }

    static std::time_t to_time_t(const time_point& tp) noexcept {
        return duration_cast<seconds>(tp.time_since_epoch()).count();
    }

    static time_point from_time_t(std::time_t t) noexcept {
        return time_point(seconds(t));
    }
};

using high_resolution_clock = steady_clock;

} // namespace mini::chrono
```

#### Part 4: 实用时间工具
```cpp
// time_utils.hpp
#pragma once
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

namespace time_utils {

// 格式化持续时间为可读字符串
template <typename Duration>
std::string format_duration(Duration d) {
    using namespace std::chrono;

    auto hrs = duration_cast<hours>(d);
    d -= hrs;
    auto mins = duration_cast<minutes>(d);
    d -= mins;
    auto secs = duration_cast<seconds>(d);
    d -= secs;
    auto ms = duration_cast<milliseconds>(d);

    std::ostringstream oss;
    if (hrs.count() > 0) {
        oss << hrs.count() << "h ";
    }
    if (mins.count() > 0) {
        oss << mins.count() << "m ";
    }
    if (secs.count() > 0) {
        oss << secs.count() << "s ";
    }
    oss << ms.count() << "ms";

    return oss.str();
}

// 格式化时间点
std::string format_time(std::chrono::system_clock::time_point tp,
                        const char* fmt = "%Y-%m-%d %H:%M:%S") {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, fmt);
    return oss.str();
}

// 解析时间字符串
std::chrono::system_clock::time_point parse_time(
    const std::string& str,
    const char* fmt = "%Y-%m-%d %H:%M:%S") {

    std::tm tm = {};
    std::istringstream iss(str);
    iss >> std::get_time(&tm, fmt);

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// 简易秒表
class Stopwatch {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_;
    Clock::duration accumulated_{0};
    bool running_ = false;

public:
    void start() {
        if (!running_) {
            start_ = Clock::now();
            running_ = true;
        }
    }

    void stop() {
        if (running_) {
            accumulated_ += Clock::now() - start_;
            running_ = false;
        }
    }

    void reset() {
        accumulated_ = Clock::duration{0};
        running_ = false;
    }

    Clock::duration elapsed() const {
        if (running_) {
            return accumulated_ + (Clock::now() - start_);
        }
        return accumulated_;
    }

    template <typename Duration = std::chrono::milliseconds>
    typename Duration::rep elapsed_count() const {
        return std::chrono::duration_cast<Duration>(elapsed()).count();
    }
};

// 速率限制器
class RateLimiter {
    using Clock = std::chrono::steady_clock;
    Clock::duration interval_;
    Clock::time_point last_allowed_;

public:
    explicit RateLimiter(Clock::duration interval)
        : interval_(interval), last_allowed_(Clock::now() - interval) {}

    bool try_acquire() {
        auto now = Clock::now();
        if (now - last_allowed_ >= interval_) {
            last_allowed_ = now;
            return true;
        }
        return false;
    }

    void wait() {
        auto now = Clock::now();
        auto next = last_allowed_ + interval_;
        if (next > now) {
            std::this_thread::sleep_until(next);
        }
        last_allowed_ = Clock::now();
    }
};

} // namespace time_utils
```

---

## 检验标准

### 知识检验
- [ ] duration、time_point、clock各自的作用是什么？
- [ ] steady_clock和system_clock的区别？各适用什么场景？
- [ ] ratio如何实现编译期有理数运算？
- [ ] 为什么测量时间间隔应该用steady_clock？

### 实践检验
- [ ] mini_duration支持类型安全的时间运算
- [ ] mini_clock在各平台正确获取时间
- [ ] 实用工具（Stopwatch、RateLimiter）工作正常

### 输出物
1. `mini_chrono.hpp`（duration）
2. `mini_time_point.hpp`
3. `mini_clock.hpp`
4. `time_utils.hpp`
5. `test_chrono.cpp`
6. `notes/month11_chrono.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 30小时 | 21% |
| 源码阅读 | 25小时 | 18% |
| mini_chrono实现 | 40小时 | 29% |
| mini_clock实现 | 25小时 | 18% |
| 工具与测试 | 20小时 | 14% |

---

## 下月预告

Month 12将是第一年的总结与综合项目月，整合本年学习内容，完成一个综合性项目，并进行知识复盘。
