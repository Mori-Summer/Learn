# Month 53: ECS架构 (Entity-Component-System)

## 本月主题概述

Entity-Component-System（ECS）是一种将数据与逻辑分离的架构模式，已成为现代游戏引擎和高性能应用的主流选择。ECS通过组合而非继承来构建实体，将行为封装在系统中，实现了极高的灵活性和卓越的缓存性能。本月将深入理解ECS的设计哲学，并从零构建一个高性能ECS框架。

### 学习目标
- 理解ECS架构的核心概念与设计原则
- 掌握两种主要的组件存储策略
- 实现高效的实体查询系统
- 理解系统调度与依赖管理
- 构建一个功能完整的ECS框架

---

## 理论学习内容

### 第一周：ECS核心概念

#### 阅读材料
1. 《Game Programming Patterns》- Component章节
2. Unity DOTS官方文档
3. EnTT库设计文档
4. Flecs库wiki

#### 核心概念

**ECS vs 传统OOP**
```
┌─────────────────────────────────────────────────────────┐
│                    传统OOP继承                          │
└─────────────────────────────────────────────────────────┘

           GameObject
               │
    ┌──────────┼──────────┐
    │          │          │
Character   Vehicle    Projectile
    │          │          │
  ┌─┴─┐      ┌─┴─┐        │
Player Enemy Car Plane  Bullet

问题：
- 深层继承导致耦合
- 菱形继承问题
- 难以运行时改变行为
- 数据分散，缓存不友好

┌─────────────────────────────────────────────────────────┐
│                    ECS组合模式                          │
└─────────────────────────────────────────────────────────┘

Entity: 只是一个ID (uint32/uint64)

Components (纯数据):
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ Position │ │ Velocity │ │ Health   │ │ Sprite   │
│ x, y, z  │ │ vx,vy,vz │ │ current  │ │ textureId│
└──────────┘ └──────────┘ │ max      │ │ width,h  │
                          └──────────┘ └──────────┘

Systems (纯逻辑):
┌────────────────┐ ┌────────────────┐ ┌────────────────┐
│ MovementSystem │ │ RenderSystem   │ │ DamageSystem   │
│ Query:         │ │ Query:         │ │ Query:         │
│ Position,      │ │ Position,      │ │ Health         │
│ Velocity       │ │ Sprite         │ │                │
└────────────────┘ └────────────────┘ └────────────────┘

Entity组合示例：
Player = Entity + Position + Velocity + Health + Sprite + Input
Enemy  = Entity + Position + Velocity + Health + Sprite + AI
Bullet = Entity + Position + Velocity + Damage + Sprite
```

**ECS术语定义**
```cpp
// Entity: 唯一标识符
using Entity = uint64_t;

// 分解Entity: 索引 + 版本号（用于检测无效引用）
struct EntityId {
    uint32_t index;    // 索引
    uint32_t version;  // 版本号，每次回收递增
};

// Component: 纯数据结构，无行为
struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Health {
    int current;
    int max;
};

// System: 纯逻辑，处理具有特定组件组合的实体
class MovementSystem {
public:
    void update(float dt) {
        // 查询所有有Position和Velocity的实体
        for (auto [entity, pos, vel] : query<Position, Velocity>()) {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            pos.z += vel.z * dt;
        }
    }
};

// World: 管理所有Entity、Component和System
class World {
    // 实体管理
    // 组件存储
    // 系统调度
};
```

### 第二周：组件存储策略

#### 阅读材料
1. 《Building an ECS》系列文章
2. Archetype存储解析
3. Sparse Set数据结构详解
4. Unity Entities源码分析

#### 核心概念

**存储策略对比**
```
┌─────────────────────────────────────────────────────────┐
│                  Sparse Set 策略                        │
│                  (EnTT, Flecs)                         │
└─────────────────────────────────────────────────────────┘

每种组件类型一个池：
Position Pool: [P0, P1, P2, P3, ...]    // 紧凑数组
Velocity Pool: [V0, V1, V2, ...]        // 紧凑数组
Health Pool:   [H0, H1, ...]            // 紧凑数组

Sparse Array (Entity -> 组件索引):
Entity 0 -> Position[0], Velocity[0]
Entity 1 -> Position[1], Health[0]
Entity 2 -> Position[2], Velocity[1], Health[1]

优点：
✓ 添加/删除组件O(1)
✓ 单组件遍历极快
✓ 灵活性高

缺点：
✗ 多组件查询需要交集计算
✗ 组件不连续存储


┌─────────────────────────────────────────────────────────┐
│                  Archetype 策略                         │
│                  (Unity DOTS, Bevy)                    │
└─────────────────────────────────────────────────────────┘

按组件组合分组：
Archetype [Position, Velocity]:
  Position: [P0, P1, P2, ...]
  Velocity: [V0, V1, V2, ...]
  Entities: [E0, E1, E2, ...]

Archetype [Position, Velocity, Health]:
  Position: [P3, P4, ...]
  Velocity: [V3, V4, ...]
  Health:   [H0, H1, ...]
  Entities: [E3, E4, ...]

Archetype [Position, Sprite]:
  Position: [P5, P6, ...]
  Sprite:   [S0, S1, ...]
  Entities: [E5, E6, ...]

优点：
✓ 同Archetype的组件紧凑存储
✓ 多组件查询只需遍历匹配的Archetype
✓ 缓存效率极高

缺点：
✗ 添加/删除组件需要移动实体
✗ Archetype数量可能爆炸
```

**Sparse Set实现**
```cpp
template<typename T>
class SparseSet {
private:
    std::vector<T> dense_;           // 紧凑的组件数组
    std::vector<uint32_t> sparse_;   // Entity -> dense索引
    std::vector<uint32_t> entities_; // dense索引 -> Entity

    static constexpr uint32_t INVALID = UINT32_MAX;

public:
    // 添加组件
    void insert(uint32_t entity, const T& component) {
        if (entity >= sparse_.size()) {
            sparse_.resize(entity + 1, INVALID);
        }

        if (sparse_[entity] != INVALID) {
            // 已存在，更新
            dense_[sparse_[entity]] = component;
        } else {
            // 新增
            sparse_[entity] = static_cast<uint32_t>(dense_.size());
            dense_.push_back(component);
            entities_.push_back(entity);
        }
    }

    // 移除组件
    void erase(uint32_t entity) {
        if (entity >= sparse_.size() || sparse_[entity] == INVALID) {
            return;
        }

        // 与最后一个交换
        uint32_t denseIdx = sparse_[entity];
        uint32_t lastEntity = entities_.back();

        dense_[denseIdx] = std::move(dense_.back());
        entities_[denseIdx] = lastEntity;
        sparse_[lastEntity] = denseIdx;

        dense_.pop_back();
        entities_.pop_back();
        sparse_[entity] = INVALID;
    }

    // 查询
    T* get(uint32_t entity) {
        if (entity >= sparse_.size() || sparse_[entity] == INVALID) {
            return nullptr;
        }
        return &dense_[sparse_[entity]];
    }

    bool contains(uint32_t entity) const {
        return entity < sparse_.size() && sparse_[entity] != INVALID;
    }

    // 遍历
    auto begin() { return dense_.begin(); }
    auto end() { return dense_.end(); }

    const std::vector<uint32_t>& entities() const { return entities_; }
    size_t size() const { return dense_.size(); }
};
```

### 第三周：查询系统设计

#### 阅读材料
1. EnTT View实现解析
2. Bevy Query系统设计
3. 迭代器模式详解

#### 核心概念

**查询设计**
```cpp
// 查询需求：高效地找到具有特定组件组合的实体

// 方案1：交集计算（Sparse Set风格）
template<typename... Ts>
class View {
    std::tuple<SparseSet<Ts>*...> pools_;

    // 找最小的池作为主导
    SparseSet<>* leading_;

public:
    class Iterator {
        // 遍历leading pool，过滤其他pool
    };

    auto begin() {
        // 从最小池开始
    }
};

// 方案2：Archetype匹配
template<typename... Ts>
class Query {
    std::vector<Archetype*> matchingArchetypes_;

public:
    void update() {
        matchingArchetypes_.clear();
        for (auto& archetype : world_.archetypes()) {
            if (archetype.hasAll<Ts...>()) {
                matchingArchetypes_.push_back(&archetype);
            }
        }
    }

    auto begin() {
        // 遍历所有匹配的Archetype
    }
};
```

**过滤器与修饰符**
```cpp
// 常见的查询修饰符

// With: 必须有组件（但不访问）
query<Position, Velocity>().with<Active>()

// Without: 必须没有组件
query<Position>().without<Static>()

// Optional: 可选组件
query<Position, Optional<Velocity>>()

// Changed: 组件发生变化
query<Position>().changed<Position>()

// Added: 新添加的组件
query<Position>().added<Position>()

// 示例实现
template<typename T>
struct Optional {
    T* ptr = nullptr;
    bool has() const { return ptr != nullptr; }
    T& get() { return *ptr; }
};

template<typename T>
struct With {};  // 标记类型

template<typename T>
struct Without {};  // 标记类型
```

### 第四周：系统调度与并行

#### 阅读材料
1. 任务图调度算法
2. 数据依赖分析
3. Lock-free编程
4. Job System设计

#### 核心概念

**系统依赖图**
```
┌─────────────────────────────────────────────────────────┐
│                    系统依赖分析                          │
└─────────────────────────────────────────────────────────┘

系统及其访问的组件：
InputSystem:     Read<Input>
MovementSystem:  Read<Velocity>, Write<Position>
PhysicsSystem:   Write<Velocity>, Read<Position>
RenderSystem:    Read<Position>, Read<Sprite>
AISystem:        Read<Position>, Write<Velocity>

依赖图：
                InputSystem
                     │
                     ▼
               MovementSystem
                     │
            ┌────────┴────────┐
            ▼                 ▼
      PhysicsSystem      AISystem  (冲突！都写Velocity)
            │                 │
            └────────┬────────┘
                     ▼
               RenderSystem

并行执行组：
Stage 1: [InputSystem]
Stage 2: [MovementSystem]
Stage 3: [PhysicsSystem] 或 [AISystem]  (互斥)
Stage 4: [RenderSystem]

或使用更细粒度的并行：
- PhysicsSystem和AISystem操作不同实体时可并行
```

**系统注册与调度**
```cpp
// 系统特征
template<typename T>
struct SystemTraits {
    // 需要特化来定义读写组件
    using reads = std::tuple<>;
    using writes = std::tuple<>;
};

// 示例
template<>
struct SystemTraits<MovementSystem> {
    using reads = std::tuple<Velocity>;
    using writes = std::tuple<Position>;
};

// 调度器
class Scheduler {
    struct SystemNode {
        std::function<void()> execute;
        std::vector<size_t> dependencies;
        std::vector<std::type_index> reads;
        std::vector<std::type_index> writes;
    };

    std::vector<SystemNode> systems_;

    void buildDependencyGraph() {
        for (size_t i = 0; i < systems_.size(); ++i) {
            for (size_t j = 0; j < i; ++j) {
                if (hasConflict(systems_[i], systems_[j])) {
                    systems_[i].dependencies.push_back(j);
                }
            }
        }
    }

    bool hasConflict(const SystemNode& a, const SystemNode& b) {
        // 检查读写冲突
        for (const auto& write : a.writes) {
            for (const auto& read : b.reads) {
                if (write == read) return true;
            }
            for (const auto& otherWrite : b.writes) {
                if (write == otherWrite) return true;
            }
        }
        // 反向检查
        for (const auto& write : b.writes) {
            for (const auto& read : a.reads) {
                if (write == read) return true;
            }
        }
        return false;
    }
};
```

---

## 源码阅读任务

### 必读项目

1. **EnTT** (https://github.com/skypjack/entt)
   - 重点文件：`src/entt/entity/`, `src/entt/core/`
   - 学习目标：理解Sparse Set ECS实现
   - 阅读时间：15小时

2. **Flecs** (https://github.com/SanderMertens/flecs)
   - 重点：查询系统和关系
   - 学习目标：理解高级ECS特性
   - 阅读时间：10小时

3. **Bevy ECS** (https://github.com/bevyengine/bevy)
   - 重点：`crates/bevy_ecs/`
   - 学习目标：理解Archetype存储
   - 阅读时间：8小时

---

## 实践项目：高性能ECS框架

### 项目概述
从零构建一个高性能ECS框架，支持高效查询、系统调度和并行执行。

### 完整代码实现

#### 1. 基础类型定义 (ecs/core/types.hpp)

```cpp
#pragma once

#include <cstdint>
#include <limits>
#include <typeindex>
#include <typeinfo>
#include <atomic>

namespace ecs {

// Entity ID: 32位索引 + 32位版本
struct Entity {
    uint32_t index;
    uint32_t version;

    bool operator==(const Entity& other) const {
        return index == other.index && version == other.version;
    }

    bool operator!=(const Entity& other) const {
        return !(*this == other);
    }

    bool isValid() const {
        return index != UINT32_MAX;
    }

    static Entity invalid() {
        return {UINT32_MAX, 0};
    }
};

// Entity哈希（用于unordered_map）
struct EntityHash {
    size_t operator()(const Entity& e) const {
        return std::hash<uint64_t>()(
            (static_cast<uint64_t>(e.version) << 32) | e.index
        );
    }
};

// 组件类型ID
using ComponentId = uint32_t;

// 类型ID生成器
class TypeIdGenerator {
    static std::atomic<ComponentId> counter_;

public:
    template<typename T>
    static ComponentId get() {
        static ComponentId id = counter_++;
        return id;
    }
};

inline std::atomic<ComponentId> TypeIdGenerator::counter_{0};

// 组件类型信息
struct ComponentInfo {
    ComponentId id;
    size_t size;
    size_t alignment;
    std::string name;

    // 构造/析构函数指针
    void (*construct)(void*);
    void (*destruct)(void*);
    void (*move)(void* dst, void* src);
    void (*copy)(void* dst, const void* src);
};

template<typename T>
ComponentInfo makeComponentInfo() {
    return ComponentInfo{
        TypeIdGenerator::get<T>(),
        sizeof(T),
        alignof(T),
        typeid(T).name(),
        [](void* ptr) { new (ptr) T(); },
        [](void* ptr) { static_cast<T*>(ptr)->~T(); },
        [](void* dst, void* src) {
            new (dst) T(std::move(*static_cast<T*>(src)));
        },
        [](void* dst, const void* src) {
            new (dst) T(*static_cast<const T*>(src));
        }
    };
}

// 组件掩码（用于快速组件组合判断）
using ComponentMask = uint64_t;

constexpr size_t MAX_COMPONENTS = 64;

template<typename... Ts>
ComponentMask makeMask() {
    ComponentMask mask = 0;
    ((mask |= (1ULL << TypeIdGenerator::get<Ts>())), ...);
    return mask;
}

} // namespace ecs
```

#### 2. 组件存储池 (ecs/storage/component_pool.hpp)

```cpp
#pragma once

#include "../core/types.hpp"
#include <vector>
#include <memory>
#include <cstring>

namespace ecs {

// 非类型安全的组件池基类
class ComponentPoolBase {
public:
    virtual ~ComponentPoolBase() = default;
    virtual void remove(uint32_t index) = 0;
    virtual bool contains(uint32_t index) const = 0;
    virtual size_t size() const = 0;
    virtual void* getRaw(uint32_t index) = 0;
    virtual const ComponentInfo& info() const = 0;
};

// 类型安全的组件池（Sparse Set实现）
template<typename T>
class ComponentPool : public ComponentPoolBase {
private:
    std::vector<T> dense_;           // 组件数据
    std::vector<uint32_t> sparse_;   // Entity索引 -> dense索引
    std::vector<uint32_t> entities_; // dense索引 -> Entity索引
    ComponentInfo info_;

    static constexpr uint32_t INVALID = UINT32_MAX;

    void ensureSparseSize(uint32_t index) {
        if (index >= sparse_.size()) {
            sparse_.resize(index + 1, INVALID);
        }
    }

public:
    ComponentPool() : info_(makeComponentInfo<T>()) {}

    // 插入组件
    template<typename... Args>
    T& emplace(uint32_t entityIndex, Args&&... args) {
        ensureSparseSize(entityIndex);

        if (sparse_[entityIndex] != INVALID) {
            // 已存在，替换
            return dense_[sparse_[entityIndex]] = T(std::forward<Args>(args)...);
        }

        // 新增
        sparse_[entityIndex] = static_cast<uint32_t>(dense_.size());
        dense_.emplace_back(std::forward<Args>(args)...);
        entities_.push_back(entityIndex);

        return dense_.back();
    }

    // 插入组件（拷贝）
    T& insert(uint32_t entityIndex, const T& component) {
        return emplace(entityIndex, component);
    }

    // 移除组件
    void remove(uint32_t entityIndex) override {
        if (entityIndex >= sparse_.size() || sparse_[entityIndex] == INVALID) {
            return;
        }

        uint32_t denseIdx = sparse_[entityIndex];
        uint32_t lastEntity = entities_.back();

        // 交换到末尾
        if (denseIdx != dense_.size() - 1) {
            dense_[denseIdx] = std::move(dense_.back());
            entities_[denseIdx] = lastEntity;
            sparse_[lastEntity] = denseIdx;
        }

        dense_.pop_back();
        entities_.pop_back();
        sparse_[entityIndex] = INVALID;
    }

    // 获取组件
    T* get(uint32_t entityIndex) {
        if (entityIndex >= sparse_.size() || sparse_[entityIndex] == INVALID) {
            return nullptr;
        }
        return &dense_[sparse_[entityIndex]];
    }

    const T* get(uint32_t entityIndex) const {
        if (entityIndex >= sparse_.size() || sparse_[entityIndex] == INVALID) {
            return nullptr;
        }
        return &dense_[sparse_[entityIndex]];
    }

    void* getRaw(uint32_t entityIndex) override {
        return get(entityIndex);
    }

    // 检查是否包含
    bool contains(uint32_t entityIndex) const override {
        return entityIndex < sparse_.size() && sparse_[entityIndex] != INVALID;
    }

    // 大小
    size_t size() const override { return dense_.size(); }

    // 组件信息
    const ComponentInfo& info() const override { return info_; }

    // 迭代器
    auto begin() { return dense_.begin(); }
    auto end() { return dense_.end(); }
    auto begin() const { return dense_.begin(); }
    auto end() const { return dense_.end(); }

    // 获取实体列表
    const std::vector<uint32_t>& entities() const { return entities_; }

    // 获取dense数组（用于批量操作）
    T* data() { return dense_.data(); }
    const T* data() const { return dense_.data(); }

    // 获取dense索引
    uint32_t getDenseIndex(uint32_t entityIndex) const {
        if (entityIndex >= sparse_.size()) return INVALID;
        return sparse_[entityIndex];
    }
};

} // namespace ecs
```

#### 3. 实体管理器 (ecs/core/entity_manager.hpp)

```cpp
#pragma once

#include "types.hpp"
#include <vector>
#include <queue>
#include <cassert>

namespace ecs {

class EntityManager {
private:
    std::vector<uint32_t> versions_;       // 每个槽位的版本号
    std::vector<ComponentMask> masks_;      // 每个实体的组件掩码
    std::queue<uint32_t> freeList_;         // 可重用的槽位
    uint32_t entityCount_ = 0;

public:
    // 创建实体
    Entity create() {
        uint32_t index;

        if (!freeList_.empty()) {
            index = freeList_.front();
            freeList_.pop();
        } else {
            index = static_cast<uint32_t>(versions_.size());
            versions_.push_back(0);
            masks_.push_back(0);
        }

        ++entityCount_;
        return Entity{index, versions_[index]};
    }

    // 销毁实体
    void destroy(Entity entity) {
        if (!isValid(entity)) return;

        // 增加版本号使旧引用失效
        ++versions_[entity.index];
        masks_[entity.index] = 0;
        freeList_.push(entity.index);
        --entityCount_;
    }

    // 验证实体
    bool isValid(Entity entity) const {
        return entity.index < versions_.size() &&
               versions_[entity.index] == entity.version;
    }

    // 组件掩码操作
    void addComponent(Entity entity, ComponentId componentId) {
        if (isValid(entity)) {
            masks_[entity.index] |= (1ULL << componentId);
        }
    }

    void removeComponent(Entity entity, ComponentId componentId) {
        if (isValid(entity)) {
            masks_[entity.index] &= ~(1ULL << componentId);
        }
    }

    bool hasComponent(Entity entity, ComponentId componentId) const {
        if (!isValid(entity)) return false;
        return (masks_[entity.index] & (1ULL << componentId)) != 0;
    }

    ComponentMask getMask(Entity entity) const {
        if (!isValid(entity)) return 0;
        return masks_[entity.index];
    }

    bool matchesMask(Entity entity, ComponentMask requiredMask) const {
        if (!isValid(entity)) return false;
        return (masks_[entity.index] & requiredMask) == requiredMask;
    }

    // 统计
    uint32_t count() const { return entityCount_; }
    uint32_t capacity() const { return static_cast<uint32_t>(versions_.size()); }
};

} // namespace ecs
```

#### 4. 视图/查询系统 (ecs/query/view.hpp)

```cpp
#pragma once

#include "../storage/component_pool.hpp"
#include <tuple>
#include <algorithm>

namespace ecs {

// 查找最小池的辅助
template<typename... Pools>
auto* findSmallestPool(Pools*... pools) {
    std::array<std::pair<size_t, ComponentPoolBase*>, sizeof...(Pools)> sizes = {
        std::make_pair(pools->size(), static_cast<ComponentPoolBase*>(pools))...
    };
    auto it = std::min_element(sizes.begin(), sizes.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    return it->second;
}

// View迭代器
template<typename... Ts>
class ViewIterator {
private:
    std::tuple<ComponentPool<Ts>*...> pools_;
    const std::vector<uint32_t>* entities_;
    size_t index_;
    size_t endIndex_;

    bool isValid() const {
        if (index_ >= endIndex_) return false;

        uint32_t entity = (*entities_)[index_];

        // 检查所有池都包含此实体
        return std::apply([entity](auto*... pools) {
            return (pools->contains(entity) && ...);
        }, pools_);
    }

    void advance() {
        ++index_;
        while (index_ < endIndex_ && !isValid()) {
            ++index_;
        }
    }

public:
    ViewIterator(std::tuple<ComponentPool<Ts>*...> pools,
                 const std::vector<uint32_t>* entities,
                 size_t index, size_t endIndex)
        : pools_(pools), entities_(entities),
          index_(index), endIndex_(endIndex) {

        // 找到第一个有效位置
        if (index_ < endIndex_ && !isValid()) {
            advance();
        }
    }

    bool operator!=(const ViewIterator& other) const {
        return index_ != other.index_;
    }

    ViewIterator& operator++() {
        advance();
        return *this;
    }

    // 返回元组：(Entity, Components...)
    auto operator*() const {
        uint32_t entityIndex = (*entities_)[index_];
        return std::tuple_cat(
            std::make_tuple(Entity{entityIndex, 0}),  // 版本需要从EntityManager获取
            std::make_tuple(std::ref(*std::get<ComponentPool<Ts>*>(pools_)->get(entityIndex))...)
        );
    }
};

// View: 用于遍历具有特定组件的实体
template<typename... Ts>
class View {
private:
    std::tuple<ComponentPool<Ts>*...> pools_;
    ComponentPool<std::tuple_element_t<0, std::tuple<Ts...>>>* leadingPool_;

public:
    explicit View(ComponentPool<Ts>*... pools)
        : pools_(pools...) {
        // 选择最小的池作为主导池
        auto smallest = findSmallestPool(pools...);
        leadingPool_ = std::get<0>(pools_);

        // 找真正最小的
        size_t minSize = leadingPool_->size();
        std::apply([&](auto*... ps) {
            ((ps->size() < minSize ?
              (leadingPool_ = reinterpret_cast<decltype(leadingPool_)>(ps),
               minSize = ps->size()) : 0), ...);
        }, pools_);
    }

    auto begin() {
        return ViewIterator<Ts...>(
            pools_,
            &leadingPool_->entities(),
            0,
            leadingPool_->entities().size()
        );
    }

    auto end() {
        return ViewIterator<Ts...>(
            pools_,
            &leadingPool_->entities(),
            leadingPool_->entities().size(),
            leadingPool_->entities().size()
        );
    }

    // 获取单个实体的组件
    std::tuple<Ts*...> get(uint32_t entityIndex) {
        return std::make_tuple(
            std::get<ComponentPool<Ts>*>(pools_)->get(entityIndex)...
        );
    }

    // 检查实体是否在视图中
    bool contains(uint32_t entityIndex) const {
        return std::apply([entityIndex](auto*... pools) {
            return (pools->contains(entityIndex) && ...);
        }, pools_);
    }

    // 对每个匹配实体执行函数
    template<typename Func>
    void each(Func&& func) {
        for (auto it = begin(); it != end(); ++it) {
            std::apply(std::forward<Func>(func), *it);
        }
    }

    // 并行版本
    template<typename Func>
    void parallelEach(Func&& func, size_t minBatchSize = 64) {
        const auto& entities = leadingPool_->entities();
        const size_t count = entities.size();

        if (count < minBatchSize * 2) {
            each(std::forward<Func>(func));
            return;
        }

        const size_t threadCount = std::thread::hardware_concurrency();
        const size_t batchSize = std::max(minBatchSize,
                                          (count + threadCount - 1) / threadCount);

        std::vector<std::thread> threads;

        for (size_t start = 0; start < count; start += batchSize) {
            size_t end = std::min(start + batchSize, count);

            threads.emplace_back([this, &func, &entities, start, end]() {
                for (size_t i = start; i < end; ++i) {
                    uint32_t entityIndex = entities[i];

                    if (!contains(entityIndex)) continue;

                    auto components = get(entityIndex);
                    std::apply([&](auto*... ptrs) {
                        if ((ptrs && ...)) {
                            func(Entity{entityIndex, 0}, *ptrs...);
                        }
                    }, components);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }
};

} // namespace ecs
```

#### 5. 世界 (ecs/world/world.hpp)

```cpp
#pragma once

#include "../core/entity_manager.hpp"
#include "../storage/component_pool.hpp"
#include "../query/view.hpp"
#include <unordered_map>
#include <memory>
#include <functional>

namespace ecs {

class World {
private:
    EntityManager entityManager_;
    std::unordered_map<ComponentId, std::unique_ptr<ComponentPoolBase>> componentPools_;

    // 系统
    struct SystemEntry {
        std::string name;
        std::function<void(World&, float)> update;
        int priority = 0;
    };
    std::vector<SystemEntry> systems_;

    // 延迟操作队列
    struct Command {
        enum Type { CreateEntity, DestroyEntity, AddComponent, RemoveComponent };
        Type type;
        Entity entity;
        std::function<void()> execute;
    };
    std::vector<Command> commandQueue_;

    template<typename T>
    ComponentPool<T>* getOrCreatePool() {
        ComponentId id = TypeIdGenerator::get<T>();

        auto it = componentPools_.find(id);
        if (it != componentPools_.end()) {
            return static_cast<ComponentPool<T>*>(it->second.get());
        }

        auto pool = std::make_unique<ComponentPool<T>>();
        auto* ptr = pool.get();
        componentPools_[id] = std::move(pool);
        return ptr;
    }

public:
    // 创建实体
    Entity createEntity() {
        return entityManager_.create();
    }

    // 销毁实体
    void destroyEntity(Entity entity) {
        if (!entityManager_.isValid(entity)) return;

        // 从所有组件池移除
        for (auto& [id, pool] : componentPools_) {
            pool->remove(entity.index);
        }

        entityManager_.destroy(entity);
    }

    // 延迟销毁
    void destroyEntityDeferred(Entity entity) {
        commandQueue_.push_back({
            Command::DestroyEntity,
            entity,
            [this, entity]() { destroyEntity(entity); }
        });
    }

    // 检查实体有效性
    bool isValid(Entity entity) const {
        return entityManager_.isValid(entity);
    }

    // 添加组件
    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args) {
        assert(entityManager_.isValid(entity));

        auto* pool = getOrCreatePool<T>();
        entityManager_.addComponent(entity, TypeIdGenerator::get<T>());

        return pool->emplace(entity.index, std::forward<Args>(args)...);
    }

    // 移除组件
    template<typename T>
    void removeComponent(Entity entity) {
        if (!entityManager_.isValid(entity)) return;

        ComponentId id = TypeIdGenerator::get<T>();
        auto it = componentPools_.find(id);
        if (it != componentPools_.end()) {
            it->second->remove(entity.index);
            entityManager_.removeComponent(entity, id);
        }
    }

    // 获取组件
    template<typename T>
    T* getComponent(Entity entity) {
        if (!entityManager_.isValid(entity)) return nullptr;

        ComponentId id = TypeIdGenerator::get<T>();
        auto it = componentPools_.find(id);
        if (it == componentPools_.end()) return nullptr;

        return static_cast<ComponentPool<T>*>(it->second.get())->get(entity.index);
    }

    template<typename T>
    const T* getComponent(Entity entity) const {
        return const_cast<World*>(this)->getComponent<T>(entity);
    }

    // 检查是否有组件
    template<typename T>
    bool hasComponent(Entity entity) const {
        return entityManager_.hasComponent(entity, TypeIdGenerator::get<T>());
    }

    // 创建视图/查询
    template<typename... Ts>
    View<Ts...> view() {
        return View<Ts...>(getOrCreatePool<Ts>()...);
    }

    // 获取组件池
    template<typename T>
    ComponentPool<T>* pool() {
        return getOrCreatePool<T>();
    }

    // 注册系统
    template<typename Func>
    void registerSystem(const std::string& name, Func&& func, int priority = 0) {
        systems_.push_back({
            name,
            std::forward<Func>(func),
            priority
        });

        // 按优先级排序
        std::sort(systems_.begin(), systems_.end(),
            [](const auto& a, const auto& b) {
                return a.priority < b.priority;
            });
    }

    // 更新所有系统
    void update(float deltaTime) {
        // 先执行延迟命令
        flushCommands();

        // 执行所有系统
        for (auto& sys : systems_) {
            sys.update(*this, deltaTime);
        }

        // 再次执行延迟命令
        flushCommands();
    }

    // 执行延迟命令
    void flushCommands() {
        for (auto& cmd : commandQueue_) {
            cmd.execute();
        }
        commandQueue_.clear();
    }

    // 统计
    size_t entityCount() const { return entityManager_.count(); }

    template<typename T>
    size_t componentCount() const {
        ComponentId id = TypeIdGenerator::get<T>();
        auto it = componentPools_.find(id);
        return it != componentPools_.end() ? it->second->size() : 0;
    }

    // 遍历所有实体
    template<typename Func>
    void forEachEntity(Func&& func) {
        for (uint32_t i = 0; i < entityManager_.capacity(); ++i) {
            Entity e{i, 0};  // 需要正确的版本
            if (entityManager_.isValid(e)) {
                func(e);
            }
        }
    }
};

} // namespace ecs
```

#### 6. 常用组件定义 (ecs/components/common.hpp)

```cpp
#pragma once

#include <string>

namespace ecs {

// 变换组件
struct Transform {
    float x = 0, y = 0, z = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
};

// 速度组件
struct Velocity {
    float x = 0, y = 0, z = 0;
};

// 刚体组件
struct RigidBody {
    float mass = 1.0f;
    float drag = 0.0f;
    bool useGravity = true;
    bool isKinematic = false;
};

// 碰撞器组件（AABB）
struct BoxCollider {
    float halfWidth = 0.5f;
    float halfHeight = 0.5f;
    float halfDepth = 0.5f;
    bool isTrigger = false;
};

// 球形碰撞器
struct SphereCollider {
    float radius = 0.5f;
    bool isTrigger = false;
};

// 渲染组件
struct Renderable {
    uint32_t meshId = 0;
    uint32_t materialId = 0;
    bool visible = true;
    bool castShadow = true;
};

// 名称组件
struct Name {
    std::string value;
};

// 标签组件（空组件用于标记）
struct Active {};
struct Static {};
struct Player {};
struct Enemy {};

// 生命周期组件
struct Lifetime {
    float remaining = 1.0f;
};

// 父子关系
struct Parent {
    Entity entity = Entity::invalid();
};

struct Children {
    std::vector<Entity> entities;
};

} // namespace ecs
```

#### 7. 常用系统 (ecs/systems/common_systems.hpp)

```cpp
#pragma once

#include "../world/world.hpp"
#include "../components/common.hpp"

namespace ecs::systems {

// 移动系统
inline void MovementSystem(World& world, float dt) {
    world.view<Transform, Velocity>().each(
        [dt](Entity entity, Transform& transform, Velocity& velocity) {
            transform.x += velocity.x * dt;
            transform.y += velocity.y * dt;
            transform.z += velocity.z * dt;
        }
    );
}

// 重力系统
inline void GravitySystem(World& world, float dt) {
    constexpr float GRAVITY = -9.8f;

    world.view<Velocity, RigidBody>().each(
        [dt](Entity entity, Velocity& velocity, RigidBody& body) {
            if (body.useGravity && !body.isKinematic) {
                velocity.y += GRAVITY * dt;
            }
        }
    );
}

// 生命周期系统
inline void LifetimeSystem(World& world, float dt) {
    std::vector<Entity> toDestroy;

    world.view<Lifetime>().each(
        [&toDestroy, dt](Entity entity, Lifetime& lifetime) {
            lifetime.remaining -= dt;
            if (lifetime.remaining <= 0) {
                toDestroy.push_back(entity);
            }
        }
    );

    for (Entity e : toDestroy) {
        world.destroyEntityDeferred(e);
    }
}

// 层级更新系统
inline void HierarchySystem(World& world, float dt) {
    // 收集根实体（没有父节点的）
    std::vector<Entity> roots;

    world.view<Transform>().each(
        [&world, &roots](Entity entity, Transform& transform) {
            if (!world.hasComponent<Parent>(entity)) {
                roots.push_back(entity);
            }
        }
    );

    // 递归更新层级
    std::function<void(Entity, const Transform&)> updateHierarchy;
    updateHierarchy = [&](Entity entity, const Transform& parentWorld) {
        auto* transform = world.getComponent<Transform>(entity);
        if (!transform) return;

        // 计算世界变换（简化：只处理位移）
        Transform worldTransform;
        worldTransform.x = parentWorld.x + transform->x;
        worldTransform.y = parentWorld.y + transform->y;
        worldTransform.z = parentWorld.z + transform->z;

        // 更新子节点
        if (auto* children = world.getComponent<Children>(entity)) {
            for (Entity child : children->entities) {
                if (world.isValid(child)) {
                    updateHierarchy(child, worldTransform);
                }
            }
        }
    };

    Transform identity;
    for (Entity root : roots) {
        updateHierarchy(root, identity);
    }
}

// 碰撞检测系统（简化版）
inline void CollisionSystem(World& world, float dt) {
    auto& transforms = *world.pool<Transform>();
    auto& colliders = *world.pool<BoxCollider>();

    const auto& entities = colliders.entities();

    // O(n^2) 简单检测
    for (size_t i = 0; i < entities.size(); ++i) {
        uint32_t entityA = entities[i];
        auto* transA = transforms.get(entityA);
        auto* collA = colliders.get(entityA);
        if (!transA || !collA) continue;

        for (size_t j = i + 1; j < entities.size(); ++j) {
            uint32_t entityB = entities[j];
            auto* transB = transforms.get(entityB);
            auto* collB = colliders.get(entityB);
            if (!transB || !collB) continue;

            // AABB碰撞检测
            bool overlap =
                std::abs(transA->x - transB->x) < (collA->halfWidth + collB->halfWidth) &&
                std::abs(transA->y - transB->y) < (collA->halfHeight + collB->halfHeight) &&
                std::abs(transA->z - transB->z) < (collA->halfDepth + collB->halfDepth);

            if (overlap) {
                // 碰撞发生，可以触发事件或回调
            }
        }
    }
}

// Debug打印系统
inline void DebugPrintSystem(World& world, float dt) {
    static float timer = 0;
    timer += dt;

    if (timer >= 1.0f) {
        timer = 0;

        std::cout << "=== ECS Debug ===" << std::endl;
        std::cout << "Entities: " << world.entityCount() << std::endl;
        std::cout << "Transforms: " << world.componentCount<Transform>() << std::endl;
        std::cout << "Velocities: " << world.componentCount<Velocity>() << std::endl;
    }
}

} // namespace ecs::systems
```

#### 8. 使用示例与基准测试 (main.cpp)

```cpp
#include "ecs/world/world.hpp"
#include "ecs/components/common.hpp"
#include "ecs/systems/common_systems.hpp"
#include <iostream>
#include <chrono>
#include <random>

using namespace ecs;

void basicExample() {
    std::cout << "=== Basic ECS Example ===\n\n";

    World world;

    // 创建一些实体
    for (int i = 0; i < 10; ++i) {
        Entity e = world.createEntity();

        world.addComponent<Transform>(e, Transform{
            static_cast<float>(i), 0.0f, 0.0f
        });

        world.addComponent<Velocity>(e, Velocity{
            1.0f, 0.0f, 0.0f
        });

        if (i % 2 == 0) {
            world.addComponent<Name>(e, Name{"Entity_" + std::to_string(i)});
        }
    }

    std::cout << "Created " << world.entityCount() << " entities\n";

    // 注册系统
    world.registerSystem("Movement", systems::MovementSystem, 10);

    // 模拟几帧
    for (int frame = 0; frame < 5; ++frame) {
        std::cout << "\nFrame " << frame << ":\n";

        world.update(0.016f);

        // 打印位置
        world.view<Transform, Name>().each(
            [](Entity e, Transform& t, Name& n) {
                std::cout << "  " << n.value << ": ("
                          << t.x << ", " << t.y << ", " << t.z << ")\n";
            }
        );
    }
}

void benchmark() {
    std::cout << "\n=== ECS Benchmark ===\n\n";

    constexpr size_t ENTITY_COUNTS[] = {10000, 50000, 100000, 500000};
    constexpr int FRAMES = 100;
    constexpr float DT = 1.0f / 60.0f;

    for (size_t entityCount : ENTITY_COUNTS) {
        World world;

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

        // 创建实体
        for (size_t i = 0; i < entityCount; ++i) {
            Entity e = world.createEntity();

            world.addComponent<Transform>(e, Transform{
                dist(rng), dist(rng), dist(rng)
            });

            world.addComponent<Velocity>(e, Velocity{
                dist(rng) * 0.1f, dist(rng) * 0.1f, dist(rng) * 0.1f
            });
        }

        // 预热
        for (int i = 0; i < 10; ++i) {
            world.view<Transform, Velocity>().each(
                [DT](Entity e, Transform& t, Velocity& v) {
                    t.x += v.x * DT;
                    t.y += v.y * DT;
                    t.z += v.z * DT;
                }
            );
        }

        // 基准测试：串行
        auto start = std::chrono::high_resolution_clock::now();

        for (int frame = 0; frame < FRAMES; ++frame) {
            world.view<Transform, Velocity>().each(
                [DT](Entity e, Transform& t, Velocity& v) {
                    t.x += v.x * DT;
                    t.y += v.y * DT;
                    t.z += v.z * DT;
                }
            );
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto serialTime = std::chrono::duration<double, std::milli>(end - start).count();

        // 基准测试：并行
        start = std::chrono::high_resolution_clock::now();

        for (int frame = 0; frame < FRAMES; ++frame) {
            world.view<Transform, Velocity>().parallelEach(
                [DT](Entity e, Transform& t, Velocity& v) {
                    t.x += v.x * DT;
                    t.y += v.y * DT;
                    t.z += v.z * DT;
                }
            );
        }

        end = std::chrono::high_resolution_clock::now();
        auto parallelTime = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "Entities: " << entityCount << "\n";
        std::cout << "  Serial:   " << serialTime / FRAMES << " ms/frame\n";
        std::cout << "  Parallel: " << parallelTime / FRAMES << " ms/frame\n";
        std::cout << "  Speedup:  " << serialTime / parallelTime << "x\n\n";
    }
}

void gameSimulation() {
    std::cout << "\n=== Game Simulation ===\n\n";

    World world;

    // 注册系统
    world.registerSystem("Gravity", systems::GravitySystem, 0);
    world.registerSystem("Movement", systems::MovementSystem, 10);
    world.registerSystem("Lifetime", systems::LifetimeSystem, 20);
    world.registerSystem("Collision", systems::CollisionSystem, 30);

    // 创建玩家
    Entity player = world.createEntity();
    world.addComponent<Transform>(player, Transform{0, 10, 0});
    world.addComponent<Velocity>(player);
    world.addComponent<RigidBody>(player, RigidBody{1.0f, 0.0f, true, false});
    world.addComponent<BoxCollider>(player, BoxCollider{0.5f, 1.0f, 0.5f, false});
    world.addComponent<Name>(player, Name{"Player"});
    world.addComponent<Player>(player);

    // 创建地面
    Entity ground = world.createEntity();
    world.addComponent<Transform>(ground, Transform{0, -1, 0});
    world.addComponent<BoxCollider>(ground, BoxCollider{50.0f, 1.0f, 50.0f, false});
    world.addComponent<Name>(ground, Name{"Ground"});
    world.addComponent<Static>(ground);

    // 创建一些敌人
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(-20.0f, 20.0f);

    for (int i = 0; i < 5; ++i) {
        Entity enemy = world.createEntity();
        world.addComponent<Transform>(enemy, Transform{
            posDist(rng), 5.0f, posDist(rng)
        });
        world.addComponent<Velocity>(enemy, Velocity{
            posDist(rng) * 0.1f, 0.0f, posDist(rng) * 0.1f
        });
        world.addComponent<RigidBody>(enemy);
        world.addComponent<BoxCollider>(enemy);
        world.addComponent<Name>(enemy, Name{"Enemy_" + std::to_string(i)});
        world.addComponent<Enemy>(enemy);
    }

    // 创建一些临时粒子
    for (int i = 0; i < 10; ++i) {
        Entity particle = world.createEntity();
        world.addComponent<Transform>(particle, Transform{0, 5, 0});
        world.addComponent<Velocity>(particle, Velocity{
            posDist(rng), posDist(rng), posDist(rng)
        });
        world.addComponent<Lifetime>(particle, Lifetime{1.0f + i * 0.2f});
    }

    std::cout << "Initial entities: " << world.entityCount() << "\n\n";

    // 模拟
    for (int frame = 0; frame < 120; ++frame) {
        world.update(1.0f / 60.0f);

        if (frame % 30 == 0) {
            std::cout << "Frame " << frame << ": "
                      << world.entityCount() << " entities\n";

            // 打印玩家位置
            if (auto* playerTrans = world.getComponent<Transform>(player)) {
                std::cout << "  Player Y: " << playerTrans->y << "\n";
            }
        }
    }

    std::cout << "\nFinal entities: " << world.entityCount() << "\n";
}

int main() {
    basicExample();
    benchmark();
    gameSimulation();

    return 0;
}
```

---

## 检验标准

### 知识检验
1. [ ] 能够解释ECS与传统OOP的区别
2. [ ] 理解Sparse Set和Archetype存储的优缺点
3. [ ] 掌握View/Query的实现原理
4. [ ] 理解系统调度和依赖分析
5. [ ] 能够设计合理的组件拆分

### 实践检验
1. [ ] 完成ECS框架核心实现
2. [ ] 支持组件的添加/删除/查询
3. [ ] View遍历正确且高效
4. [ ] 并行遍历正确工作
5. [ ] 100万实体达到良好性能

### 代码质量
1. [ ] 类型安全，编译期检查
2. [ ] 内存管理正确
3. [ ] 接口设计清晰易用
4. [ ] 有完整的测试

---

## 输出物清单

1. **学习笔记**
   - [ ] ECS架构设计笔记
   - [ ] 存储策略对比分析
   - [ ] 源码阅读笔记

2. **代码产出**
   - [ ] 高性能ECS框架
   - [ ] 常用组件和系统库
   - [ ] 基准测试套件

3. **文档产出**
   - [ ] ECS设计文档
   - [ ] API使用指南
   - [ ] 性能调优指南

---

## 时间分配表

| 周次 | 理论学习 | 源码阅读 | 项目实践 | 总计 |
|------|----------|----------|----------|------|
| Week 1 | 15h | 12h | 8h | 35h |
| Week 2 | 12h | 10h | 13h | 35h |
| Week 3 | 10h | 6h | 19h | 35h |
| Week 4 | 8h | 5h | 22h | 35h |
| **总计** | **45h** | **33h** | **62h** | **140h** |

---

## 下月预告

**Month 54: SYCL与异构计算入门**

下个月将进入异构计算领域：
- GPU计算基础概念
- SYCL标准与实现
- 内存模型与数据传输
- 并行执行模型
- 实践项目：使用SYCL实现矩阵运算

建议提前：
1. 了解GPU架构基础
2. 复习并行计算概念
3. 安装SYCL编译器（Intel DPC++或hipSYCL）
