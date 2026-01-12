# Month 29: 零拷贝技术——消除数据拷贝开销

## 本月主题概述

零拷贝是高性能服务器的关键技术。本月学习Linux下各种零拷贝技术，包括sendfile、splice、mmap，以及用户态协议栈的概念。

---

## 理论学习内容

### 第一周：传统I/O与零拷贝

**学习目标**：理解数据拷贝的开销和零拷贝的价值

**阅读材料**：
- [ ] 《Linux高性能服务器编程》零拷贝章节
- [ ] IBM developerWorks零拷贝文章

**核心概念**：

```
传统文件发送（4次拷贝）：
┌─────────┐  read()   ┌─────────┐
│ 磁盘    │ ───────>  │ 内核缓冲│ (DMA拷贝)
└─────────┘           └────┬────┘
                           │ CPU拷贝
                      ┌────▼────┐
                      │用户缓冲 │
                      └────┬────┘
                           │ CPU拷贝
                      ┌────▼────┐
                      │socket缓冲│
                      └────┬────┘
                           │ DMA拷贝
                      ┌────▼────┐
                      │  网卡   │
                      └─────────┘

零拷贝目标：
1. 减少CPU拷贝次数
2. 减少用户态/内核态切换
3. 减少内存带宽消耗
```

### 第二周：sendfile系统调用

```cpp
#include <sys/sendfile.h>

// sendfile - 在内核中直接传输文件到socket
ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count);

// 使用示例
void send_file(int sock_fd, const char* filename) {
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) return;

    struct stat st;
    fstat(file_fd, &st);

    off_t offset = 0;
    size_t remaining = st.st_size;

    while (remaining > 0) {
        ssize_t sent = sendfile(sock_fd, file_fd, &offset, remaining);
        if (sent > 0) {
            remaining -= sent;
        } else if (sent == 0) {
            break;
        } else {
            if (errno == EAGAIN || errno == EINTR) continue;
            break;
        }
    }

    close(file_fd);
}

/*
sendfile流程（2次拷贝 + 1次gather）：
┌─────────┐  DMA   ┌─────────┐
│ 磁盘    │ ───>   │内核缓冲 │
└─────────┘        └────┬────┘
                        │ 只传递描述符
                   ┌────▼────┐
                   │socket缓冲│
                   └────┬────┘
                        │ DMA gather
                   ┌────▼────┐
                   │  网卡   │
                   └─────────┘

注意：需要网卡支持scatter-gather DMA
*/
```

### 第三周：splice与vmsplice

```cpp
#include <fcntl.h>

// splice - 在两个fd之间移动数据（需要一个是管道）
ssize_t splice(int fd_in, loff_t* off_in,
               int fd_out, loff_t* off_out,
               size_t len, unsigned int flags);

// 使用splice实现零拷贝echo
void splice_echo(int sock_fd) {
    int pipefd[2];
    pipe(pipefd);

    while (true) {
        // socket -> pipe
        ssize_t n = splice(sock_fd, nullptr, pipefd[1], nullptr,
                          65536, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
        if (n <= 0) break;

        // pipe -> socket
        while (n > 0) {
            ssize_t sent = splice(pipefd[0], nullptr, sock_fd, nullptr,
                                  n, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
            if (sent > 0) n -= sent;
            else if (errno == EAGAIN) continue;
            else break;
        }
    }

    close(pipefd[0]);
    close(pipefd[1]);
}

// vmsplice - 将用户空间内存映射到管道
ssize_t vmsplice(int fd, const struct iovec* iov,
                 unsigned long nr_segs, unsigned int flags);

// 使用vmsplice发送数据
void vmsplice_send(int sock_fd, const void* data, size_t len) {
    int pipefd[2];
    pipe(pipefd);

    struct iovec iov = {
        .iov_base = (void*)data,
        .iov_len = len
    };

    // 用户空间 -> 管道（零拷贝）
    vmsplice(pipefd[1], &iov, 1, SPLICE_F_GIFT);

    // 管道 -> socket（零拷贝）
    splice(pipefd[0], nullptr, sock_fd, nullptr, len, SPLICE_F_MOVE);

    close(pipefd[0]);
    close(pipefd[1]);
}
```

### 第四周：mmap与用户态协议栈

```cpp
#include <sys/mman.h>

// mmap文件发送
void mmap_send(int sock_fd, const char* filename) {
    int fd = open(filename, O_RDONLY);
    struct stat st;
    fstat(fd, &st);

    void* addr = mmap(nullptr, st.st_size, PROT_READ,
                      MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        return;
    }

    // madvise提示内核访问模式
    madvise(addr, st.st_size, MADV_SEQUENTIAL);

    // 发送数据
    send(sock_fd, addr, st.st_size, 0);

    munmap(addr, st.st_size);
    close(fd);
}

// 共享内存与零拷贝
class SharedBuffer {
public:
    SharedBuffer(size_t size) : size_(size) {
        // 创建匿名共享映射
        addr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    }

    ~SharedBuffer() {
        if (addr_ != MAP_FAILED) {
            munmap(addr_, size_);
        }
    }

    void* data() { return addr_; }
    size_t size() const { return size_; }

private:
    void* addr_ = MAP_FAILED;
    size_t size_;
};

/*
用户态协议栈概念：

传统路径：
应用程序 <-> 系统调用 <-> 内核协议栈 <-> 网卡驱动 <-> 网卡

用户态协议栈（如DPDK）：
应用程序 <-> 用户态协议栈 <-> 用户态网卡驱动 <-> 网卡

优势：
1. 消除系统调用开销
2. 完全零拷贝
3. 更高的包处理速率

适用场景：
- 超高性能要求（如10Gbps+）
- 专用网络设备
- NFV（网络功能虚拟化）
*/
```

---

## 源码阅读任务

1. **Nginx的sendfile使用**
   - `ngx_linux_sendfile_chain.c`
   - 理解如何结合sendfile和writev

2. **Kafka的零拷贝**
   - Java的FileChannel.transferTo
   - 理解消息队列如何利用零拷贝

---

## 实践项目

### 项目：高性能静态文件服务器

```cpp
// file_server.hpp
#pragma once
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <unordered_map>

class FileCache {
public:
    struct CachedFile {
        void* data;
        size_t size;
        time_t mtime;

        ~CachedFile() {
            if (data && data != MAP_FAILED) {
                munmap(data, size);
            }
        }
    };

    CachedFile* get(const std::string& path) {
        auto it = cache_.find(path);
        if (it != cache_.end()) {
            // 检查文件是否更新
            struct stat st;
            if (stat(path.c_str(), &st) == 0 &&
                st.st_mtime == it->second.mtime) {
                return &it->second;
            }
            cache_.erase(it);
        }

        // 加载文件到缓存
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return nullptr;

        struct stat st;
        if (fstat(fd, &st) < 0) {
            close(fd);
            return nullptr;
        }

        void* addr = mmap(nullptr, st.st_size, PROT_READ,
                          MAP_PRIVATE, fd, 0);
        close(fd);

        if (addr == MAP_FAILED) return nullptr;

        madvise(addr, st.st_size, MADV_RANDOM);

        auto [iter, _] = cache_.emplace(path, CachedFile{addr, (size_t)st.st_size, st.st_mtime});
        return &iter->second;
    }

private:
    std::unordered_map<std::string, CachedFile> cache_;
};

class FileServer {
public:
    enum class Method { NORMAL, SENDFILE, MMAP };

    FileServer(const std::string& root, Method method = Method::SENDFILE)
        : root_(root), method_(method) {}

    bool send_file(int sock_fd, const std::string& path) {
        std::string full_path = root_ + path;

        switch (method_) {
            case Method::NORMAL:
                return send_normal(sock_fd, full_path);
            case Method::SENDFILE:
                return send_with_sendfile(sock_fd, full_path);
            case Method::MMAP:
                return send_with_mmap(sock_fd, full_path);
        }
        return false;
    }

private:
    bool send_normal(int sock_fd, const std::string& path) {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;

        char buf[8192];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            ssize_t sent = 0;
            while (sent < n) {
                ssize_t r = write(sock_fd, buf + sent, n - sent);
                if (r <= 0) {
                    if (errno == EINTR) continue;
                    close(fd);
                    return false;
                }
                sent += r;
            }
        }

        close(fd);
        return true;
    }

    bool send_with_sendfile(int sock_fd, const std::string& path) {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;

        struct stat st;
        if (fstat(fd, &st) < 0) {
            close(fd);
            return false;
        }

        off_t offset = 0;
        size_t remaining = st.st_size;

        while (remaining > 0) {
            ssize_t sent = sendfile(sock_fd, fd, &offset, remaining);
            if (sent > 0) {
                remaining -= sent;
            } else if (sent == 0) {
                break;
            } else {
                if (errno == EAGAIN || errno == EINTR) continue;
                close(fd);
                return false;
            }
        }

        close(fd);
        return true;
    }

    bool send_with_mmap(int sock_fd, const std::string& path) {
        auto* cached = cache_.get(path);
        if (!cached) return false;

        size_t sent = 0;
        while (sent < cached->size) {
            ssize_t r = write(sock_fd, (char*)cached->data + sent,
                             cached->size - sent);
            if (r > 0) {
                sent += r;
            } else if (r == 0) {
                break;
            } else {
                if (errno == EAGAIN || errno == EINTR) continue;
                return false;
            }
        }

        return true;
    }

private:
    std::string root_;
    Method method_;
    FileCache cache_;
};

// 性能测试框架
class Benchmark {
public:
    static void compare_methods(const std::string& file_path, int iterations) {
        printf("Benchmarking file: %s\n", file_path.c_str());
        printf("Iterations: %d\n\n", iterations);

        int sv[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

        // 创建接收线程
        std::thread receiver([fd = sv[1], iterations]() {
            char buf[65536];
            for (int i = 0; i < iterations * 3; ++i) {
                while (true) {
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n <= 0) break;
                }
            }
        });

        FileServer normal("", FileServer::Method::NORMAL);
        FileServer sendfile("", FileServer::Method::SENDFILE);
        FileServer mmap("", FileServer::Method::MMAP);

        // 测试各方法
        auto test = [&](FileServer& server, const char* name) {
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                server.send_file(sv[0], file_path);
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            printf("%s: %ld ms\n", name, ms);
        };

        test(normal, "Normal read/write");
        test(sendfile, "sendfile      ");
        test(mmap, "mmap + write   ");

        close(sv[0]);
        receiver.join();
        close(sv[1]);
    }
};
```

---

## 检验标准

- [ ] 理解传统I/O的数据拷贝开销
- [ ] 掌握sendfile系统调用使用
- [ ] 理解splice和vmsplice的原理
- [ ] 能使用mmap优化文件读取
- [ ] 了解用户态协议栈的概念

### 输出物
1. `file_server.hpp` - 多种零拷贝方式的文件服务器
2. `benchmark.cpp` - 性能对比测试
3. `splice_proxy.cpp` - 使用splice的代理服务器
4. `notes/month29_zero_copy.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| 零拷贝原理 | 20小时 |
| sendfile实践 | 30小时 |
| splice/vmsplice | 30小时 |
| mmap优化 | 30小时 |
| 性能测试对比 | 30小时 |

---

## 下月预告

Month 30将学习**Reactor模式**，构建事件驱动的服务器架构。
