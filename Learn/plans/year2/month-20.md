# Month 20: Actor模型与消息传递——另一种并发范式

## 本月主题概述

Actor模型是一种与共享内存截然不同的并发编程范式。每个Actor有自己的状态，只通过消息与其他Actor通信，从根本上避免了数据竞争。

---

## 理论学习内容

### 核心概念

- **Actor**: 独立的计算单元，有私有状态
- **Mailbox**: 每个Actor有一个消息队列
- **消息传递**: Actor之间唯一的通信方式
- **无共享**: 没有共享状态，没有锁

### Actor系统设计

```cpp
// 概念性API
class Actor {
    Mailbox mailbox;
    virtual void receive(Message msg) = 0;
public:
    void send(ActorRef target, Message msg);
};
```

---

## 实践项目

### 项目：简单的Actor框架

```cpp
// actor.hpp
#pragma once
#include <memory>
#include <functional>
#include <any>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

class Actor;
using ActorRef = std::shared_ptr<Actor>;
using Message = std::any;

class Actor : public std::enable_shared_from_this<Actor> {
    std::queue<std::pair<ActorRef, Message>> mailbox_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
    std::thread thread_;

protected:
    virtual void on_receive(ActorRef sender, const Message& msg) = 0;

    void process_messages() {
        while (running_) {
            std::pair<ActorRef, Message> item;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] {
                    return !mailbox_.empty() || !running_;
                });
                if (!running_ && mailbox_.empty()) break;
                item = std::move(mailbox_.front());
                mailbox_.pop();
            }
            on_receive(item.first, item.second);
        }
    }

public:
    Actor() {
        thread_ = std::thread(&Actor::process_messages, this);
    }

    virtual ~Actor() {
        running_ = false;
        cv_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

    void send(ActorRef sender, Message msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        mailbox_.emplace(sender, std::move(msg));
        cv_.notify_one();
    }

    template <typename T, typename... Args>
    static ActorRef create(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

// 使用示例
class PingActor : public Actor {
    int count_ = 0;
protected:
    void on_receive(ActorRef sender, const Message& msg) override {
        if (auto* ping = std::any_cast<std::string>(&msg)) {
            if (*ping == "ping" && count_++ < 5) {
                std::cout << "Received ping, sending pong\n";
                sender->send(shared_from_this(), std::string("pong"));
            }
        }
    }
};
```

---

## 检验标准

- [ ] 理解Actor模型的核心思想
- [ ] 实现简单的Actor框架
- [ ] 理解与共享内存模型的区别

### 输出物
1. `actor.hpp`
2. `test_actor.cpp`
3. `notes/month20_actor.md`

---

## 下月预告

Month 21将学习**协程基础（C++20）**，探索无栈协程的原理和使用。
