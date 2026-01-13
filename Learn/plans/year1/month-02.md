# Month 02: 调试器精通——GDB/LLDB作为显微镜

## 本月主题概述

调试器不仅是修Bug的工具，更是理解程序运行时行为的"显微镜"。本月目标是精通GDB/LLDB，学会观察内存布局、虚函数表、对象生命周期等底层细节，培养"看穿抽象"的能力。

---

## 第一周：调试器基础与工作原理（35小时）

### 学习目标
- 理解调试器的底层实现机制
- 掌握ptrace系统调用的工作原理
- 理解DWARF调试信息格式

### Day 1-2：调试器原理总览（10小时）

#### 阅读材料
- [ ] 《Debugging with GDB》官方手册第1-5章
- [ ] Eli Bendersky博客系列：[How debuggers work](https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1)
  - Part 1: Basics
  - Part 2: Breakpoints
  - Part 3: Debugging information
- [ ] LLDB官方教程：https://lldb.llvm.org/use/tutorial.html

#### 核心概念笔记

**1. ptrace系统调用——调试器的基石**

```c
// ptrace 函数原型
long ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data);

// 关键操作
PTRACE_TRACEME    // 子进程调用，允许父进程调试自己
PTRACE_ATTACH     // 附加到已运行的进程
PTRACE_PEEKTEXT   // 读取被调试进程的内存
PTRACE_POKETEXT   // 写入被调试进程的内存
PTRACE_GETREGS    // 获取寄存器状态
PTRACE_SETREGS    // 设置寄存器状态
PTRACE_CONT       // 继续执行
PTRACE_SINGLESTEP // 单步执行
```

**2. 断点实现原理——INT 3指令替换**

```
原始代码:
0x400500:  48 89 e5    mov rbp, rsp
0x400503:  89 7d fc    mov [rbp-4], edi

设置断点后:
0x400500:  CC          int 3        <- 替换第一个字节
0x400501:  89 e5       (被破坏)
0x400503:  89 7d fc    mov [rbp-4], edi

断点触发流程:
1. CPU执行到0x400500，遇到INT 3 (0xCC)
2. 触发SIGTRAP信号
3. 调试器收到信号，暂停被调试进程
4. 调试器恢复原始字节(48)，准备继续执行
```

**3. 单步执行的硬件支持——EFLAGS TF位**

```
EFLAGS寄存器中的TF(Trap Flag)位:
- TF=1: CPU每执行一条指令后触发调试异常
- 调试器通过PTRACE_SINGLESTEP设置TF位
- 执行一条指令后自动清除TF，触发SIGTRAP
```

#### 动手实验1：观察ptrace调用

```c
// mini_debugger.c - 最简调试器
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <program>\n", argv[0]);
        return 1;
    }

    pid_t child = fork();

    if (child == 0) {
        // 子进程：请求被追踪
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl(argv[1], argv[1], NULL);
    } else {
        // 父进程：调试器
        int status;
        struct user_regs_struct regs;

        wait(&status);  // 等待子进程停止

        while (WIFSTOPPED(status)) {
            // 获取寄存器
            ptrace(PTRACE_GETREGS, child, NULL, &regs);
            printf("RIP: 0x%llx\n", regs.rip);

            // 单步执行
            ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
            wait(&status);
        }
    }
    return 0;
}
```

### Day 3-4：DWARF调试信息深入（10小时）

#### 核心概念

**DWARF (Debugging With Attributed Record Formats)**

```
DWARF信息包含:
├── .debug_info     # 类型、变量、函数的描述
├── .debug_abbrev   # 缩写表
├── .debug_line     # 源码行号映射
├── .debug_frame    # 调用帧信息(栈展开)
├── .debug_loc      # 变量位置描述
└── .debug_ranges   # 地址范围
```

#### 动手实验2：探索DWARF信息

```bash
# 创建测试程序
cat > dwarf_test.cpp << 'EOF'
struct Point {
    int x;
    int y;
    Point(int a, int b) : x(a), y(b) {}
};

int add(int a, int b) {
    return a + b;
}

int main() {
    Point p(10, 20);
    int result = add(p.x, p.y);
    return result;
}
EOF

# 编译（对比不同选项）
g++ -g0 dwarf_test.cpp -o test_no_debug      # 无调试信息
g++ -g1 dwarf_test.cpp -o test_minimal       # 最小调试信息
g++ -g2 dwarf_test.cpp -o test_default       # 默认调试信息
g++ -g3 dwarf_test.cpp -o test_full          # 完整调试信息(含宏)

# 对比文件大小
ls -la test_*

# 查看DWARF信息
readelf --debug-dump=info test_default | head -200

# 查看行号信息
readelf --debug-dump=line test_default

# 使用dwarfdump (更友好的格式)
# macOS: dwarfdump test_default
# Linux: 安装 dwarfdump 或使用 llvm-dwarfdump

# 查看符号表
nm test_default
objdump -t test_default | grep -E "(main|add|Point)"
```

#### 扩展阅读
- [ ] DWARF标准文档（选读重要章节）
- [ ] 理解 `-gsplit-dwarf` 和 `.dwo` 文件

### Day 5-7：构建简易调试器（15小时）

#### 项目：Mini Debugger

```cpp
// mini_debugger.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

class MiniDebugger {
public:
    MiniDebugger(std::string prog_name, pid_t pid)
        : m_prog_name(std::move(prog_name)), m_pid(pid) {}

    void run();
    void handle_command(const std::string& line);

    void continue_execution();
    void set_breakpoint(std::intptr_t addr);
    void dump_registers();
    void read_memory(std::intptr_t addr, size_t size);
    void single_step();
    void print_backtrace();

private:
    std::string m_prog_name;
    pid_t m_pid;

    struct Breakpoint {
        std::intptr_t addr;
        uint8_t saved_data;  // 保存被替换的原始字节
        bool enabled;
    };
    std::unordered_map<std::intptr_t, Breakpoint> m_breakpoints;

    void wait_for_signal();
    uint64_t read_word(std::intptr_t addr);
    void write_word(std::intptr_t addr, uint64_t data);
};
```

```cpp
// mini_debugger.cpp
#include "mini_debugger.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

void MiniDebugger::set_breakpoint(std::intptr_t addr) {
    // 读取该地址的原始数据
    uint64_t data = read_word(addr);
    uint8_t saved_byte = static_cast<uint8_t>(data & 0xFF);

    // 用INT 3 (0xCC) 替换第一个字节
    uint64_t int3_data = ((data & ~0xFF) | 0xCC);
    write_word(addr, int3_data);

    // 保存断点信息
    m_breakpoints[addr] = {addr, saved_byte, true};
    std::cout << "Breakpoint set at 0x" << std::hex << addr << std::endl;
}

void MiniDebugger::continue_execution() {
    // 检查是否在断点上，如果是，先单步跳过
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, m_pid, nullptr, &regs);

    auto bp_it = m_breakpoints.find(regs.rip - 1);
    if (bp_it != m_breakpoints.end() && bp_it->second.enabled) {
        // 恢复原始指令
        auto& bp = bp_it->second;
        uint64_t data = read_word(bp.addr);
        uint64_t restored = ((data & ~0xFF) | bp.saved_data);
        write_word(bp.addr, restored);

        // 回退RIP
        regs.rip -= 1;
        ptrace(PTRACE_SETREGS, m_pid, nullptr, &regs);

        // 单步执行
        ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
        wait_for_signal();

        // 重新设置断点
        data = read_word(bp.addr);
        write_word(bp.addr, (data & ~0xFF) | 0xCC);
    }

    ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);
    wait_for_signal();
}

void MiniDebugger::dump_registers() {
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, m_pid, nullptr, &regs);

    std::cout << std::hex
              << "rax: 0x" << std::setw(16) << std::setfill('0') << regs.rax << "\n"
              << "rbx: 0x" << std::setw(16) << regs.rbx << "\n"
              << "rcx: 0x" << std::setw(16) << regs.rcx << "\n"
              << "rdx: 0x" << std::setw(16) << regs.rdx << "\n"
              << "rdi: 0x" << std::setw(16) << regs.rdi << "\n"
              << "rsi: 0x" << std::setw(16) << regs.rsi << "\n"
              << "rbp: 0x" << std::setw(16) << regs.rbp << "\n"
              << "rsp: 0x" << std::setw(16) << regs.rsp << "\n"
              << "rip: 0x" << std::setw(16) << regs.rip << "\n"
              << std::dec;
}

uint64_t MiniDebugger::read_word(std::intptr_t addr) {
    return ptrace(PTRACE_PEEKDATA, m_pid, addr, nullptr);
}

void MiniDebugger::write_word(std::intptr_t addr, uint64_t data) {
    ptrace(PTRACE_POKEDATA, m_pid, addr, data);
}

void MiniDebugger::wait_for_signal() {
    int status;
    waitpid(m_pid, &status, 0);

    if (WIFEXITED(status)) {
        std::cout << "Process exited with code " << WEXITSTATUS(status) << std::endl;
    } else if (WIFSTOPPED(status)) {
        std::cout << "Process stopped by signal " << WSTOPSIG(status) << std::endl;
    }
}
```

### 第一周检验清单

- [ ] 能解释ptrace的基本操作流程
- [ ] 能说明INT 3断点的实现原理
- [ ] 能使用readelf/objdump查看调试信息
- [ ] 完成mini debugger的基础框架
- [ ] 笔记：`notes/week01_debugger_internals.md`

---

## 第二周：GDB/LLDB核心命令精通（45小时）

### 学习目标
- 熟练掌握执行控制、断点管理、数据检查命令
- 建立GDB与LLDB命令的映射关系
- 形成调试问题的系统化方法论

### Day 1-2：执行控制命令（12小时）

#### GDB命令详解与实践

```gdb
# ===== 启动与执行 =====
# 启动程序
(gdb) file ./myprogram              # 加载可执行文件
(gdb) run arg1 arg2                 # 运行程序（带参数）
(gdb) run < input.txt               # 重定向输入
(gdb) run > output.txt 2>&1         # 重定向输出

# 附加到运行中的进程
(gdb) attach <pid>
(gdb) detach

# 设置环境变量
(gdb) set environment LD_PRELOAD=./libhook.so
(gdb) show environment

# ===== 单步执行 =====
(gdb) start                         # 运行到main第一行停止
(gdb) next (n)                      # 单步，不进入函数
(gdb) step (s)                      # 单步，进入函数
(gdb) nexti (ni)                    # 汇编级单步，不进入
(gdb) stepi (si)                    # 汇编级单步，进入

# 高级单步
(gdb) finish                        # 执行到当前函数返回
(gdb) until                         # 执行到当前循环结束
(gdb) until <linenum>               # 执行到指定行
(gdb) advance <location>            # 执行到指定位置

# ===== 继续与跳转 =====
(gdb) continue (c)                  # 继续执行
(gdb) jump <location>               # 跳转到指定位置（危险！）
(gdb) signal SIGCONT                # 发送信号并继续
```

#### LLDB等效命令

```lldb
# LLDB采用更一致的命令结构: <noun> <verb> [options]

# 启动
(lldb) target create ./myprogram
(lldb) process launch -- arg1 arg2
(lldb) run arg1 arg2                # 简写

# 附加
(lldb) process attach --pid <pid>
(lldb) process attach --name <process-name>

# 单步执行
(lldb) thread step-over             # next
(lldb) thread step-in               # step
(lldb) thread step-out              # finish
(lldb) thread step-inst             # stepi
(lldb) thread step-inst-over        # nexti

# 简写
(lldb) n                            # next
(lldb) s                            # step
(lldb) finish
(lldb) ni
(lldb) si
```

#### 练习程序

```cpp
// step_practice.cpp
#include <iostream>
#include <vector>
#include <algorithm>

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);  // 递归调用
}

void process_vector(std::vector<int>& vec) {
    for (auto& v : vec) {
        v = factorial(v);
    }
}

int main() {
    std::vector<int> numbers = {1, 2, 3, 4, 5};

    std::cout << "Before: ";
    for (int n : numbers) std::cout << n << " ";
    std::cout << "\n";

    process_vector(numbers);

    std::cout << "After: ";
    for (int n : numbers) std::cout << n << " ";
    std::cout << "\n";

    return 0;
}
```

#### 练习任务
```bash
g++ -g -O0 step_practice.cpp -o step_practice

# 练习1: 使用step进入factorial递归
# 练习2: 使用next跳过函数调用
# 练习3: 在递归中使用finish快速返回
# 练习4: 使用until跳出循环
```

### Day 3-4：断点管理精通（12小时）

#### 断点类型详解

```gdb
# ===== 基本断点 =====
(gdb) break main                    # 函数名
(gdb) break 42                      # 当前文件行号
(gdb) break file.cpp:42             # 指定文件行号
(gdb) break *0x400520               # 地址断点

# ===== 条件断点 =====
(gdb) break func if x > 10          # 条件断点
(gdb) break func if strcmp(s,"test")==0  # 字符串比较
(gdb) condition 1 x > 10            # 给已有断点添加条件

# ===== 临时断点 =====
(gdb) tbreak func                   # 触发一次后自动删除

# ===== 数据断点（Watchpoint）=====
(gdb) watch var                     # 写入时触发
(gdb) rwatch var                    # 读取时触发
(gdb) awatch var                    # 读写都触发
(gdb) watch -l var                  # 观察地址（即使变量离开作用域）

# ===== 捕获点（Catchpoint）=====
(gdb) catch throw                   # 捕获异常抛出
(gdb) catch catch                   # 捕获异常被捕获
(gdb) catch throw std::runtime_error  # 捕获特定异常
(gdb) catch syscall write           # 捕获系统调用
(gdb) catch signal SIGSEGV          # 捕获信号

# ===== 断点管理 =====
(gdb) info breakpoints              # 列出所有断点
(gdb) delete 1                      # 删除断点1
(gdb) delete                        # 删除所有断点
(gdb) disable 1                     # 禁用断点1
(gdb) enable 1                      # 启用断点1
(gdb) ignore 1 10                   # 断点1忽略前10次

# ===== 断点命令 =====
(gdb) break func
(gdb) commands 1
> silent                            # 不打印停止信息
> print x
> print y
> continue
> end
```

#### LLDB断点命令

```lldb
# 设置断点
(lldb) breakpoint set --name main
(lldb) breakpoint set --file main.cpp --line 42
(lldb) breakpoint set --address 0x400520
(lldb) br s -n main                 # 简写

# 条件断点
(lldb) breakpoint set --name func --condition 'x > 10'

# 数据断点
(lldb) watchpoint set variable var
(lldb) watchpoint set expression -- &var

# 管理断点
(lldb) breakpoint list
(lldb) breakpoint delete 1
(lldb) breakpoint disable 1
(lldb) breakpoint modify --condition 'x > 20' 1

# 断点命令
(lldb) breakpoint command add 1
> p x
> continue
> DONE
```

#### 练习程序：断点实战

```cpp
// breakpoint_practice.cpp
#include <iostream>
#include <stdexcept>
#include <vector>

class Counter {
public:
    int value = 0;

    void increment() {
        ++value;  // 设置watchpoint观察value的变化
    }

    void add(int n) {
        for (int i = 0; i < n; ++i) {
            increment();
        }
    }
};

void may_throw(int x) {
    if (x < 0) {
        throw std::runtime_error("negative value");
    }
    if (x > 100) {
        throw std::out_of_range("value too large");
    }
}

int main() {
    Counter c;

    // 练习watchpoint
    c.add(5);
    std::cout << "Counter: " << c.value << "\n";

    // 练习catch throw
    std::vector<int> test_values = {10, 50, -5, 200};
    for (int v : test_values) {
        try {
            std::cout << "Testing " << v << "... ";
            may_throw(v);
            std::cout << "OK\n";
        } catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << "\n";
        }
    }

    return 0;
}
```

#### 练习任务
```bash
# 练习1: 在Counter::increment上设置断点，观察调用次数
# 练习2: 设置watchpoint观察c.value的变化
# 练习3: 使用catch throw捕获异常抛出点
# 练习4: 设置条件断点，只在特定条件下停止
```

### Day 5-6：数据检查与内存分析（14小时）

#### 打印与格式化

```gdb
# ===== 基本打印 =====
(gdb) print var                     # 打印变量
(gdb) print/x var                   # 十六进制
(gdb) print/d var                   # 十进制
(gdb) print/t var                   # 二进制
(gdb) print/c var                   # 字符
(gdb) print/f var                   # 浮点数
(gdb) print/a var                   # 地址

# ===== 表达式求值 =====
(gdb) print x + y * 2
(gdb) print func(arg)               # 调用函数！
(gdb) print obj.method()            # 调用方法
(gdb) print (int*)ptr               # 类型转换

# ===== 数组与指针 =====
(gdb) print *array@10               # 打印数组前10个元素
(gdb) print array[0]@10             # 同上
(gdb) print *ptr                    # 解引用
(gdb) print ptr[5]                  # 索引访问

# ===== 结构体与对象 =====
(gdb) print obj                     # 打印整个对象
(gdb) print obj.member              # 成员访问
(gdb) print *this                   # 打印当前对象
(gdb) ptype obj                     # 打印类型信息
(gdb) ptype /o obj                  # 打印类型信息（含偏移量）

# ===== 内存检查 (x命令) =====
# 格式: x/<count><format><size> <address>
# count: 数量
# format: x(hex), d(decimal), u(unsigned), o(octal),
#         t(binary), a(address), c(char), s(string), i(instruction)
# size: b(byte), h(halfword=2bytes), w(word=4bytes), g(giant=8bytes)

(gdb) x/16xb &var                   # 16个字节，十六进制
(gdb) x/4xw &var                    # 4个字（32位），十六进制
(gdb) x/2xg &var                    # 2个双字（64位），十六进制
(gdb) x/10i $rip                    # 从RIP开始的10条指令
(gdb) x/s str_ptr                   # 打印字符串

# ===== 自动显示 =====
(gdb) display var                   # 每次停止时显示var
(gdb) display/x $rax                # 每次显示rax寄存器
(gdb) undisplay 1                   # 取消显示
(gdb) info display                  # 查看所有display

# ===== 变量与作用域 =====
(gdb) info locals                   # 局部变量
(gdb) info args                     # 函数参数
(gdb) info variables                # 全局/静态变量
(gdb) info scope func               # 函数作用域内的变量
```

#### LLDB数据检查

```lldb
# 打印
(lldb) expression var               # 或 p var
(lldb) expression -f x -- var       # 十六进制
(lldb) expression -f b -- var       # 二进制

# 帧变量
(lldb) frame variable               # 所有局部变量
(lldb) frame variable var           # 特定变量
(lldb) frame variable -L var        # 带地址

# 内存读取
(lldb) memory read &var
(lldb) memory read -s 1 -f x -c 16 &var  # 16字节，十六进制
(lldb) memory read -s 4 -f d -c 4 &var   # 4个int，十进制

# 寄存器
(lldb) register read
(lldb) register read rax rbx
(lldb) register read -f d rax       # 十进制格式
```

#### 练习程序：数据检查

```cpp
// data_inspect.cpp
#include <string>
#include <vector>
#include <memory>
#include <cstring>

struct ComplexStruct {
    int id;
    char name[32];
    double values[4];
    int* ptr;

    ComplexStruct() : id(42), ptr(new int(100)) {
        strcpy(name, "test_object");
        for (int i = 0; i < 4; ++i) values[i] = i * 1.5;
    }
    ~ComplexStruct() { delete ptr; }
};

int main() {
    // 基本类型
    int x = 0x12345678;
    float f = 3.14159f;
    double d = 2.71828;

    // 字符串
    const char* cstr = "Hello, World!";
    std::string str = "C++ String";

    // 数组
    int arr[] = {10, 20, 30, 40, 50};

    // 结构体
    ComplexStruct cs;

    // 智能指针
    auto sp = std::make_shared<int>(999);
    auto up = std::make_unique<double>(88.88);

    // 容器
    std::vector<int> vec = {1, 2, 3, 4, 5};

    return 0;  // 在这里设断点，检查所有变量
}
```

### Day 7：调用栈与导航（7小时）

#### 调用栈命令

```gdb
# ===== 回溯 =====
(gdb) backtrace                     # 完整调用栈
(gdb) bt                            # 简写
(gdb) bt 5                          # 只显示5帧
(gdb) bt full                       # 显示每帧的局部变量
(gdb) bt -full                      # 从最内层开始

# ===== 帧切换 =====
(gdb) frame 3                       # 切换到第3帧
(gdb) up                            # 上移一帧（向调用者方向）
(gdb) down                          # 下移一帧（向被调用者方向）
(gdb) up 3                          # 上移3帧

# ===== 帧信息 =====
(gdb) info frame                    # 当前帧详细信息
(gdb) info frame 3                  # 第3帧的详细信息
(gdb) info args                     # 当前帧的参数
(gdb) info locals                   # 当前帧的局部变量

# ===== 源码查看 =====
(gdb) list                          # 显示当前位置源码
(gdb) list 50                       # 显示第50行附近
(gdb) list func                     # 显示函数
(gdb) list file.cpp:100             # 显示指定位置
(gdb) list -                        # 向前翻页
(gdb) set listsize 30               # 设置显示行数
```

### 第二周检验清单

- [ ] 能熟练使用step/next/finish控制执行
- [ ] 能设置各类断点（条件、数据、捕获）
- [ ] 能使用x命令检查任意内存
- [ ] 能使用bt分析调用栈
- [ ] 建立GDB/LLDB命令对照表
- [ ] 笔记：`notes/week02_gdb_commands.md`

---

## 第三周：内存与对象布局分析（40小时）

### 学习目标
- 使用调试器观察C++对象的真实内存布局
- 理解虚函数表(vtable)的结构和查找过程
- 分析标准库容器的内部实现

### Day 1-2：std::string的SSO分析（12小时）

#### 背景知识：Small String Optimization

```
std::string的两种存储模式:

1. SSO模式（短字符串）:
   +------------------+
   | 字符数据 (内联)   |  <- 直接存储在对象内部
   | ...              |
   | size | 标志位    |
   +------------------+

2. 堆分配模式（长字符串）:
   +------------------+
   | 指针 -> 堆内存    |  <- 指向堆上的数据
   | size             |
   | capacity         |
   +------------------+
```

#### 实验程序

```cpp
// sso_analysis.cpp
#include <string>
#include <iostream>
#include <cstring>

void analyze_string(const std::string& s, const char* name) {
    std::cout << "=== " << name << " ===\n";
    std::cout << "Content: \"" << s << "\"\n";
    std::cout << "Length: " << s.length() << "\n";
    std::cout << "Capacity: " << s.capacity() << "\n";
    std::cout << "Data ptr: " << (void*)s.data() << "\n";
    std::cout << "Object addr: " << (void*)&s << "\n";

    // 判断是否使用SSO
    const char* obj_start = reinterpret_cast<const char*>(&s);
    const char* obj_end = obj_start + sizeof(s);
    const char* data = s.data();

    if (data >= obj_start && data < obj_end) {
        std::cout << "Storage: SSO (inline)\n";
    } else {
        std::cout << "Storage: Heap allocated\n";
    }
    std::cout << "\n";
}

int main() {
    // 测试不同长度的字符串
    std::string empty;
    std::string tiny = "Hi";
    std::string small = "Hello";
    std::string medium = "Hello, World!!";   // 14字符
    std::string boundary = "Hello, World!!!"; // 15字符
    std::string longer = "Hello, World!!!!"; // 16字符
    std::string long_str = "This is a very long string that definitely exceeds SSO";

    // 打印sizeof
    std::cout << "sizeof(std::string) = " << sizeof(std::string) << "\n\n";

    analyze_string(empty, "empty");
    analyze_string(tiny, "tiny (2 chars)");
    analyze_string(small, "small (5 chars)");
    analyze_string(medium, "medium (14 chars)");
    analyze_string(boundary, "boundary (15 chars)");
    analyze_string(longer, "longer (16 chars)");
    analyze_string(long_str, "long_str");

    return 0;  // 在这里设断点
}
```

#### GDB调试命令

```gdb
# 编译并启动调试
# g++ -g -O0 sso_analysis.cpp -o sso_analysis
# gdb ./sso_analysis

(gdb) break main
(gdb) run
(gdb) next  # 执行到所有字符串初始化完成

# 查看string对象的原始内存
(gdb) x/32xb &empty
(gdb) x/32xb &small
(gdb) x/32xb &long_str

# 使用ptype查看string的内部结构
(gdb) ptype std::string
(gdb) ptype /o std::string  # 带偏移量

# 查看内部成员 (libstdc++ 实现)
(gdb) p small._M_dataplus
(gdb) p small._M_dataplus._M_p
(gdb) p small._M_string_length

# 对于libstdc++，SSO数据存储在_M_local_buf中
(gdb) p small._M_dataplus._M_p == small._M_local_buf  # SSO检测

# 对比长字符串
(gdb) p long_str._M_dataplus._M_p
(gdb) p long_str._M_allocated_capacity
```

#### 深入分析：不同实现的SSO

```cpp
// 不同标准库的SSO阈值:
// - libstdc++ (GCC): 15字节 (64位系统)
// - libc++ (Clang): 22字节 (64位系统)
// - MSVC STL: 15字节

// 测试实际阈值
#include <string>
#include <iostream>

int main() {
    for (int i = 0; i <= 30; ++i) {
        std::string s(i, 'x');
        const char* obj = reinterpret_cast<const char*>(&s);
        bool is_sso = (s.data() >= obj && s.data() < obj + sizeof(s));
        std::cout << "Length " << i << ": "
                  << (is_sso ? "SSO" : "Heap")
                  << " (capacity=" << s.capacity() << ")\n";
    }
    return 0;
}
```

### Day 3-4：虚函数表深度分析（14小时）

#### 理论背景

```
单继承的vtable布局:

class Base:
+------------------+
| vptr ------------|---> Base vtable:
+------------------+     +------------------+
| Base::member     |     | type_info* (RTTI)|
+------------------+     +------------------+
                         | Base::~Base()    |
                         +------------------+
                         | Base::foo()      |
                         +------------------+
                         | Base::bar()      |
                         +------------------+

class Derived : public Base:
+------------------+
| vptr ------------|---> Derived vtable:
+------------------+     +------------------+
| Base::member     |     | type_info* (RTTI)|
+------------------+     +------------------+
| Derived::member  |     | Derived::~Derived|
+------------------+     +------------------+
                         | Derived::foo()   | <- 覆盖
                         +------------------+
                         | Base::bar()      | <- 继承
                         +------------------+
                         | Derived::baz()   | <- 新增
                         +------------------+
```

#### 实验程序

```cpp
// vtable_analysis.cpp
#include <iostream>
#include <cstdint>

class Base {
public:
    int base_data = 0x11111111;

    virtual void foo() {
        std::cout << "Base::foo()\n";
    }
    virtual void bar() {
        std::cout << "Base::bar()\n";
    }
    virtual ~Base() {
        std::cout << "Base::~Base()\n";
    }
};

class Derived : public Base {
public:
    int derived_data = 0x22222222;

    void foo() override {
        std::cout << "Derived::foo()\n";
    }
    virtual void baz() {
        std::cout << "Derived::baz()\n";
    }
    ~Derived() override {
        std::cout << "Derived::~Derived()\n";
    }
};

class AnotherDerived : public Base {
public:
    int another_data = 0x33333333;

    void bar() override {
        std::cout << "AnotherDerived::bar()\n";
    }
};

// 手动遍历vtable
void dump_vtable(void* obj, int count) {
    void** vptr = *reinterpret_cast<void***>(obj);
    std::cout << "vtable at " << vptr << ":\n";
    for (int i = -1; i < count; ++i) {
        std::cout << "  [" << i << "]: " << vptr[i] << "\n";
    }
}

int main() {
    Base* b1 = new Base();
    Base* b2 = new Derived();
    Base* b3 = new AnotherDerived();

    std::cout << "=== Object sizes ===\n";
    std::cout << "sizeof(Base) = " << sizeof(Base) << "\n";
    std::cout << "sizeof(Derived) = " << sizeof(Derived) << "\n";

    std::cout << "\n=== VTable dumps ===\n";
    dump_vtable(b1, 4);
    std::cout << "\n";
    dump_vtable(b2, 5);
    std::cout << "\n";
    dump_vtable(b3, 4);

    std::cout << "\n=== Virtual calls ===\n";
    b1->foo();  // 在这里设断点，单步进入观察vtable查找
    b2->foo();
    b3->bar();

    delete b1;
    delete b2;
    delete b3;

    return 0;
}
```

#### GDB调试vtable

```gdb
# g++ -g -O0 vtable_analysis.cpp -o vtable_analysis
# gdb ./vtable_analysis

(gdb) break main
(gdb) run
(gdb) next  # 执行到对象创建完成

# 查看对象内存布局
(gdb) p *b1
(gdb) p *b2
(gdb) p *(Derived*)b2  # 转换为派生类查看

# 查看vptr（对象的第一个8字节）
(gdb) p /a *(void**)b1
(gdb) p /a *(void**)b2

# 查看vtable内容
(gdb) x/6a *(void**)b1    # 6个地址，包括RTTI
(gdb) x/7a *(void**)b2

# 使用GDB内置命令（如果支持）
(gdb) info vtbl b1
(gdb) info vtbl b2

# 观察虚函数调用
(gdb) break Derived::foo
(gdb) continue

# 在汇编级别观察vtable查找
(gdb) disassemble main
# 找到b2->foo()的调用点
(gdb) break *<address>
(gdb) continue
(gdb) display/i $pc
(gdb) si  # 单步执行，观察vtable查找过程
```

#### 多重继承的vtable

```cpp
// multiple_inheritance_vtable.cpp
#include <iostream>

class A {
public:
    int a_data = 0xAAAAAAAA;
    virtual void func_a() { std::cout << "A::func_a\n"; }
    virtual ~A() {}
};

class B {
public:
    int b_data = 0xBBBBBBBB;
    virtual void func_b() { std::cout << "B::func_b\n"; }
    virtual ~B() {}
};

class C : public A, public B {
public:
    int c_data = 0xCCCCCCCC;
    void func_a() override { std::cout << "C::func_a\n"; }
    void func_b() override { std::cout << "C::func_b\n"; }
    virtual void func_c() { std::cout << "C::func_c\n"; }
};

int main() {
    C c;
    A* a_ptr = &c;
    B* b_ptr = &c;

    std::cout << "sizeof(A) = " << sizeof(A) << "\n";
    std::cout << "sizeof(B) = " << sizeof(B) << "\n";
    std::cout << "sizeof(C) = " << sizeof(C) << "\n";

    std::cout << "\nAddresses:\n";
    std::cout << "&c = " << &c << "\n";
    std::cout << "a_ptr = " << a_ptr << "\n";
    std::cout << "b_ptr = " << b_ptr << "\n";  // 注意地址调整！

    return 0;
}
```

### Day 5-6：智能指针控制块分析（10小时）

#### shared_ptr内部结构

```
std::shared_ptr<T> 结构:

shared_ptr对象:
+------------------+
| T* ptr           |  -> 指向实际对象
+------------------+
| control_block*   |  -> 指向控制块
+------------------+

控制块 (_Sp_counted_base):
+------------------+
| vptr (用于析构)   |
+------------------+
| use_count        |  强引用计数
+------------------+
| weak_count       |  弱引用计数 + 1
+------------------+
| deleter (可选)    |
+------------------+
| allocator (可选)  |
+------------------+

make_shared优化:
+------------------+
| control_block    |  <- 单次分配
| + T对象          |  <- 对象紧跟控制块
+------------------+
```

#### 实验程序

```cpp
// shared_ptr_analysis.cpp
#include <memory>
#include <iostream>

struct Widget {
    int data[4] = {1, 2, 3, 4};

    Widget() { std::cout << "Widget constructed\n"; }
    ~Widget() { std::cout << "Widget destroyed\n"; }
};

void print_counts(const std::shared_ptr<Widget>& sp, const char* name) {
    std::cout << name << ": use_count=" << sp.use_count() << "\n";
}

int main() {
    std::cout << "=== Creating shared_ptr with new ===\n";
    std::shared_ptr<Widget> sp1(new Widget());  // 两次分配
    print_counts(sp1, "sp1");

    std::cout << "\n=== Creating shared_ptr with make_shared ===\n";
    auto sp2 = std::make_shared<Widget>();  // 单次分配
    print_counts(sp2, "sp2");

    std::cout << "\n=== Copying shared_ptr ===\n";
    auto sp3 = sp2;
    print_counts(sp2, "sp2");
    print_counts(sp3, "sp3");

    std::cout << "\n=== Creating weak_ptr ===\n";
    std::weak_ptr<Widget> wp = sp2;
    print_counts(sp2, "sp2");
    // weak_count不直接可见，需要通过调试器观察

    std::cout << "\n=== Resetting shared_ptrs ===\n";
    sp2.reset();
    print_counts(sp3, "sp3 after sp2.reset()");

    std::cout << "\n=== Locking weak_ptr ===\n";
    if (auto locked = wp.lock()) {
        print_counts(locked, "locked");
    }

    return 0;  // 设断点观察
}
```

#### GDB分析shared_ptr

```gdb
# gdb ./shared_ptr_analysis

(gdb) break main
(gdb) run
(gdb) # 执行到sp2创建后

# 查看shared_ptr结构
(gdb) p sp2
(gdb) p sp2._M_ptr           # 对象指针
(gdb) p sp2._M_refcount      # 控制块包装器

# 查看控制块
(gdb) p sp2._M_refcount._M_pi                    # 控制块指针
(gdb) p *sp2._M_refcount._M_pi                   # 控制块内容
(gdb) p sp2._M_refcount._M_pi->_M_use_count      # 强引用计数
(gdb) p sp2._M_refcount._M_pi->_M_weak_count     # 弱引用计数

# 观察make_shared的单次分配优化
# 对象和控制块应该是连续的内存
(gdb) p sp2._M_ptr
(gdb) p sp2._M_refcount._M_pi
# 比较两个地址，它们应该很接近

# 对比普通new的情况
(gdb) p sp1._M_ptr
(gdb) p sp1._M_refcount._M_pi
# 这两个地址会相距较远

# 观察weak_ptr后的计数变化
(gdb) # 创建wp后
(gdb) p sp2._M_refcount._M_pi->_M_use_count   # 仍然是1
(gdb) p sp2._M_refcount._M_pi->_M_weak_count  # 变成2（weak_count+1的设计）
```

### Day 7：STL容器内存布局（4小时）

#### std::vector分析

```cpp
// vector_analysis.cpp
#include <vector>
#include <iostream>

int main() {
    std::vector<int> v;

    std::cout << "Empty vector:\n";
    std::cout << "  size: " << v.size() << "\n";
    std::cout << "  capacity: " << v.capacity() << "\n";
    std::cout << "  data: " << v.data() << "\n";

    // 触发多次扩容
    for (int i = 0; i < 20; ++i) {
        size_t old_cap = v.capacity();
        v.push_back(i);
        if (v.capacity() != old_cap) {
            std::cout << "Reallocation at size " << v.size()
                      << ", new capacity: " << v.capacity() << "\n";
        }
    }

    return 0;  // 设断点
}
```

```gdb
(gdb) p v
(gdb) p v._M_impl._M_start           # 数据起始指针
(gdb) p v._M_impl._M_finish          # 当前结束位置
(gdb) p v._M_impl._M_end_of_storage  # 分配的结束位置

# 计算size和capacity
(gdb) p v._M_impl._M_finish - v._M_impl._M_start         # size
(gdb) p v._M_impl._M_end_of_storage - v._M_impl._M_start # capacity

# 查看元素
(gdb) p *v._M_impl._M_start@10       # 前10个元素
```

### 第三周检验清单

- [ ] 能解释std::string的SSO机制
- [ ] 能用调试器找到SSO的临界值
- [ ] 能解释vtable的结构和虚函数调用过程
- [ ] 能分析shared_ptr的引用计数
- [ ] 能解释make_shared的单次分配优化
- [ ] 笔记：`notes/week03_memory_layout.md`

---

## 第四周：高级调试技术（20小时 + 项目时间）

### 学习目标
- 掌握逆向调试技术
- 精通多线程调试
- 熟练分析core dump

### Day 1-2：逆向调试（8小时）

#### GDB Record and Replay

```gdb
# 逆向调试需要先启用recording
(gdb) break main
(gdb) run
(gdb) record                         # 开始记录

# 正常执行
(gdb) continue
# 程序崩溃或到达某个状态后

# 逆向执行
(gdb) reverse-continue               # 反向继续执行
(gdb) reverse-step                   # 反向单步（进入函数）
(gdb) reverse-next                   # 反向单步（不进入函数）
(gdb) reverse-finish                 # 反向执行到调用者

# 查看记录状态
(gdb) info record
(gdb) record stop                    # 停止记录

# 设置记录限制（防止内存爆炸）
(gdb) set record full insn-number-max 200000
```

#### 练习程序：查找bug的引入点

```cpp
// reverse_debug_practice.cpp
#include <vector>
#include <iostream>

int global_counter = 0;

void buggy_increment(int* ptr) {
    if (global_counter > 5) {
        *ptr = -1;  // Bug: 破坏数据
    } else {
        (*ptr)++;
    }
    global_counter++;
}

int main() {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (auto& d : data) {
        buggy_increment(&d);
    }

    // 检查结果
    for (int d : data) {
        if (d < 0) {
            std::cout << "Found corrupted data!\n";
            break;  // 在这里设断点，然后reverse找bug引入点
        }
    }

    return 0;
}
```

### Day 3-4：多线程调试（8小时）

#### 多线程调试命令

```gdb
# ===== 线程信息 =====
(gdb) info threads                   # 列出所有线程
(gdb) thread <id>                    # 切换到指定线程
(gdb) thread apply all bt            # 所有线程的回溯
(gdb) thread apply 1 2 3 bt          # 指定线程的回溯
(gdb) thread apply all print var     # 所有线程打印变量

# ===== 线程控制 =====
(gdb) set scheduler-locking off      # 所有线程都运行（默认）
(gdb) set scheduler-locking on       # 只运行当前线程
(gdb) set scheduler-locking step     # 单步时只运行当前线程
(gdb) set scheduler-locking replay   # 逆向调试时的特殊模式

# ===== 线程断点 =====
(gdb) break func thread 2            # 只在线程2触发的断点
(gdb) break func if $_thread == 2    # 等效写法

# ===== 非停止模式（Non-stop mode）=====
(gdb) set non-stop on                # 断点只停止触发的线程
(gdb) set target-async on            # 启用异步模式
(gdb) interrupt -a                   # 停止所有线程
```

#### LLDB多线程命令

```lldb
(lldb) thread list                   # 列出线程
(lldb) thread select <id>            # 切换线程
(lldb) thread backtrace all          # 所有线程回溯
(lldb) thread continue               # 继续当前线程
```

#### 练习程序：竞态条件调试

```cpp
// race_condition.cpp
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>

int shared_counter = 0;  // 非原子，有竞态
std::atomic<int> safe_counter{0};

void increment_shared(int times) {
    for (int i = 0; i < times; ++i) {
        shared_counter++;  // 竞态条件！
    }
}

void increment_safe(int times) {
    for (int i = 0; i < times; ++i) {
        safe_counter++;
    }
}

int main() {
    const int num_threads = 4;
    const int increments_per_thread = 100000;

    // 测试竞态条件
    {
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(increment_shared, increments_per_thread);
        }
        for (auto& t : threads) t.join();

        std::cout << "Expected: " << num_threads * increments_per_thread << "\n";
        std::cout << "Actual (shared): " << shared_counter << "\n";
    }

    // 测试原子操作
    {
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(increment_safe, increments_per_thread);
        }
        for (auto& t : threads) t.join();

        std::cout << "Actual (atomic): " << safe_counter << "\n";
    }

    return 0;
}
```

### Day 5：Core Dump分析（4小时）

#### 配置Core Dump

```bash
# Linux配置
ulimit -c unlimited                  # 允许生成core文件
echo "/tmp/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern

# macOS配置
ulimit -c unlimited
# core文件通常在 /cores/ 目录

# 验证配置
ulimit -c
```

#### 分析Core Dump

```gdb
# 加载core文件
gdb ./program core

# 基本分析
(gdb) bt                             # 查看崩溃时的调用栈
(gdb) bt full                        # 带局部变量的调用栈
(gdb) info registers                 # 查看寄存器状态
(gdb) info frame                     # 当前帧信息

# 检查崩溃原因
(gdb) p $_siginfo                    # 信号信息
(gdb) p $_siginfo._sifields._sigfault.si_addr  # 故障地址

# 检查内存状态
(gdb) x/10i $pc                      # 崩溃点的指令
(gdb) info proc mappings             # 内存映射
```

#### 练习程序：常见崩溃类型

```cpp
// crash_examples.cpp
#include <cstring>
#include <iostream>

void null_pointer_crash() {
    int* ptr = nullptr;
    *ptr = 42;  // SIGSEGV
}

void stack_overflow() {
    int arr[1000000];  // 栈溢出
    arr[0] = 1;
}

void buffer_overflow() {
    char buf[10];
    strcpy(buf, "This is a very long string that overflows the buffer");
}

void use_after_free() {
    int* ptr = new int(42);
    delete ptr;
    *ptr = 100;  // 使用已释放的内存
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <crash_type>\n";
        std::cout << "  1 - null pointer\n";
        std::cout << "  2 - stack overflow\n";
        std::cout << "  3 - buffer overflow\n";
        std::cout << "  4 - use after free\n";
        return 1;
    }

    switch (argv[1][0]) {
        case '1': null_pointer_crash(); break;
        case '2': stack_overflow(); break;
        case '3': buffer_overflow(); break;
        case '4': use_after_free(); break;
    }

    return 0;
}
```

### Day 6-7：项目完善与整合

完成本月实践项目的所有部分：

1. **memory_inspector.hpp** - 完善并测试
2. **cpp_inspector.py** - GDB Python扩展
3. **leak_detector.hpp** - 简易内存泄漏检测器
4. **调试案例分析报告** - 至少3个

### 第四周检验清单

- [ ] 能使用record进行逆向调试
- [ ] 能调试多线程程序，分析竞态条件
- [ ] 能分析core dump定位崩溃原因
- [ ] 完成所有项目代码
- [ ] 笔记：`notes/week04_advanced_debugging.md`

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
#include <type_traits>

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

class DumpSharedPtrCmd(gdb.Command):
    """Dump std::shared_ptr internal state"""

    def __init__(self):
        super().__init__("dump_shared_ptr", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        try:
            val = gdb.parse_and_eval(arg)
            ptr = val['_M_ptr']
            refcount = val['_M_refcount']['_M_pi']

            print(f"shared_ptr at {val.address}")
            print(f"  Managed pointer: {ptr}")
            print(f"  Control block: {refcount}")

            if refcount:
                use_count = refcount['_M_use_count']
                weak_count = refcount['_M_weak_count']
                print(f"  Use count: {use_count}")
                print(f"  Weak count: {weak_count}")
            else:
                print("  (empty)")

        except Exception as e:
            print(f"Error: {e}")

DumpSharedPtrCmd()
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

## 月度总结与检验

### 知识检验问题

1. **调试器如何实现断点？**
   - 答：通过ptrace的PTRACE_POKETEXT将目标地址的第一个字节替换为INT 3 (0xCC)。当CPU执行到该指令时触发SIGTRAP信号，调试器捕获信号后暂停被调试进程，恢复原始字节，准备用户交互。

2. **什么是DWARF调试信息？**
   - 答：DWARF是一种调试信息格式，包含源码行号映射、类型信息、变量位置等数据。编译时使用-g选项生成。调试器依赖DWARF信息将机器地址映射回源码位置。

3. **std::string的SSO是什么？**
   - 答：Small String Optimization，短字符串直接存储在string对象内部（栈上），避免堆分配。libstdc++阈值为15字节，libc++为22字节。

4. **shared_ptr控制块包含什么？**
   - 答：use_count（强引用计数）、weak_count（弱引用计数+1）、虚函数表指针（用于正确析构）、可能还有自定义deleter和allocator。

### 实践检验
- [ ] 能够使用GDB/LLDB观察任意C++对象的内存布局
- [ ] 能够分析虚函数调用的vtable查找过程
- [ ] 能够分析core dump定位崩溃原因
- [ ] 完成内存分析工具集的开发

### 最终输出物清单

1. [ ] `memory_inspector.hpp` - 内存分析工具
2. [ ] `cpp_inspector.py` - GDB Python扩展脚本
3. [ ] `leak_detector.hpp` - 简易内存泄漏检测器
4. [ ] `notes/month02_debugging.md` - 调试技术总笔记
5. [ ] 3份调试案例分析报告
6. [ ] GDB/LLDB命令速查表

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
