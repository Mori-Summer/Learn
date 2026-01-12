# Month 35: 协议设计与序列化

## 本月主题概述

高效的序列化是高性能网络应用的基础。本月学习Protobuf和FlatBuffers两种主流序列化框架，理解协议设计的最佳实践，并实现自定义协议层。

---

## 理论学习内容

### 第一周：自定义协议设计

**学习目标**：理解网络协议设计原则

**阅读材料**：
- [ ] 《计算机网络》协议设计章节
- [ ] 各种开源项目的协议设计文档

**核心概念**：

```
协议设计要素：

1. 消息边界
   - 定长消息
   - 分隔符（如\r\n）
   - 长度前缀（TLV）

2. 版本兼容
   - 前向兼容（旧代码读新数据）
   - 后向兼容（新代码读旧数据）

3. 消息格式
   ┌────────────────────────────────────────┐
   │ Magic(4) │ Version(2) │ Type(2)       │
   ├────────────────────────────────────────┤
   │ Length(4) │ Reserved(4)               │
   ├────────────────────────────────────────┤
   │ Payload (Length bytes)                │
   └────────────────────────────────────────┘

4. 字节序
   - 大端（网络字节序）
   - 小端（x86/ARM）
```

```cpp
// protocol.hpp
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <arpa/inet.h>

#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;      // 魔数
    uint16_t version;    // 协议版本
    uint16_t type;       // 消息类型
    uint32_t length;     // 负载长度
    uint32_t sequence;   // 序列号
    uint32_t checksum;   // 校验和

    static constexpr uint32_t MAGIC = 0x12345678;
    static constexpr uint16_t VERSION = 1;

    void to_network() {
        magic = htonl(magic);
        version = htons(version);
        type = htons(type);
        length = htonl(length);
        sequence = htonl(sequence);
        checksum = htonl(checksum);
    }

    void to_host() {
        magic = ntohl(magic);
        version = ntohs(version);
        type = ntohs(type);
        length = ntohl(length);
        sequence = ntohl(sequence);
        checksum = ntohl(checksum);
    }

    bool is_valid() const {
        return magic == MAGIC;
    }
};
#pragma pack(pop)

class Message {
public:
    enum Type : uint16_t {
        HEARTBEAT = 0,
        REQUEST = 1,
        RESPONSE = 2,
        PUSH = 3
    };

    MessageHeader header;
    std::vector<uint8_t> payload;

    static uint32_t calculate_checksum(const void* data, size_t len) {
        uint32_t sum = 0;
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i) {
            sum += p[i];
        }
        return sum;
    }

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer(sizeof(MessageHeader) + payload.size());

        MessageHeader h = header;
        h.length = payload.size();
        h.checksum = calculate_checksum(payload.data(), payload.size());
        h.to_network();

        std::memcpy(buffer.data(), &h, sizeof(h));
        std::memcpy(buffer.data() + sizeof(h), payload.data(), payload.size());

        return buffer;
    }

    bool deserialize(const uint8_t* data, size_t len) {
        if (len < sizeof(MessageHeader)) return false;

        std::memcpy(&header, data, sizeof(header));
        header.to_host();

        if (!header.is_valid()) return false;
        if (len < sizeof(MessageHeader) + header.length) return false;

        payload.assign(data + sizeof(MessageHeader),
                      data + sizeof(MessageHeader) + header.length);

        uint32_t checksum = calculate_checksum(payload.data(), payload.size());
        return checksum == header.checksum;
    }
};
```

### 第二周：Protocol Buffers

```protobuf
// message.proto
syntax = "proto3";

package myapp;

message User {
    int32 id = 1;
    string name = 2;
    string email = 3;
    repeated string tags = 4;

    enum Status {
        UNKNOWN = 0;
        ACTIVE = 1;
        INACTIVE = 2;
    }
    Status status = 5;
}

message Request {
    string method = 1;
    bytes payload = 2;
    map<string, string> headers = 3;
}

message Response {
    int32 code = 1;
    string message = 2;
    bytes data = 3;
}
```

```cpp
// protobuf_usage.cpp
#include "message.pb.h"

void protobuf_example() {
    // 创建消息
    myapp::User user;
    user.set_id(1);
    user.set_name("Alice");
    user.set_email("alice@example.com");
    user.add_tags("developer");
    user.add_tags("cpp");
    user.set_status(myapp::User::ACTIVE);

    // 序列化
    std::string serialized;
    user.SerializeToString(&serialized);

    // 反序列化
    myapp::User parsed;
    parsed.ParseFromString(serialized);

    // 访问字段
    std::cout << "ID: " << parsed.id() << std::endl;
    std::cout << "Name: " << parsed.name() << std::endl;

    // 序列化到文件
    std::ofstream file("user.bin", std::ios::binary);
    user.SerializeToOstream(&file);

    // 从文件反序列化
    std::ifstream in_file("user.bin", std::ios::binary);
    myapp::User from_file;
    from_file.ParseFromIstream(&in_file);
}

/*
Protobuf编码原理：
- Varint: 可变长度整数编码
- 字段标识: (field_number << 3) | wire_type
- Wire types:
  0: Varint (int32, int64, bool, enum)
  1: 64-bit (fixed64, double)
  2: Length-delimited (string, bytes, embedded messages)
  5: 32-bit (fixed32, float)

优点：
- 高效紧凑
- 强类型
- 版本兼容

缺点：
- 需要编译proto文件
- 不是人类可读
*/
```

### 第三周：FlatBuffers

```
// message.fbs
namespace MyApp;

enum Status : byte { Unknown = 0, Active = 1, Inactive = 2 }

table User {
    id: int;
    name: string;
    email: string;
    tags: [string];
    status: Status = Unknown;
}

table Request {
    method: string;
    payload: [ubyte];
}

table Response {
    code: int;
    message: string;
    data: [ubyte];
}

root_type Response;
```

```cpp
// flatbuffers_usage.cpp
#include "message_generated.h"

void flatbuffers_example() {
    flatbuffers::FlatBufferBuilder builder(1024);

    // 创建嵌套对象
    auto name = builder.CreateString("Alice");
    auto email = builder.CreateString("alice@example.com");
    auto tags = builder.CreateVector(
        std::vector<flatbuffers::Offset<flatbuffers::String>>{
            builder.CreateString("developer"),
            builder.CreateString("cpp")
        }
    );

    // 创建User
    auto user = MyApp::CreateUser(
        builder,
        1,              // id
        name,           // name
        email,          // email
        tags,           // tags
        MyApp::Status_Active
    );

    builder.Finish(user);

    // 获取buffer
    uint8_t* buf = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    // 零拷贝访问（无需反序列化）
    auto parsed = MyApp::GetUser(buf);
    std::cout << "ID: " << parsed->id() << std::endl;
    std::cout << "Name: " << parsed->name()->str() << std::endl;

    // 访问数组
    for (auto tag : *parsed->tags()) {
        std::cout << "Tag: " << tag->str() << std::endl;
    }
}

/*
FlatBuffers vs Protobuf：

FlatBuffers优点：
- 零拷贝反序列化（直接访问buffer）
- 更快的访问速度
- 无需解析步骤

FlatBuffers缺点：
- 序列化后体积稍大
- 随机访问效率低
- 修改已序列化数据困难

适用场景：
- Protobuf: 通用序列化、RPC
- FlatBuffers: 游戏、实时应用、内存映射
*/
```

### 第四周：序列化层封装

```cpp
// serializer.hpp
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <typeinfo>

// 序列化接口
class ISerializer {
public:
    virtual ~ISerializer() = default;

    virtual std::vector<uint8_t> serialize(const void* obj, const std::type_info& type) = 0;
    virtual void* deserialize(const uint8_t* data, size_t len, const std::type_info& type) = 0;
};

// JSON序列化器
class JsonSerializer : public ISerializer {
public:
    std::vector<uint8_t> serialize(const void* obj, const std::type_info& type) override {
        // 使用nlohmann/json或rapidjson
        // ...
    }

    void* deserialize(const uint8_t* data, size_t len, const std::type_info& type) override {
        // ...
    }
};

// Protobuf序列化器
class ProtobufSerializer : public ISerializer {
public:
    std::vector<uint8_t> serialize(const void* obj, const std::type_info& type) override {
        // 使用protobuf的反射API
        // ...
    }

    void* deserialize(const uint8_t* data, size_t len, const std::type_info& type) override {
        // ...
    }
};

// 序列化工厂
class SerializerFactory {
public:
    enum class Type {
        JSON,
        PROTOBUF,
        FLATBUFFERS,
        MSGPACK
    };

    static std::unique_ptr<ISerializer> create(Type type) {
        switch (type) {
            case Type::JSON:
                return std::make_unique<JsonSerializer>();
            case Type::PROTOBUF:
                return std::make_unique<ProtobufSerializer>();
            // ...
        }
        return nullptr;
    }
};

// 高层封装
template<typename T>
class Codec {
public:
    using Serializer = std::function<std::vector<uint8_t>(const T&)>;
    using Deserializer = std::function<T(const uint8_t*, size_t)>;

    Codec(Serializer ser, Deserializer deser)
        : serialize_(std::move(ser)), deserialize_(std::move(deser)) {}

    std::vector<uint8_t> encode(const T& obj) const {
        return serialize_(obj);
    }

    T decode(const uint8_t* data, size_t len) const {
        return deserialize_(data, len);
    }

private:
    Serializer serialize_;
    Deserializer deserialize_;
};
```

---

## 实践项目

### 项目：多格式消息框架

```cpp
// message_framework.hpp
#pragma once
#include <variant>
#include <any>
#include <functional>
#include <unordered_map>

// 通用消息接口
class IMessage {
public:
    virtual ~IMessage() = default;
    virtual uint32_t type_id() const = 0;
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual bool deserialize(const uint8_t* data, size_t len) = 0;
};

// 消息注册表
class MessageRegistry {
public:
    using Factory = std::function<std::unique_ptr<IMessage>()>;

    static MessageRegistry& instance() {
        static MessageRegistry inst;
        return inst;
    }

    void register_message(uint32_t type_id, Factory factory) {
        factories_[type_id] = std::move(factory);
    }

    std::unique_ptr<IMessage> create(uint32_t type_id) {
        auto it = factories_.find(type_id);
        if (it != factories_.end()) {
            return it->second();
        }
        return nullptr;
    }

private:
    std::unordered_map<uint32_t, Factory> factories_;
};

// 注册宏
#define REGISTER_MESSAGE(TypeId, MessageClass) \
    static bool _registered_##MessageClass = []() { \
        MessageRegistry::instance().register_message(TypeId, []() { \
            return std::make_unique<MessageClass>(); \
        }); \
        return true; \
    }()

// 消息编解码器
class MessageCodec {
public:
    // 编码消息（带头部）
    static std::vector<uint8_t> encode(const IMessage& msg) {
        auto payload = msg.serialize();

        std::vector<uint8_t> buffer;
        buffer.resize(8 + payload.size());

        uint32_t type_id = msg.type_id();
        uint32_t length = payload.size();

        std::memcpy(buffer.data(), &type_id, 4);
        std::memcpy(buffer.data() + 4, &length, 4);
        std::memcpy(buffer.data() + 8, payload.data(), payload.size());

        return buffer;
    }

    // 解码消息
    static std::unique_ptr<IMessage> decode(const uint8_t* data, size_t len) {
        if (len < 8) return nullptr;

        uint32_t type_id, length;
        std::memcpy(&type_id, data, 4);
        std::memcpy(&length, data + 4, 4);

        if (len < 8 + length) return nullptr;

        auto msg = MessageRegistry::instance().create(type_id);
        if (msg && msg->deserialize(data + 8, length)) {
            return msg;
        }
        return nullptr;
    }
};
```

---

## 检验标准

- [ ] 理解协议设计的核心要素
- [ ] 掌握Protobuf的使用
- [ ] 掌握FlatBuffers的使用
- [ ] 理解两者的优缺点和适用场景
- [ ] 实现通用序列化层封装

### 输出物
1. `protocol.hpp` - 自定义协议实现
2. `message.proto` - Protobuf消息定义
3. `message.fbs` - FlatBuffers消息定义
4. `message_framework.hpp` - 消息框架
5. `notes/month35_serialization.md`

---

## 时间分配

| 内容 | 时间 |
|-----|------|
| 协议设计原理 | 25小时 |
| Protobuf学习 | 35小时 |
| FlatBuffers学习 | 35小时 |
| 框架封装 | 35小时 |
| 性能对比测试 | 10小时 |

---

## 下月预告

Month 36将进行**第三年总结与综合项目**，整合所学知识构建完整网络库。
