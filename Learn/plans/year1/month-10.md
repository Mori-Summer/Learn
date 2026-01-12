# Month 10: 字符串处理与正则表达式——文本处理的艺术

## 本月主题概述

字符串是最常用的数据类型之一，但C++的字符串处理有许多深层细节。本月将深入std::string的SSO优化、std::string_view的设计哲学、字符编码处理，以及正则表达式的使用。

---

## 理论学习内容

### 第一周：std::string内部实现

**学习目标**：理解string的内存布局和优化策略

**阅读材料**：
- [ ] 《STL源码剖析》string章节
- [ ] CppCon演讲："std::string - The Complete Story"
- [ ] 博客：SSO实现对比分析

**核心概念**：

#### 小字符串优化（SSO）
```cpp
// std::string的典型布局（简化）
// 方案1：libstdc++（GCC）
class string {
    // 当字符串短时，直接存储在这里
    // 当字符串长时，这是指向堆的指针
    union {
        char local_buffer[16];  // SSO缓冲区
        struct {
            char* ptr;
            size_t size;
            size_t capacity;
        } heap;
    };
    // 使用某种标志位区分
};

// 方案2：libc++（LLVM）
// 使用capacity的最低位作为标志
// short string: capacity最低位为1
// long string: capacity最低位为0（对齐保证）
```

**验证SSO大小**：
```cpp
#include <string>
#include <iostream>

int main() {
    std::string s;
    // 找到SSO阈值
    for (int i = 0; i < 32; ++i) {
        s.push_back('a');
        // data()地址在栈上还是堆上
        bool is_sso = (s.data() >= reinterpret_cast<const char*>(&s) &&
                       s.data() < reinterpret_cast<const char*>(&s) + sizeof(s));
        std::cout << "size=" << i + 1 << " SSO=" << is_sso << "\n";
    }
}
// 典型结果：libstdc++ SSO上限15字节，libc++ SSO上限22字节
```

#### Copy-on-Write（已废弃）
```cpp
// C++11之前，一些实现使用COW
// std::string a = "hello";
// std::string b = a;  // b和a共享同一块内存
// b[0] = 'H';  // 此时才真正拷贝

// C++11要求string是move-safe，COW被禁止
// 因为COW需要引用计数，与move语义和多线程不兼容
```

### 第二周：std::string_view

**学习目标**：理解非拥有字符串视图

**核心概念**：
```cpp
// string_view = 指针 + 长度，不拥有数据
class string_view {
    const char* data_;
    size_t size_;
public:
    // 从各种来源构造
    string_view(const char* s);
    string_view(const char* s, size_t len);
    string_view(const std::string& s);

    // 不能修改
    const char* data() const;
    size_t size() const;
    char operator[](size_t pos) const;

    // 子串视图（不分配内存！）
    string_view substr(size_t pos, size_t len) const;

    // 查找
    size_t find(char c) const;
};
```

**使用场景和注意事项**：
```cpp
// 好的使用
void process(std::string_view sv);  // 函数参数
process("literal");      // 无拷贝
process(some_string);    // 无拷贝
process(sv.substr(0, 5)); // 无分配

// 危险！悬空引用
std::string_view dangerous() {
    std::string s = "hello";
    return s;  // s被销毁，返回悬空view
}

std::string_view also_dangerous(std::string s) {
    return s;  // s是按值传递，函数返回时销毁
}

// 注意：string_view没有null终止保证
void print_cstr(const char* s);  // 需要null终止
std::string_view sv = "hello";
// print_cstr(sv.data());  // 危险！sv可能不是null终止的

// 安全的做法
std::string temp(sv);
print_cstr(temp.c_str());
```

### 第三周：字符编码与Unicode

**学习目标**：理解C++的字符编码支持

**核心概念**：
```cpp
// 基本字符类型
char      // 至少8位，通常用于ASCII或UTF-8
wchar_t   // 宽字符，Windows上16位，Linux上32位
char8_t   // C++20，专用于UTF-8
char16_t  // UTF-16
char32_t  // UTF-32

// 字符串字面量前缀
"hello"     // const char[]
L"hello"    // const wchar_t[]
u8"hello"   // const char8_t[] (C++20)
u"hello"    // const char16_t[]
U"hello"    // const char32_t[]

// 原始字符串（避免转义）
R"(raw string with \n literal backslash)"
```

**UTF-8处理**：
```cpp
#include <string>
#include <codecvt>  // C++17废弃
#include <locale>

// UTF-8字符串长度（码点数，不是字节数）
size_t utf8_length(const std::string& s) {
    size_t len = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = s[i];
        if ((c & 0x80) == 0) i += 1;        // ASCII
        else if ((c & 0xE0) == 0xC0) i += 2; // 2字节
        else if ((c & 0xF0) == 0xE0) i += 3; // 3字节
        else if ((c & 0xF8) == 0xF0) i += 4; // 4字节
        else ++i;  // 无效，跳过
        ++len;
    }
    return len;
}

// UTF-8迭代器（简化版）
class Utf8Iterator {
    const char* ptr_;
public:
    explicit Utf8Iterator(const char* p) : ptr_(p) {}

    char32_t operator*() const {
        unsigned char c = *ptr_;
        if ((c & 0x80) == 0) return c;
        if ((c & 0xE0) == 0xC0) {
            return ((c & 0x1F) << 6) | (ptr_[1] & 0x3F);
        }
        // ...处理3字节和4字节
        return 0;
    }

    Utf8Iterator& operator++() {
        unsigned char c = *ptr_;
        if ((c & 0x80) == 0) ptr_ += 1;
        else if ((c & 0xE0) == 0xC0) ptr_ += 2;
        else if ((c & 0xF0) == 0xE0) ptr_ += 3;
        else if ((c & 0xF8) == 0xF0) ptr_ += 4;
        else ++ptr_;
        return *this;
    }
};
```

### 第四周：正则表达式

**学习目标**：掌握std::regex的使用

**基本使用**：
```cpp
#include <regex>
#include <string>
#include <iostream>

// 匹配
std::string text = "Hello, World!";
std::regex pattern(R"(\w+)");

if (std::regex_search(text, pattern)) {
    std::cout << "Found match\n";
}

// 提取匹配
std::smatch matches;
if (std::regex_search(text, matches, pattern)) {
    std::cout << "Match: " << matches[0] << "\n";
}

// 遍历所有匹配
std::sregex_iterator begin(text.begin(), text.end(), pattern);
std::sregex_iterator end;
for (auto it = begin; it != end; ++it) {
    std::cout << "Found: " << (*it)[0] << "\n";
}

// 替换
std::string result = std::regex_replace(text, pattern, "[$&]");
// result = "[Hello], [World]!"

// 验证
std::regex email_pattern(R"([\w.]+@[\w.]+\.\w+)");
bool valid = std::regex_match("user@example.com", email_pattern);
```

**性能注意**：
```cpp
// std::regex编译开销大，应该重用
// 错误做法
for (const auto& line : lines) {
    std::regex pat(R"(\d+)");  // 每次循环都编译正则
    std::regex_search(line, pat);
}

// 正确做法
std::regex pat(R"(\d+)");  // 编译一次
for (const auto& line : lines) {
    std::regex_search(line, pat);
}

// std::regex性能较差，考虑替代方案
// - RE2（Google，线性时间保证）
// - PCRE2
// - Boost.Regex
// - 手写状态机（性能关键时）
```

---

## 源码阅读任务

### 深度阅读清单

- [ ] `std::string`的SSO实现（libstdc++或libc++）
- [ ] `std::string_view`实现
- [ ] `std::char_traits`特化
- [ ] `std::basic_regex`基本结构

---

## 实践项目

### 项目：实现字符串处理库

#### Part 1: mini_string（带SSO）
```cpp
// mini_string.hpp
#pragma once
#include <cstring>
#include <algorithm>
#include <stdexcept>

class MiniString {
    static constexpr size_t SSO_CAPACITY = 15;  // 不含null终止符

    union {
        struct {
            char* ptr;
            size_t size;
            size_t capacity;
        } heap_;

        struct {
            char data[SSO_CAPACITY + 1];
        } sso_;
    };

    // 使用最高字节的最高位作为标志
    // 短字符串：sso_.data[SSO_CAPACITY] 的最高位为0
    // 长字符串：设置标志

    bool is_short() const {
        return (sso_.data[SSO_CAPACITY] & 0x80) == 0;
    }

    void set_short_size(size_t n) {
        sso_.data[SSO_CAPACITY] = static_cast<char>(SSO_CAPACITY - n);
    }

    size_t short_size() const {
        return SSO_CAPACITY - static_cast<unsigned char>(sso_.data[SSO_CAPACITY]);
    }

    void set_long() {
        sso_.data[SSO_CAPACITY] |= 0x80;
    }

public:
    // 默认构造
    MiniString() noexcept {
        sso_.data[0] = '\0';
        set_short_size(0);
    }

    // C字符串构造
    MiniString(const char* s) : MiniString(s, std::strlen(s)) {}

    MiniString(const char* s, size_t len) {
        if (len <= SSO_CAPACITY) {
            std::memcpy(sso_.data, s, len);
            sso_.data[len] = '\0';
            set_short_size(len);
        } else {
            heap_.ptr = new char[len + 1];
            std::memcpy(heap_.ptr, s, len + 1);
            heap_.size = len;
            heap_.capacity = len;
            set_long();
        }
    }

    // 拷贝构造
    MiniString(const MiniString& other) {
        if (other.is_short()) {
            std::memcpy(&sso_, &other.sso_, sizeof(sso_));
        } else {
            heap_.ptr = new char[other.heap_.capacity + 1];
            std::memcpy(heap_.ptr, other.heap_.ptr, other.heap_.size + 1);
            heap_.size = other.heap_.size;
            heap_.capacity = other.heap_.capacity;
            set_long();
        }
    }

    // 移动构造
    MiniString(MiniString&& other) noexcept {
        if (other.is_short()) {
            std::memcpy(&sso_, &other.sso_, sizeof(sso_));
        } else {
            heap_ = other.heap_;
            set_long();
            // 将other置于有效的短字符串状态
            other.sso_.data[0] = '\0';
            other.set_short_size(0);
        }
    }

    // 析构
    ~MiniString() {
        if (!is_short()) {
            delete[] heap_.ptr;
        }
    }

    // 赋值
    MiniString& operator=(MiniString other) noexcept {
        swap(*this, other);
        return *this;
    }

    friend void swap(MiniString& a, MiniString& b) noexcept {
        char temp[sizeof(MiniString)];
        std::memcpy(temp, &a, sizeof(MiniString));
        std::memcpy(&a, &b, sizeof(MiniString));
        std::memcpy(&b, temp, sizeof(MiniString));
    }

    // 访问
    const char* c_str() const noexcept {
        return is_short() ? sso_.data : heap_.ptr;
    }

    const char* data() const noexcept { return c_str(); }

    size_t size() const noexcept {
        return is_short() ? short_size() : heap_.size;
    }

    size_t length() const noexcept { return size(); }

    size_t capacity() const noexcept {
        return is_short() ? SSO_CAPACITY : heap_.capacity;
    }

    bool empty() const noexcept { return size() == 0; }

    char& operator[](size_t pos) {
        return is_short() ? sso_.data[pos] : heap_.ptr[pos];
    }

    const char& operator[](size_t pos) const {
        return is_short() ? sso_.data[pos] : heap_.ptr[pos];
    }

    // 修改
    void reserve(size_t new_cap) {
        if (new_cap <= capacity()) return;

        char* new_ptr = new char[new_cap + 1];
        std::memcpy(new_ptr, c_str(), size() + 1);

        if (!is_short()) {
            delete[] heap_.ptr;
        }

        heap_.ptr = new_ptr;
        heap_.size = size();  // 保存size（在修改前）
        heap_.capacity = new_cap;
        set_long();
    }

    MiniString& operator+=(const MiniString& other) {
        return append(other.c_str(), other.size());
    }

    MiniString& operator+=(const char* s) {
        return append(s, std::strlen(s));
    }

    MiniString& operator+=(char c) {
        push_back(c);
        return *this;
    }

    void push_back(char c) {
        size_t sz = size();
        if (sz >= capacity()) {
            reserve(std::max(capacity() * 2, size_t(16)));
        }

        if (is_short()) {
            sso_.data[sz] = c;
            sso_.data[sz + 1] = '\0';
            set_short_size(sz + 1);
        } else {
            heap_.ptr[sz] = c;
            heap_.ptr[sz + 1] = '\0';
            heap_.size = sz + 1;
        }
    }

    MiniString& append(const char* s, size_t len) {
        size_t sz = size();
        size_t new_size = sz + len;

        if (new_size > capacity()) {
            reserve(std::max(new_size, capacity() * 2));
        }

        char* dst = is_short() ? sso_.data : heap_.ptr;
        std::memcpy(dst + sz, s, len);
        dst[new_size] = '\0';

        if (is_short()) {
            set_short_size(new_size);
        } else {
            heap_.size = new_size;
        }

        return *this;
    }

    void clear() noexcept {
        if (is_short()) {
            sso_.data[0] = '\0';
            set_short_size(0);
        } else {
            heap_.ptr[0] = '\0';
            heap_.size = 0;
        }
    }

    // 比较
    friend bool operator==(const MiniString& a, const MiniString& b) {
        return a.size() == b.size() &&
               std::memcmp(a.c_str(), b.c_str(), a.size()) == 0;
    }

    friend bool operator!=(const MiniString& a, const MiniString& b) {
        return !(a == b);
    }

    friend bool operator<(const MiniString& a, const MiniString& b) {
        return std::lexicographical_compare(
            a.c_str(), a.c_str() + a.size(),
            b.c_str(), b.c_str() + b.size());
    }
};

MiniString operator+(const MiniString& a, const MiniString& b) {
    MiniString result;
    result.reserve(a.size() + b.size());
    result += a;
    result += b;
    return result;
}
```

#### Part 2: mini_string_view
```cpp
// mini_string_view.hpp
#pragma once
#include <cstring>
#include <stdexcept>
#include <algorithm>

class MiniStringView {
    const char* data_ = nullptr;
    size_t size_ = 0;

public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    constexpr MiniStringView() noexcept = default;

    constexpr MiniStringView(const char* s)
        : data_(s), size_(s ? std::char_traits<char>::length(s) : 0) {}

    constexpr MiniStringView(const char* s, size_t len)
        : data_(s), size_(len) {}

    // 从MiniString隐式转换
    MiniStringView(const MiniString& s) : data_(s.c_str()), size_(s.size()) {}

    // 迭代器
    constexpr const char* begin() const noexcept { return data_; }
    constexpr const char* end() const noexcept { return data_ + size_; }
    constexpr const char* cbegin() const noexcept { return begin(); }
    constexpr const char* cend() const noexcept { return end(); }

    // 访问
    constexpr const char* data() const noexcept { return data_; }
    constexpr size_t size() const noexcept { return size_; }
    constexpr size_t length() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr const char& operator[](size_t pos) const { return data_[pos]; }

    constexpr const char& at(size_t pos) const {
        if (pos >= size_) {
            throw std::out_of_range("MiniStringView::at");
        }
        return data_[pos];
    }

    constexpr const char& front() const { return data_[0]; }
    constexpr const char& back() const { return data_[size_ - 1]; }

    // 修改器（只修改视图，不修改原数据）
    constexpr void remove_prefix(size_t n) {
        data_ += n;
        size_ -= n;
    }

    constexpr void remove_suffix(size_t n) {
        size_ -= n;
    }

    // 子串
    constexpr MiniStringView substr(size_t pos = 0, size_t count = npos) const {
        if (pos > size_) {
            throw std::out_of_range("MiniStringView::substr");
        }
        return MiniStringView(data_ + pos, std::min(count, size_ - pos));
    }

    // 查找
    constexpr size_t find(char c, size_t pos = 0) const noexcept {
        for (size_t i = pos; i < size_; ++i) {
            if (data_[i] == c) return i;
        }
        return npos;
    }

    constexpr size_t find(MiniStringView sv, size_t pos = 0) const noexcept {
        if (sv.empty()) return pos <= size_ ? pos : npos;
        if (sv.size_ > size_) return npos;

        for (size_t i = pos; i <= size_ - sv.size_; ++i) {
            bool match = true;
            for (size_t j = 0; j < sv.size_; ++j) {
                if (data_[i + j] != sv[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return i;
        }
        return npos;
    }

    constexpr size_t rfind(char c, size_t pos = npos) const noexcept {
        if (empty()) return npos;
        size_t start = std::min(pos, size_ - 1);
        for (size_t i = start + 1; i > 0; --i) {
            if (data_[i - 1] == c) return i - 1;
        }
        return npos;
    }

    constexpr bool starts_with(MiniStringView sv) const noexcept {
        return size_ >= sv.size_ &&
               std::char_traits<char>::compare(data_, sv.data_, sv.size_) == 0;
    }

    constexpr bool ends_with(MiniStringView sv) const noexcept {
        return size_ >= sv.size_ &&
               std::char_traits<char>::compare(
                   data_ + size_ - sv.size_, sv.data_, sv.size_) == 0;
    }

    constexpr bool contains(MiniStringView sv) const noexcept {
        return find(sv) != npos;
    }

    // 比较
    constexpr int compare(MiniStringView sv) const noexcept {
        size_t len = std::min(size_, sv.size_);
        int result = std::char_traits<char>::compare(data_, sv.data_, len);
        if (result != 0) return result;
        if (size_ < sv.size_) return -1;
        if (size_ > sv.size_) return 1;
        return 0;
    }

    friend constexpr bool operator==(MiniStringView a, MiniStringView b) noexcept {
        return a.size_ == b.size_ &&
               std::char_traits<char>::compare(a.data_, b.data_, a.size_) == 0;
    }

    friend constexpr bool operator!=(MiniStringView a, MiniStringView b) noexcept {
        return !(a == b);
    }

    friend constexpr bool operator<(MiniStringView a, MiniStringView b) noexcept {
        return a.compare(b) < 0;
    }
};
```

#### Part 3: 字符串工具函数
```cpp
// string_utils.hpp
#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <algorithm>

namespace string_utils {

// 分割字符串
std::vector<std::string_view> split(std::string_view str,
                                    std::string_view delim) {
    std::vector<std::string_view> result;
    size_t start = 0;

    while (start < str.size()) {
        size_t end = str.find(delim, start);
        if (end == std::string_view::npos) {
            result.push_back(str.substr(start));
            break;
        }
        result.push_back(str.substr(start, end - start));
        start = end + delim.size();
    }

    return result;
}

// 去除前后空白
std::string_view trim(std::string_view str) {
    auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    size_t start = 0;
    while (start < str.size() && is_space(str[start])) ++start;

    size_t end = str.size();
    while (end > start && is_space(str[end - 1])) --end;

    return str.substr(start, end - start);
}

// 连接字符串
template <typename Container>
std::string join(const Container& parts, std::string_view delim) {
    if (parts.empty()) return "";

    std::string result;
    auto it = parts.begin();
    result = *it++;

    for (; it != parts.end(); ++it) {
        result += delim;
        result += *it;
    }

    return result;
}

// 大小写转换
std::string to_lower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string to_upper(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

// 替换所有
std::string replace_all(std::string_view str,
                        std::string_view from,
                        std::string_view to) {
    std::string result;
    result.reserve(str.size());

    size_t pos = 0;
    while (pos < str.size()) {
        size_t found = str.find(from, pos);
        if (found == std::string_view::npos) {
            result += str.substr(pos);
            break;
        }
        result += str.substr(pos, found - pos);
        result += to;
        pos = found + from.size();
    }

    return result;
}

// 格式化（简单版本）
template <typename... Args>
std::string format(std::string_view fmt, Args&&... args) {
    // 简化实现，使用snprintf
    char buffer[1024];
    int len = std::snprintf(buffer, sizeof(buffer), fmt.data(),
                            std::forward<Args>(args)...);
    return std::string(buffer, len > 0 ? len : 0);
}

} // namespace string_utils
```

---

## 检验标准

### 知识检验
- [ ] 解释SSO的原理和好处
- [ ] std::string_view的生命周期陷阱有哪些？
- [ ] UTF-8编码的规则是什么？如何计算字符数？
- [ ] std::regex的性能问题是什么？有什么替代方案？

### 实践检验
- [ ] MiniString正确实现SSO
- [ ] MiniStringView安全且功能完整
- [ ] 字符串工具函数正确处理边界情况

### 输出物
1. `mini_string.hpp`（带SSO）
2. `mini_string_view.hpp`
3. `string_utils.hpp`
4. `test_strings.cpp`
5. `notes/month10_strings.md`

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习 | 30小时 | 21% |
| 源码阅读 | 25小时 | 18% |
| MiniString实现 | 35小时 | 25% |
| MiniStringView实现 | 25小时 | 18% |
| 工具函数与测试 | 25小时 | 18% |

---

## 下月预告

Month 11将学习**时间库与chrono**，深入理解C++的时间表示、duration、time_point，以及时钟类型和时间计算。
