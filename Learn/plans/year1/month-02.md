# Month 02: 调试器精通——GDB/LLDB作为显微镜

## 本月主题概述

调试器不仅是修Bug的工具，更是理解程序运行时行为的"显微镜"。本月目标是精通GDB/LLDB，学会观察内存布局、虚函数表、对象生命周期等底层细节，培养"看穿抽象"的能力。

---

## 理论学习内容

### 第一周：调试器基础与工作原理

**学习目标**：理解调试器如何工作

**阅读材料**：
- [ ] 《Debugging with GDB》官方手册第1-5章
- [ ] 博客：How debuggers work (Eli Bendersky系列)
- [ ] LLDB官方教程：https://lldb.llvm.org/use/tutorial.html

**核心概念**：
1. ptrace系统调用：调试器的基石
2. 断点实现原理：INT 3指令替换
3. 单步执行的硬件支持：EFLAGS TF位
4. DWARF调试信息格式

**动手实验**：
```bash
# 编译带调试信息的程序
g++ -g -O0 main.cpp -o main

# 查看DWARF信息
readelf --debug-dump=info main | head -100

# 查看符号表
nm main
objdump -t main
```

### 第二周：GDB/LLDB核心命令精通

**学习目标**：熟练使用调试器进行程序分析

**命令分类学习**：

#### 执行控制
```gdb
# GDB
run [args]              # 运行程序
start                   # 运行到main暂停
continue (c)            # 继续执行
next (n)                # 单步（跳过函数）
step (s)                # 单步（进入函数）
finish                  # 执行到当前函数返回
until <line>            # 执行到指定行
```

#### 断点管理
```gdb
break <location>        # 设置断点
break func if x > 10    # 条件断点
watch <expr>            # 数据断点（写入时触发）
rwatch <expr>           # 读取断点
awatch <expr>           # 读写断点
catch throw             # 捕获异常抛出
info breakpoints        # 查看所有断点
delete <num>            # 删除断点
disable/enable <num>    # 禁用/启用断点
```

#### 数据检查
```gdb
print (p) <expr>        # 打印表达式值
print/x <expr>          # 十六进制格式
print *array@10         # 打印数组前10个元素
x/<n><f><u> <addr>      # 检查内存
  # n: 数量, f: 格式(x/d/s/i), u: 单位(b/h/w/g)
ptype <type>            # 打印类型信息
info locals             # 查看局部变量
info args               # 查看函数参数
```

#### LLDB等效命令
```lldb
# LLDB语法更一致
frame variable          # 查看局部变量
expression <expr>       # 计算表达式
memory read <addr>      # 读取内存
register read           # 查看寄存器
```

### 第三周：内存与对象布局分析

**学习目标**：使用调试器观察C++对象的内存布局

**实践任务1：观察std::string的SSO**
```cpp
#include <string>

int main() {
    std::string short_str = "hello";      // SSO
    std::string long_str = "this is a very long string that exceeds SSO";
    return 0;
}
```
```gdb
# 在main设断点后
(gdb) p short_str
(gdb) p &short_str
(gdb) x/32xb &short_str    # 查看对象的原始字节
(gdb) p long_str
(gdb) x/32xb &long_str
# 对比两者的内存布局差异
```

**实践任务2：观察虚函数表**
```cpp
class Base {
public:
    virtual void foo() { }
    virtual void bar() { }
    int x = 1;
};

class Derived : public Base {
public:
    void foo() override { }
    virtual void baz() { }
    int y = 2;
};

int main() {
    Base* b = new Derived();
    b->foo();
    return 0;
}
```
```gdb
(gdb) p *b
(gdb) p /a *(void**)b           # 打印vtable指针
(gdb) x/4a *(void**)b           # 查看vtable内容
(gdb) info vtbl b               # GDB扩展命令
```

**实践任务3：观察shared_ptr控制块**
```cpp
#include <memory>

int main() {
    auto sp1 = std::make_shared<int>(42);
    auto sp2 = sp1;
    auto wp = std::weak_ptr<int>(sp1);
    return 0;
}
```
```gdb
# 观察控制块的引用计数
(gdb) p sp1
(gdb) p sp1._M_ptr
(gdb) p *sp1._M_refcount._M_pi
# 查看strong_count和weak_count
```

### 第四周：高级调试技术

**学习目标**：掌握逆向执行、多线程调试、core dump分析

#### 逆向调试（Reverse Debugging）
```gdb
# 需要record功能
(gdb) record
(gdb) continue
# 程序崩溃后
(gdb) reverse-continue   # 反向执行
(gdb) reverse-step       # 反向单步
(gdb) reverse-next
```

#### 多线程调试
```gdb
(gdb) info threads                  # 查看所有线程
(gdb) thread <id>                   # 切换线程
(gdb) thread apply all bt           # 所有线程的调用栈
(gdb) set scheduler-locking on      # 只运行当前线程
```

#### Core Dump分析
```bash
# 启用core dump
ulimit -c unlimited

# 分析core文件
gdb ./program core
(gdb) bt                            # 查看崩溃时的调用栈
(gdb) frame <n>                     # 切换到特定帧
(gdb) info registers                # 查看寄存器状态
```

---

## 源码阅读任务

### 本月源码目标：std::shared_ptr控制块实现

**阅读路径**：
- GCC libstdc++: `bits/shared_ptr_base.h`
- 重点关注`_Sp_counted_base`类

**分析要点**：
1. [ ] 控制块的内存布局（strong_count, weak_count）
2. [ ] 原子操作的使用（`__atomic_add_fetch`）
3. [ ] `make_shared`如何实现单次分配优化
4. [ ] weak_ptr如何避免悬空指针

---

## 实践项目

### 项目：内存分析工具集

**目标**：编写一组工具函数和GDB脚本，用于分析C++程序的内存行为

#### Part 1: 内存布局打印器
```cpp
// memory_inspector.hpp
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iomanip>

template <typename T>
void dump_memory(const T& obj, const char* name = "object") {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&obj);
    std::cout << "Memory dump of " << name
              << " at " << static_cast<const void*>(ptr)
              << " (size: " << sizeof(T) << " bytes)\n";

    for (size_t i = 0; i < sizeof(T); ++i) {
        if (i % 16 == 0) {
            std::cout << std::hex << std::setw(4) << i << ": ";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(ptr[i]) << " ";
        if (i % 16 == 15) std::cout << "\n";
    }
    std::cout << std::dec << std::setfill(' ') << "\n";
}

// 打印对象的vtable信息
template <typename T>
void dump_vtable(const T& obj) {
    static_assert(std::is_polymorphic_v<T>, "T must be polymorphic");
    void** vtable_ptr = *reinterpret_cast<void***>(const_cast<T*>(&obj));
    std::cout << "VTable pointer: " << vtable_ptr << "\n";
    // 打印前几个虚函数地址
    for (int i = 0; i < 5; ++i) {
        std::cout << "  [" << i << "]: " << vtable_ptr[i] << "\n";
    }
}
```

#### Part 2: GDB Python脚本
```python
# cpp_inspector.py - 放入 ~/.gdbinit 或单独加载

import gdb

class DumpVectorCmd(gdb.Command):
    """Dump std::vector contents with memory info"""

    def __init__(self):
        super().__init__("dump_vector", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        try:
            val = gdb.parse_and_eval(arg)
            # 获取vector的内部指针
            start = val['_M_impl']['_M_start']
            finish = val['_M_impl']['_M_finish']
            end_storage = val['_M_impl']['_M_end_of_storage']

            size = finish - start
            capacity = end_storage - start

            print(f"Vector at {val.address}")
            print(f"  Size: {size}")
            print(f"  Capacity: {capacity}")
            print(f"  Data pointer: {start}")
            print(f"  Elements:")

            for i in range(min(int(size), 10)):
                elem = start[i]
                print(f"    [{i}]: {elem}")

            if size > 10:
                print(f"    ... ({size - 10} more elements)")

        except Exception as e:
            print(f"Error: {e}")

DumpVectorCmd()

class ShowLayoutCmd(gdb.Command):
    """Show memory layout of a type"""

    def __init__(self):
        super().__init__("show_layout", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        try:
            t = gdb.lookup_type(arg)
            print(f"Type: {t}")
            print(f"Size: {t.sizeof} bytes")

            if t.code == gdb.TYPE_CODE_STRUCT or t.code == gdb.TYPE_CODE_CLASS:
                print("Fields:")
                for field in t.fields():
                    offset = field.bitpos // 8 if hasattr(field, 'bitpos') else '?'
                    print(f"  +{offset}: {field.name} ({field.type})")

        except Exception as e:
            print(f"Error: {e}")

ShowLayoutCmd()
```

#### Part 3: 内存泄漏检测器（简易版）
```cpp
// leak_detector.hpp
#include <unordered_map>
#include <iostream>
#include <cstdlib>

class LeakDetector {
public:
    static LeakDetector& instance() {
        static LeakDetector inst;
        return inst;
    }

    void record_alloc(void* ptr, size_t size, const char* file, int line) {
        allocations_[ptr] = {size, file, line};
    }

    void record_free(void* ptr) {
        allocations_.erase(ptr);
    }

    void report() {
        if (allocations_.empty()) {
            std::cout << "No memory leaks detected!\n";
            return;
        }

        std::cout << "Memory leaks detected:\n";
        size_t total = 0;
        for (const auto& [ptr, info] : allocations_) {
            std::cout << "  " << ptr << ": " << info.size << " bytes"
                      << " (allocated at " << info.file << ":" << info.line << ")\n";
            total += info.size;
        }
        std::cout << "Total leaked: " << total << " bytes\n";
    }

private:
    struct AllocInfo {
        size_t size;
        const char* file;
        int line;
    };
    std::unordered_map<void*, AllocInfo> allocations_;
};

// 宏定义，用于追踪分配
#ifdef ENABLE_LEAK_DETECTION
#define TRACK_NEW(ptr, size) \
    LeakDetector::instance().record_alloc(ptr, size, __FILE__, __LINE__)
#define TRACK_DELETE(ptr) \
    LeakDetector::instance().record_free(ptr)
#else
#define TRACK_NEW(ptr, size)
#define TRACK_DELETE(ptr)
#endif
```

---

## 检验标准

### 知识检验
能够回答以下问题：
- [ ] 调试器如何实现断点？解释INT 3的作用
- [ ] 什么是DWARF调试信息？为什么需要`-g`编译选项？
- [ ] std::string的SSO（小字符串优化）是什么？如何用调试器验证？
- [ ] std::shared_ptr的控制块包含哪些信息？

### 实践检验
- [ ] 能够使用GDB/LLDB观察任意C++对象的内存布局
- [ ] 能够分析虚函数调用的vtable查找过程
- [ ] 能够分析core dump定位崩溃原因
- [ ] 完成内存分析工具集的开发

### 输出物
1. `memory_inspector.hpp` 内存分析工具
2. `cpp_inspector.py` GDB Python扩展
3. `notes/month02_debugging.md` 调试技术笔记
4. 至少3个调试案例分析报告

---

## 时间分配（140小时/月）

| 内容 | 时间 | 占比 |
|------|------|------|
| 理论学习（调试器原理） | 35小时 | 25% |
| 命令练习与实践 | 45小时 | 32% |
| 源码阅读（shared_ptr） | 25小时 | 18% |
| 工具开发 | 25小时 | 18% |
| 笔记与复习 | 10小时 | 7% |

---

## 下月预告

Month 03将开始**STL容器源码深度分析**，系统性地阅读`std::map`（红黑树）、`std::unordered_map`（哈希表）的实现，理解不同容器的性能特性和设计权衡。
