# Month 52: 面向数据设计实践 (Data-Oriented Design in Practice)

## 本月主题概述

在上月学习DOD基础之后，本月将把这些原理应用于更复杂的实际场景。我们将实现DOD风格的空间数据结构、碰撞检测系统，并探索多线程环境下的DOD设计模式。通过构建一个高性能物理引擎核心，深入理解DOD在复杂系统中的应用。

### 学习目标
- 掌握空间数据结构的DOD实现
- 实现高效的宽相位和窄相位碰撞检测
- 理解多线程DOD设计模式
- 构建可扩展的高性能物理引擎核心
- 学会性能分析与调优

---

## 理论学习内容

### 第一周：空间数据结构的DOD实现

#### 阅读材料
1. 《Real-Time Collision Detection》- Christer Ericson
2. 《Game Physics Engine Development》- Ian Millington
3. Bullet物理引擎设计文档
4. Box2D源码分析文章

#### 核心概念

**空间划分策略比较**
```
┌─────────────────────────────────────────────────────────┐
│                    空间划分方法                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  均匀网格 (Uniform Grid)          四叉树/八叉树          │
│  ┌──┬──┬──┬──┐                   ┌───────┬───────┐     │
│  │  │  │  │  │                   │       │   ┌─┬─┤     │
│  ├──┼──┼──┼──┤                   │       │   ├─┼─┤     │
│  │  │  │  │  │                   ├───┬───┼───┴─┴─┤     │
│  ├──┼──┼──┼──┤                   │   │   │       │     │
│  │  │  │  │  │                   └───┴───┴───────┘     │
│  └──┴──┴──┴──┘                                         │
│  优点：简单、缓存友好             优点：自适应、节省空间   │
│  缺点：不适应不均匀分布           缺点：指针追踪、缓存不友好│
│                                                         │
│  BVH (层次包围盒)                SAP (扫掠与裁剪)        │
│       ●───────────●             按X排序: ───○─○──○──○─  │
│      / \         / \            按Y排序: ──○──○─○───○─  │
│     ●   ●       ●   ●           只检测重叠的轴区间       │
│    /|   |\     /|   |\          优点：增量更新、适合静态  │
│   ● ●   ● ●   ● ●   ● ●         缺点：不适合高速运动物体 │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

**DOD风格的网格实现**
```cpp
// 传统OOP方式 - 指针和虚函数
class GridCell {
    std::vector<GameObject*> objects;  // 指针追踪
};
class SpatialGrid {
    std::vector<std::vector<GridCell*>> cells;  // 二维指针数组
};

// DOD方式 - 紧凑数据
struct SpatialGridDOD {
    // 网格配置
    float originX, originY, originZ;
    float cellSize;
    int gridSizeX, gridSizeY, gridSizeZ;

    // 对象数据（SoA）
    std::vector<float> posX, posY, posZ;
    std::vector<float> halfExtentX, halfExtentY, halfExtentZ;
    std::vector<uint32_t> objectIds;

    // 网格映射 - 每个单元格存储对象索引范围
    std::vector<uint32_t> cellStart;  // 每个单元格的起始索引
    std::vector<uint32_t> cellCount;  // 每个单元格的对象数量
    std::vector<uint32_t> sortedIndices;  // 按单元格排序的对象索引

    // 计算对象所在的单元格
    int getCellIndex(float x, float y, float z) const {
        int cx = static_cast<int>((x - originX) / cellSize);
        int cy = static_cast<int>((y - originY) / cellSize);
        int cz = static_cast<int>((z - originZ) / cellSize);

        cx = std::clamp(cx, 0, gridSizeX - 1);
        cy = std::clamp(cy, 0, gridSizeY - 1);
        cz = std::clamp(cz, 0, gridSizeZ - 1);

        return cx + cy * gridSizeX + cz * gridSizeX * gridSizeY;
    }

    // 重建网格（每帧或对象移动后）
    void rebuild() {
        const size_t objectCount = posX.size();
        const size_t cellCount_ = gridSizeX * gridSizeY * gridSizeZ;

        // 重置计数
        std::fill(cellCount.begin(), cellCount.end(), 0);
        cellCount.resize(cellCount_);
        cellStart.resize(cellCount_);

        // 第一遍：计算每个单元格的对象数量
        for (size_t i = 0; i < objectCount; ++i) {
            int cell = getCellIndex(posX[i], posY[i], posZ[i]);
            ++cellCount[cell];
        }

        // 计算前缀和得到起始索引
        uint32_t total = 0;
        for (size_t i = 0; i < cellCount_; ++i) {
            cellStart[i] = total;
            total += cellCount[i];
        }

        // 第二遍：填充排序索引
        sortedIndices.resize(objectCount);
        std::vector<uint32_t> currentIndex = cellStart;

        for (size_t i = 0; i < objectCount; ++i) {
            int cell = getCellIndex(posX[i], posY[i], posZ[i]);
            sortedIndices[currentIndex[cell]++] = static_cast<uint32_t>(i);
        }
    }
};
```

### 第二周：碰撞检测系统

#### 阅读材料
1. 《Game Programming Gems 6》- 碰撞检测章节
2. GJK算法详解
3. SAT（分离轴定理）实现
4. PhysX碰撞检测文档

#### 核心概念

**碰撞检测流程**
```
┌─────────────────────────────────────────────────────────┐
│                    碰撞检测流程                          │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│              宽相位 (Broad Phase)                        │
│  - 快速剔除不可能碰撞的对象对                            │
│  - 使用空间数据结构                                      │
│  - 输出：潜在碰撞对列表                                  │
│  - 复杂度：O(n log n) 或 O(n)                            │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│              窄相位 (Narrow Phase)                       │
│  - 精确检测实际碰撞                                      │
│  - 计算碰撞点、法线、穿透深度                            │
│  - 使用GJK、SAT等算法                                    │
│  - 复杂度：O(k) where k = 潜在碰撞对数量                  │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│              碰撞响应 (Collision Response)               │
│  - 计算冲量                                              │
│  - 更新速度                                              │
│  - 位置修正                                              │
└─────────────────────────────────────────────────────────┘
```

**AABB碰撞检测 - SIMD优化**
```cpp
#include <immintrin.h>

// AABB结构（DOD风格，分离存储）
struct AABBSystem {
    // 存储为min/max分开的SoA
    std::vector<float> minX, minY, minZ;
    std::vector<float> maxX, maxY, maxZ;

    // 批量AABB vs AABB测试（AVX）
    // 测试一个AABB与多个AABB
    void testOneVsMany(
        size_t queryIdx,
        const uint32_t* candidateIndices,
        size_t candidateCount,
        std::vector<std::pair<uint32_t, uint32_t>>& pairs
    ) {
        // 加载查询AABB，广播到所有通道
        __m256 qMinX = _mm256_set1_ps(minX[queryIdx]);
        __m256 qMinY = _mm256_set1_ps(minY[queryIdx]);
        __m256 qMinZ = _mm256_set1_ps(minZ[queryIdx]);
        __m256 qMaxX = _mm256_set1_ps(maxX[queryIdx]);
        __m256 qMaxY = _mm256_set1_ps(maxY[queryIdx]);
        __m256 qMaxZ = _mm256_set1_ps(maxZ[queryIdx]);

        size_t i = 0;
        for (; i + 8 <= candidateCount; i += 8) {
            // 收集8个候选AABB的数据
            alignas(32) float cMinX[8], cMinY[8], cMinZ[8];
            alignas(32) float cMaxX[8], cMaxY[8], cMaxZ[8];

            for (int j = 0; j < 8; ++j) {
                uint32_t idx = candidateIndices[i + j];
                cMinX[j] = minX[idx]; cMinY[j] = minY[idx]; cMinZ[j] = minZ[idx];
                cMaxX[j] = maxX[idx]; cMaxY[j] = maxY[idx]; cMaxZ[j] = maxZ[idx];
            }

            __m256 candMinX = _mm256_load_ps(cMinX);
            __m256 candMinY = _mm256_load_ps(cMinY);
            __m256 candMinZ = _mm256_load_ps(cMinZ);
            __m256 candMaxX = _mm256_load_ps(cMaxX);
            __m256 candMaxY = _mm256_load_ps(cMaxY);
            __m256 candMaxZ = _mm256_load_ps(cMaxZ);

            // AABB重叠测试
            // overlap = qMin <= candMax && candMin <= qMax（每个轴）
            __m256 overlapX = _mm256_and_ps(
                _mm256_cmp_ps(qMinX, candMaxX, _CMP_LE_OQ),
                _mm256_cmp_ps(candMinX, qMaxX, _CMP_LE_OQ)
            );
            __m256 overlapY = _mm256_and_ps(
                _mm256_cmp_ps(qMinY, candMaxY, _CMP_LE_OQ),
                _mm256_cmp_ps(candMinY, qMaxY, _CMP_LE_OQ)
            );
            __m256 overlapZ = _mm256_and_ps(
                _mm256_cmp_ps(qMinZ, candMaxZ, _CMP_LE_OQ),
                _mm256_cmp_ps(candMinZ, qMaxZ, _CMP_LE_OQ)
            );

            // 三轴都重叠才算碰撞
            __m256 overlap = _mm256_and_ps(
                _mm256_and_ps(overlapX, overlapY), overlapZ);

            // 提取碰撞掩码
            int mask = _mm256_movemask_ps(overlap);

            // 输出碰撞对
            while (mask) {
                int bit = __builtin_ctz(mask);  // 找到最低位的1
                pairs.emplace_back(queryIdx, candidateIndices[i + bit]);
                mask &= mask - 1;  // 清除最低位的1
            }
        }

        // 处理剩余
        for (; i < candidateCount; ++i) {
            uint32_t idx = candidateIndices[i];
            if (minX[queryIdx] <= maxX[idx] && minX[idx] <= maxX[queryIdx] &&
                minY[queryIdx] <= maxY[idx] && minY[idx] <= maxY[queryIdx] &&
                minZ[queryIdx] <= maxZ[idx] && minZ[idx] <= maxZ[queryIdx]) {
                pairs.emplace_back(queryIdx, idx);
            }
        }
    }
};
```

### 第三周：多线程DOD设计

#### 阅读材料
1. Intel TBB文档
2. 《C++ Concurrency in Action》
3. Job System设计模式
4. 《Parallelizing Game Engine》演讲

#### 核心概念

**数据并行vs任务并行**
```cpp
// 数据并行：对同一操作的不同数据并行执行
void updateParticlesDataParallel(ParticleSystem& ps, float dt) {
    const size_t count = ps.size();
    const size_t threadCount = std::thread::hardware_concurrency();
    const size_t chunkSize = (count + threadCount - 1) / threadCount;

    std::vector<std::thread> threads;
    for (size_t t = 0; t < threadCount; ++t) {
        size_t start = t * chunkSize;
        size_t end = std::min(start + chunkSize, count);

        threads.emplace_back([&ps, dt, start, end]() {
            for (size_t i = start; i < end; ++i) {
                ps.updateParticle(i, dt);
            }
        });
    }

    for (auto& t : threads) t.join();
}

// 任务并行：不同类型的任务并行执行
struct PhysicsTaskGraph {
    // 任务定义
    struct Task {
        std::function<void()> work;
        std::vector<size_t> dependencies;
        std::atomic<int> pendingDeps{0};
    };

    std::vector<Task> tasks;

    void execute() {
        std::vector<std::future<void>> futures;
        std::queue<size_t> ready;

        // 找到所有无依赖的任务
        for (size_t i = 0; i < tasks.size(); ++i) {
            if (tasks[i].dependencies.empty()) {
                ready.push(i);
            }
        }

        // 执行任务
        while (!ready.empty()) {
            size_t taskId = ready.front();
            ready.pop();

            tasks[taskId].work();

            // 更新依赖此任务的其他任务
            for (size_t i = 0; i < tasks.size(); ++i) {
                auto& deps = tasks[i].dependencies;
                auto it = std::find(deps.begin(), deps.end(), taskId);
                if (it != deps.end()) {
                    if (--tasks[i].pendingDeps == 0) {
                        ready.push(i);
                    }
                }
            }
        }
    }
};
```

**无锁数据结构**
```cpp
// 无锁碰撞对收集器
class LockFreePairCollector {
private:
    struct alignas(64) PaddedAtomic {
        std::atomic<uint32_t> value{0};
        char padding[64 - sizeof(std::atomic<uint32_t>)];
    };

    std::vector<std::pair<uint32_t, uint32_t>> pairs_;
    PaddedAtomic writeIndex_;
    size_t capacity_;

public:
    explicit LockFreePairCollector(size_t capacity)
        : pairs_(capacity), capacity_(capacity) {}

    bool tryAdd(uint32_t a, uint32_t b) {
        uint32_t idx = writeIndex_.value.fetch_add(1, std::memory_order_relaxed);
        if (idx >= capacity_) {
            return false;  // 容量不足
        }
        pairs_[idx] = {a, b};
        return true;
    }

    void clear() {
        writeIndex_.value.store(0, std::memory_order_relaxed);
    }

    size_t size() const {
        return std::min(
            static_cast<size_t>(writeIndex_.value.load(std::memory_order_relaxed)),
            capacity_
        );
    }

    auto begin() { return pairs_.begin(); }
    auto end() { return pairs_.begin() + size(); }
};
```

### 第四周：物理引擎集成

#### 阅读材料
1. Box2D Lite源码
2. 《Game Physics Pearls》
3. Position Based Dynamics论文
4. 《Physics for Game Developers》

#### 核心概念

**物理模拟流水线**
```
┌─────────────────────────────────────────────────────────┐
│                    物理更新流程                          │
└─────────────────────────────────────────────────────────┘
        │
        ▼
┌───────────────────┐
│ 1. 应用外力      │ ──▶ 并行：每个物体独立
│   (重力、风力)    │
└───────────────────┘
        │
        ▼
┌───────────────────┐
│ 2. 积分速度      │ ──▶ 并行：每个物体独立
│   v += a * dt     │
└───────────────────┘
        │
        ▼
┌───────────────────┐
│ 3. 宽相位碰撞    │ ──▶ 并行：空间分区并行
│   (空间划分)      │
└───────────────────┘
        │
        ▼
┌───────────────────┐
│ 4. 窄相位碰撞    │ ──▶ 并行：每个碰撞对独立
│   (精确检测)      │
└───────────────────┘
        │
        ▼
┌───────────────────┐
│ 5. 约束求解      │ ──▶ 顺序或着色并行
│   (碰撞响应)      │
└───────────────────┘
        │
        ▼
┌───────────────────┐
│ 6. 积分位置      │ ──▶ 并行：每个物体独立
│   p += v * dt     │
└───────────────────┘
```

---

## 源码阅读任务

### 必读项目

1. **Box2D** (https://github.com/erincatto/box2d)
   - 重点文件：`src/collision/`, `src/dynamics/`
   - 学习目标：理解2D物理引擎架构
   - 阅读时间：12小时

2. **Bullet Physics** (https://github.com/bulletphysics/bullet3)
   - 重点文件：`src/BulletCollision/`
   - 学习目标：理解3D碰撞检测实现
   - 阅读时间：10小时

3. **ReactPhysics3D** (https://github.com/DanielChappworthy/reactphysics3d)
   - 重点：现代C++物理引擎设计
   - 阅读时间：6小时

---

## 实践项目：高性能物理引擎核心

### 项目概述
构建一个高性能的2D/3D物理引擎核心，实现完整的碰撞检测和响应系统，支持多线程并行处理。

### 完整代码实现

#### 1. 数学库 (physics/math/math_types.hpp)

```cpp
#pragma once

#include <cmath>
#include <algorithm>

#ifdef __AVX__
#include <immintrin.h>
#endif

namespace physics {

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3 cross(const Vec3& v) const {
        return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
    }
    float lengthSq() const { return dot(*this); }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const {
        float len = length();
        return len > 0 ? *this / len : Vec3{};
    }

    static Vec3 min(const Vec3& a, const Vec3& b) {
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    }
    static Vec3 max(const Vec3& a, const Vec3& b) {
        return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
    }
};

struct AABB {
    Vec3 min, max;

    AABB() = default;
    AABB(const Vec3& min, const Vec3& max) : min(min), max(max) {}

    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 extents() const { return (max - min) * 0.5f; }

    bool overlaps(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    AABB expanded(float margin) const {
        return {min - Vec3{margin, margin, margin},
                max + Vec3{margin, margin, margin}};
    }

    static AABB fromCenterExtents(const Vec3& center, const Vec3& extents) {
        return {center - extents, center + extents};
    }
};

struct Transform {
    Vec3 position;
    // 简化：只用四元数的旋转，这里用欧拉角简化
    Vec3 rotation;
    Vec3 scale{1, 1, 1};

    Vec3 transformPoint(const Vec3& p) const {
        // 简化实现
        return position + Vec3{p.x * scale.x, p.y * scale.y, p.z * scale.z};
    }
};

} // namespace physics
```

#### 2. 刚体系统 (physics/dynamics/rigid_body_system.hpp)

```cpp
#pragma once

#include "../math/math_types.hpp"
#include <vector>
#include <cstdint>

namespace physics {

// 刚体类型
enum class BodyType : uint8_t {
    Static,     // 静态物体，不移动
    Dynamic,    // 动态物体，受力影响
    Kinematic   // 运动学物体，手动控制移动
};

// 刚体系统 - DOD风格
class RigidBodySystem {
public:
    // 位置与旋转
    std::vector<float> posX, posY, posZ;
    std::vector<float> rotX, rotY, rotZ, rotW;  // 四元数

    // 线速度与角速度
    std::vector<float> velX, velY, velZ;
    std::vector<float> angVelX, angVelY, angVelZ;

    // 力与扭矩累加器
    std::vector<float> forceX, forceY, forceZ;
    std::vector<float> torqueX, torqueY, torqueZ;

    // 质量属性
    std::vector<float> invMass;      // 逆质量（0表示无限质量/静态）
    std::vector<float> invInertiaX, invInertiaY, invInertiaZ;  // 逆惯性张量对角线

    // AABB（用于碰撞检测）
    std::vector<float> aabbMinX, aabbMinY, aabbMinZ;
    std::vector<float> aabbMaxX, aabbMaxY, aabbMaxZ;

    // 材质属性
    std::vector<float> restitution;  // 弹性系数
    std::vector<float> friction;     // 摩擦系数

    // 类型与标志
    std::vector<BodyType> bodyTypes;
    std::vector<uint32_t> flags;

    // 碰撞器索引（指向碰撞器系统）
    std::vector<uint32_t> colliderIndex;

    // 用户数据
    std::vector<void*> userData;

    size_t count() const { return posX.size(); }

    // 创建刚体
    uint32_t create(const Vec3& position, BodyType type = BodyType::Dynamic) {
        uint32_t id = static_cast<uint32_t>(count());

        posX.push_back(position.x);
        posY.push_back(position.y);
        posZ.push_back(position.z);
        rotX.push_back(0); rotY.push_back(0); rotZ.push_back(0); rotW.push_back(1);

        velX.push_back(0); velY.push_back(0); velZ.push_back(0);
        angVelX.push_back(0); angVelY.push_back(0); angVelZ.push_back(0);

        forceX.push_back(0); forceY.push_back(0); forceZ.push_back(0);
        torqueX.push_back(0); torqueY.push_back(0); torqueZ.push_back(0);

        float im = (type == BodyType::Static) ? 0.0f : 1.0f;
        invMass.push_back(im);
        invInertiaX.push_back(im);
        invInertiaY.push_back(im);
        invInertiaZ.push_back(im);

        aabbMinX.push_back(-0.5f); aabbMinY.push_back(-0.5f); aabbMinZ.push_back(-0.5f);
        aabbMaxX.push_back(0.5f); aabbMaxY.push_back(0.5f); aabbMaxZ.push_back(0.5f);

        restitution.push_back(0.3f);
        friction.push_back(0.5f);

        bodyTypes.push_back(type);
        flags.push_back(0);
        colliderIndex.push_back(UINT32_MAX);
        userData.push_back(nullptr);

        return id;
    }

    // 设置质量
    void setMass(uint32_t id, float mass) {
        if (mass <= 0 || bodyTypes[id] == BodyType::Static) {
            invMass[id] = 0;
        } else {
            invMass[id] = 1.0f / mass;
        }
    }

    // 应用力
    void applyForce(uint32_t id, const Vec3& force) {
        forceX[id] += force.x;
        forceY[id] += force.y;
        forceZ[id] += force.z;
    }

    // 应用冲量
    void applyImpulse(uint32_t id, const Vec3& impulse) {
        velX[id] += impulse.x * invMass[id];
        velY[id] += impulse.y * invMass[id];
        velZ[id] += impulse.z * invMass[id];
    }

    // 清除力累加器
    void clearForces() {
        std::fill(forceX.begin(), forceX.end(), 0.0f);
        std::fill(forceY.begin(), forceY.end(), 0.0f);
        std::fill(forceZ.begin(), forceZ.end(), 0.0f);
        std::fill(torqueX.begin(), torqueX.end(), 0.0f);
        std::fill(torqueY.begin(), torqueY.end(), 0.0f);
        std::fill(torqueZ.begin(), torqueZ.end(), 0.0f);
    }

    // 积分 - 半隐式欧拉
    void integrate(float dt) {
        const size_t n = count();

        // 并行友好的循环
        for (size_t i = 0; i < n; ++i) {
            if (invMass[i] == 0) continue;  // 跳过静态物体

            // 更新速度：v += (F/m) * dt
            velX[i] += forceX[i] * invMass[i] * dt;
            velY[i] += forceY[i] * invMass[i] * dt;
            velZ[i] += forceZ[i] * invMass[i] * dt;

            // 更新位置：p += v * dt
            posX[i] += velX[i] * dt;
            posY[i] += velY[i] * dt;
            posZ[i] += velZ[i] * dt;
        }
    }

#ifdef __AVX__
    void integrateAVX(float dt) {
        const size_t n = count();
        const __m256 vdt = _mm256_set1_ps(dt);

        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            // 加载逆质量
            __m256 im = _mm256_loadu_ps(&invMass[i]);

            // 加载力
            __m256 fx = _mm256_loadu_ps(&forceX[i]);
            __m256 fy = _mm256_loadu_ps(&forceY[i]);
            __m256 fz = _mm256_loadu_ps(&forceZ[i]);

            // 加载速度
            __m256 vx = _mm256_loadu_ps(&velX[i]);
            __m256 vy = _mm256_loadu_ps(&velY[i]);
            __m256 vz = _mm256_loadu_ps(&velZ[i]);

            // 加载位置
            __m256 px = _mm256_loadu_ps(&posX[i]);
            __m256 py = _mm256_loadu_ps(&posY[i]);
            __m256 pz = _mm256_loadu_ps(&posZ[i]);

            // 计算加速度：a = F * invMass
            __m256 ax = _mm256_mul_ps(fx, im);
            __m256 ay = _mm256_mul_ps(fy, im);
            __m256 az = _mm256_mul_ps(fz, im);

            // 更新速度：v += a * dt
            vx = _mm256_fmadd_ps(ax, vdt, vx);
            vy = _mm256_fmadd_ps(ay, vdt, vy);
            vz = _mm256_fmadd_ps(az, vdt, vz);

            // 更新位置：p += v * dt
            px = _mm256_fmadd_ps(vx, vdt, px);
            py = _mm256_fmadd_ps(vy, vdt, py);
            pz = _mm256_fmadd_ps(vz, vdt, pz);

            // 存储结果
            _mm256_storeu_ps(&velX[i], vx);
            _mm256_storeu_ps(&velY[i], vy);
            _mm256_storeu_ps(&velZ[i], vz);
            _mm256_storeu_ps(&posX[i], px);
            _mm256_storeu_ps(&posY[i], py);
            _mm256_storeu_ps(&posZ[i], pz);
        }

        // 处理剩余
        for (; i < n; ++i) {
            if (invMass[i] == 0) continue;
            velX[i] += forceX[i] * invMass[i] * dt;
            velY[i] += forceY[i] * invMass[i] * dt;
            velZ[i] += forceZ[i] * invMass[i] * dt;
            posX[i] += velX[i] * dt;
            posY[i] += velY[i] * dt;
            posZ[i] += velZ[i] * dt;
        }
    }
#endif
};

} // namespace physics
```

#### 3. 宽相位碰撞检测 (physics/collision/broad_phase.hpp)

```cpp
#pragma once

#include "../math/math_types.hpp"
#include <vector>
#include <algorithm>
#include <thread>
#include <future>

namespace physics {

// 碰撞对
struct CollisionPair {
    uint32_t a, b;
};

// 均匀网格宽相位
class UniformGridBroadPhase {
private:
    float cellSize_;
    int gridSizeX_, gridSizeY_, gridSizeZ_;
    Vec3 origin_;

    // 网格数据
    std::vector<uint32_t> cellStart_;
    std::vector<uint32_t> cellCount_;
    std::vector<uint32_t> sortedBodies_;
    std::vector<int> bodyCells_;  // 每个物体所在的单元格

    int getCellIndex(float x, float y, float z) const {
        int cx = static_cast<int>((x - origin_.x) / cellSize_);
        int cy = static_cast<int>((y - origin_.y) / cellSize_);
        int cz = static_cast<int>((z - origin_.z) / cellSize_);

        cx = std::clamp(cx, 0, gridSizeX_ - 1);
        cy = std::clamp(cy, 0, gridSizeY_ - 1);
        cz = std::clamp(cz, 0, gridSizeZ_ - 1);

        return cx + cy * gridSizeX_ + cz * gridSizeX_ * gridSizeY_;
    }

public:
    UniformGridBroadPhase(
        const Vec3& worldMin,
        const Vec3& worldMax,
        float cellSize)
        : cellSize_(cellSize), origin_(worldMin) {

        Vec3 size = worldMax - worldMin;
        gridSizeX_ = std::max(1, static_cast<int>(std::ceil(size.x / cellSize)));
        gridSizeY_ = std::max(1, static_cast<int>(std::ceil(size.y / cellSize)));
        gridSizeZ_ = std::max(1, static_cast<int>(std::ceil(size.z / cellSize)));

        size_t totalCells = gridSizeX_ * gridSizeY_ * gridSizeZ_;
        cellStart_.resize(totalCells);
        cellCount_.resize(totalCells);
    }

    void update(const float* posX, const float* posY, const float* posZ,
                size_t bodyCount) {
        const size_t cellCount = cellStart_.size();

        // 重置
        std::fill(cellCount_.begin(), cellCount_.end(), 0);
        bodyCells_.resize(bodyCount);

        // 计算每个物体所在的单元格
        for (size_t i = 0; i < bodyCount; ++i) {
            int cell = getCellIndex(posX[i], posY[i], posZ[i]);
            bodyCells_[i] = cell;
            ++cellCount_[cell];
        }

        // 计算前缀和
        uint32_t total = 0;
        for (size_t i = 0; i < cellCount; ++i) {
            cellStart_[i] = total;
            total += cellCount_[i];
        }

        // 排序物体索引
        sortedBodies_.resize(bodyCount);
        std::vector<uint32_t> currentIndex = cellStart_;

        for (size_t i = 0; i < bodyCount; ++i) {
            int cell = bodyCells_[i];
            sortedBodies_[currentIndex[cell]++] = static_cast<uint32_t>(i);
        }
    }

    std::vector<CollisionPair> findPairs(
        const float* aabbMinX, const float* aabbMinY, const float* aabbMinZ,
        const float* aabbMaxX, const float* aabbMaxY, const float* aabbMaxZ,
        size_t bodyCount) {

        std::vector<CollisionPair> pairs;
        pairs.reserve(bodyCount * 4);  // 预估

        // 遍历每个单元格
        for (int cz = 0; cz < gridSizeZ_; ++cz) {
            for (int cy = 0; cy < gridSizeY_; ++cy) {
                for (int cx = 0; cx < gridSizeX_; ++cx) {
                    int cellIdx = cx + cy * gridSizeX_ +
                                  cz * gridSizeX_ * gridSizeY_;

                    uint32_t start = cellStart_[cellIdx];
                    uint32_t count = cellCount_[cellIdx];

                    if (count == 0) continue;

                    // 单元格内部碰撞
                    for (uint32_t i = 0; i < count; ++i) {
                        uint32_t bodyA = sortedBodies_[start + i];

                        // 与同单元格其他物体
                        for (uint32_t j = i + 1; j < count; ++j) {
                            uint32_t bodyB = sortedBodies_[start + j];

                            if (aabbOverlap(bodyA, bodyB,
                                aabbMinX, aabbMinY, aabbMinZ,
                                aabbMaxX, aabbMaxY, aabbMaxZ)) {
                                pairs.push_back({bodyA, bodyB});
                            }
                        }

                        // 与相邻单元格的物体（只检查"前方"邻居避免重复）
                        static const int neighborOffsets[][3] = {
                            {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
                            {1, 1, 0}, {1, 0, 1}, {0, 1, 1},
                            {1, 1, 1}, {1, -1, 0}, {1, 0, -1},
                            {0, 1, -1}, {1, 1, -1}, {1, -1, 1},
                            {-1, 1, 1}
                        };

                        for (const auto& offset : neighborOffsets) {
                            int nx = cx + offset[0];
                            int ny = cy + offset[1];
                            int nz = cz + offset[2];

                            if (nx < 0 || nx >= gridSizeX_ ||
                                ny < 0 || ny >= gridSizeY_ ||
                                nz < 0 || nz >= gridSizeZ_) continue;

                            int neighborIdx = nx + ny * gridSizeX_ +
                                             nz * gridSizeX_ * gridSizeY_;

                            uint32_t nStart = cellStart_[neighborIdx];
                            uint32_t nCount = cellCount_[neighborIdx];

                            for (uint32_t k = 0; k < nCount; ++k) {
                                uint32_t bodyB = sortedBodies_[nStart + k];

                                if (aabbOverlap(bodyA, bodyB,
                                    aabbMinX, aabbMinY, aabbMinZ,
                                    aabbMaxX, aabbMaxY, aabbMaxZ)) {
                                    pairs.push_back({bodyA, bodyB});
                                }
                            }
                        }
                    }
                }
            }
        }

        return pairs;
    }

    // 并行版本
    std::vector<CollisionPair> findPairsParallel(
        const float* aabbMinX, const float* aabbMinY, const float* aabbMinZ,
        const float* aabbMaxX, const float* aabbMaxY, const float* aabbMaxZ,
        size_t bodyCount) {

        const size_t threadCount = std::thread::hardware_concurrency();
        const size_t cellsPerThread = cellStart_.size() / threadCount;

        std::vector<std::future<std::vector<CollisionPair>>> futures;

        for (size_t t = 0; t < threadCount; ++t) {
            size_t startCell = t * cellsPerThread;
            size_t endCell = (t == threadCount - 1) ?
                             cellStart_.size() : (t + 1) * cellsPerThread;

            futures.push_back(std::async(std::launch::async, [=]() {
                return findPairsInRange(startCell, endCell,
                    aabbMinX, aabbMinY, aabbMinZ,
                    aabbMaxX, aabbMaxY, aabbMaxZ);
            }));
        }

        // 合并结果
        std::vector<CollisionPair> allPairs;
        for (auto& f : futures) {
            auto pairs = f.get();
            allPairs.insert(allPairs.end(), pairs.begin(), pairs.end());
        }

        return allPairs;
    }

private:
    bool aabbOverlap(uint32_t a, uint32_t b,
        const float* minX, const float* minY, const float* minZ,
        const float* maxX, const float* maxY, const float* maxZ) const {
        return minX[a] <= maxX[b] && maxX[a] >= minX[b] &&
               minY[a] <= maxY[b] && maxY[a] >= minY[b] &&
               minZ[a] <= maxZ[b] && maxZ[a] >= minZ[b];
    }

    std::vector<CollisionPair> findPairsInRange(
        size_t startCell, size_t endCell,
        const float* aabbMinX, const float* aabbMinY, const float* aabbMinZ,
        const float* aabbMaxX, const float* aabbMaxY, const float* aabbMaxZ) {

        std::vector<CollisionPair> pairs;

        for (size_t cellIdx = startCell; cellIdx < endCell; ++cellIdx) {
            uint32_t start = cellStart_[cellIdx];
            uint32_t count = cellCount_[cellIdx];

            for (uint32_t i = 0; i < count; ++i) {
                uint32_t bodyA = sortedBodies_[start + i];

                for (uint32_t j = i + 1; j < count; ++j) {
                    uint32_t bodyB = sortedBodies_[start + j];

                    if (aabbOverlap(bodyA, bodyB,
                        aabbMinX, aabbMinY, aabbMinZ,
                        aabbMaxX, aabbMaxY, aabbMaxZ)) {
                        pairs.push_back({bodyA, bodyB});
                    }
                }
            }
        }

        return pairs;
    }
};

} // namespace physics
```

#### 4. 窄相位与碰撞求解 (physics/collision/narrow_phase.hpp)

```cpp
#pragma once

#include "../math/math_types.hpp"
#include "broad_phase.hpp"
#include <vector>

namespace physics {

// 接触点信息
struct ContactPoint {
    Vec3 position;       // 接触点世界坐标
    Vec3 normal;         // 从A指向B的法线
    float penetration;   // 穿透深度
    uint32_t bodyA, bodyB;
};

// 碰撞流形（一对物体的所有接触点）
struct ContactManifold {
    uint32_t bodyA, bodyB;
    std::vector<ContactPoint> contacts;
    float restitution;
    float friction;
};

// 球体vs球体碰撞检测
class SphereCollider {
public:
    static bool testSphereSphere(
        const Vec3& posA, float radiusA,
        const Vec3& posB, float radiusB,
        ContactPoint& contact) {

        Vec3 d = posB - posA;
        float distSq = d.lengthSq();
        float radiusSum = radiusA + radiusB;

        if (distSq >= radiusSum * radiusSum) {
            return false;
        }

        float dist = std::sqrt(distSq);

        contact.normal = dist > 0.0001f ? d / dist : Vec3{0, 1, 0};
        contact.penetration = radiusSum - dist;
        contact.position = posA + contact.normal * (radiusA - contact.penetration * 0.5f);

        return true;
    }
};

// 球体vs平面碰撞检测
class PlaneCollider {
public:
    static bool testSpherePlane(
        const Vec3& spherePos, float radius,
        const Vec3& planeNormal, float planeOffset,
        ContactPoint& contact) {

        float dist = spherePos.dot(planeNormal) - planeOffset;

        if (dist >= radius) {
            return false;
        }

        contact.normal = planeNormal;
        contact.penetration = radius - dist;
        contact.position = spherePos - planeNormal * (dist + contact.penetration * 0.5f);

        return true;
    }
};

// 窄相位碰撞检测系统
class NarrowPhaseSystem {
public:
    // 球体数据（DOD风格）
    std::vector<float> spherePosX, spherePosY, spherePosZ;
    std::vector<float> sphereRadius;
    std::vector<uint32_t> sphereBodyIndex;  // 对应的刚体索引

    // 平面数据
    std::vector<float> planeNormalX, planeNormalY, planeNormalZ;
    std::vector<float> planeOffset;

    std::vector<ContactManifold> detectCollisions(
        const std::vector<CollisionPair>& broadPhasePairs,
        const float* bodyPosX, const float* bodyPosY, const float* bodyPosZ,
        const float* bodyRestitution, const float* bodyFriction) {

        std::vector<ContactManifold> manifolds;

        for (const auto& pair : broadPhasePairs) {
            ContactManifold manifold;
            manifold.bodyA = pair.a;
            manifold.bodyB = pair.b;

            // 简化：假设所有物体都是球体
            // 实际应根据碰撞器类型选择检测算法

            Vec3 posA{bodyPosX[pair.a], bodyPosY[pair.a], bodyPosZ[pair.a]};
            Vec3 posB{bodyPosX[pair.b], bodyPosY[pair.b], bodyPosZ[pair.b]};

            // 查找对应的球体碰撞器
            float radiusA = 0.5f;  // 默认半径
            float radiusB = 0.5f;

            for (size_t i = 0; i < sphereBodyIndex.size(); ++i) {
                if (sphereBodyIndex[i] == pair.a) radiusA = sphereRadius[i];
                if (sphereBodyIndex[i] == pair.b) radiusB = sphereRadius[i];
            }

            ContactPoint contact;
            if (SphereCollider::testSphereSphere(posA, radiusA, posB, radiusB, contact)) {
                contact.bodyA = pair.a;
                contact.bodyB = pair.b;
                manifold.contacts.push_back(contact);

                // 混合材质属性
                manifold.restitution = std::min(
                    bodyRestitution[pair.a], bodyRestitution[pair.b]);
                manifold.friction = std::sqrt(
                    bodyFriction[pair.a] * bodyFriction[pair.b]);

                manifolds.push_back(manifold);
            }
        }

        return manifolds;
    }
};

// 冲量求解器
class ImpulseSolver {
public:
    static void resolveCollision(
        const ContactManifold& manifold,
        float* velX, float* velY, float* velZ,
        float* angVelX, float* angVelY, float* angVelZ,
        const float* invMass,
        const float* posX, const float* posY, const float* posZ) {

        for (const auto& contact : manifold.contacts) {
            uint32_t a = contact.bodyA;
            uint32_t b = contact.bodyB;

            // 相对速度
            Vec3 velA{velX[a], velY[a], velZ[a]};
            Vec3 velB{velX[b], velY[b], velZ[b]};
            Vec3 relVel = velB - velA;

            // 法向相对速度
            float velAlongNormal = relVel.dot(contact.normal);

            // 如果物体正在分离，不处理
            if (velAlongNormal > 0) continue;

            // 计算冲量大小
            float e = manifold.restitution;
            float j = -(1 + e) * velAlongNormal;
            j /= invMass[a] + invMass[b];

            // 应用冲量
            Vec3 impulse = contact.normal * j;

            velX[a] -= impulse.x * invMass[a];
            velY[a] -= impulse.y * invMass[a];
            velZ[a] -= impulse.z * invMass[a];

            velX[b] += impulse.x * invMass[b];
            velY[b] += impulse.y * invMass[b];
            velZ[b] += impulse.z * invMass[b];

            // 位置修正（防止穿透）
            const float percent = 0.2f;
            const float slop = 0.01f;
            float correctionMag = std::max(contact.penetration - slop, 0.0f) /
                                  (invMass[a] + invMass[b]) * percent;
            Vec3 correction = contact.normal * correctionMag;

            // 注意：实际实现中，位置修正应该在专门的步骤中进行
            // 这里为简化，直接通过速度修正来处理

            // 摩擦力
            Vec3 tangent = relVel - contact.normal * velAlongNormal;
            if (tangent.lengthSq() > 0.0001f) {
                tangent = tangent.normalized();

                float jt = -relVel.dot(tangent);
                jt /= invMass[a] + invMass[b];

                // 库仑摩擦
                Vec3 frictionImpulse;
                if (std::abs(jt) < j * manifold.friction) {
                    frictionImpulse = tangent * jt;
                } else {
                    frictionImpulse = tangent * (-j * manifold.friction);
                }

                velX[a] -= frictionImpulse.x * invMass[a];
                velY[a] -= frictionImpulse.y * invMass[a];
                velZ[a] -= frictionImpulse.z * invMass[a];

                velX[b] += frictionImpulse.x * invMass[b];
                velY[b] += frictionImpulse.y * invMass[b];
                velZ[b] += frictionImpulse.z * invMass[b];
            }
        }
    }
};

} // namespace physics
```

#### 5. 物理世界 (physics/world.hpp)

```cpp
#pragma once

#include "dynamics/rigid_body_system.hpp"
#include "collision/broad_phase.hpp"
#include "collision/narrow_phase.hpp"
#include <functional>

namespace physics {

struct WorldConfig {
    Vec3 gravity{0, -9.8f, 0};
    Vec3 worldMin{-100, -100, -100};
    Vec3 worldMax{100, 100, 100};
    float broadPhaseCellSize = 2.0f;
    int solverIterations = 4;
    float fixedDeltaTime = 1.0f / 60.0f;
};

class PhysicsWorld {
private:
    WorldConfig config_;
    RigidBodySystem bodies_;
    UniformGridBroadPhase broadPhase_;
    NarrowPhaseSystem narrowPhase_;

    float accumulator_ = 0;

    // 回调
    std::function<void(uint32_t, uint32_t, const ContactPoint&)> onCollision_;

public:
    explicit PhysicsWorld(const WorldConfig& config = {})
        : config_(config),
          broadPhase_(config.worldMin, config.worldMax, config.broadPhaseCellSize) {}

    // 创建刚体
    uint32_t createBody(const Vec3& position, BodyType type = BodyType::Dynamic) {
        return bodies_.create(position, type);
    }

    // 设置属性
    void setMass(uint32_t body, float mass) {
        bodies_.setMass(body, mass);
    }

    void setVelocity(uint32_t body, const Vec3& velocity) {
        bodies_.velX[body] = velocity.x;
        bodies_.velY[body] = velocity.y;
        bodies_.velZ[body] = velocity.z;
    }

    void setRestitution(uint32_t body, float restitution) {
        bodies_.restitution[body] = restitution;
    }

    void setFriction(uint32_t body, float friction) {
        bodies_.friction[body] = friction;
    }

    // 添加球体碰撞器
    void addSphereCollider(uint32_t body, float radius) {
        narrowPhase_.spherePosX.push_back(bodies_.posX[body]);
        narrowPhase_.spherePosY.push_back(bodies_.posY[body]);
        narrowPhase_.spherePosZ.push_back(bodies_.posZ[body]);
        narrowPhase_.sphereRadius.push_back(radius);
        narrowPhase_.sphereBodyIndex.push_back(body);

        // 更新AABB
        bodies_.aabbMinX[body] = -radius;
        bodies_.aabbMinY[body] = -radius;
        bodies_.aabbMinZ[body] = -radius;
        bodies_.aabbMaxX[body] = radius;
        bodies_.aabbMaxY[body] = radius;
        bodies_.aabbMaxZ[body] = radius;
    }

    // 应用力
    void applyForce(uint32_t body, const Vec3& force) {
        bodies_.applyForce(body, force);
    }

    // 应用冲量
    void applyImpulse(uint32_t body, const Vec3& impulse) {
        bodies_.applyImpulse(body, impulse);
    }

    // 设置碰撞回调
    void setCollisionCallback(
        std::function<void(uint32_t, uint32_t, const ContactPoint&)> callback) {
        onCollision_ = callback;
    }

    // 主更新函数
    void update(float deltaTime) {
        accumulator_ += deltaTime;

        while (accumulator_ >= config_.fixedDeltaTime) {
            step(config_.fixedDeltaTime);
            accumulator_ -= config_.fixedDeltaTime;
        }
    }

    // 单步物理模拟
    void step(float dt) {
        const size_t bodyCount = bodies_.count();
        if (bodyCount == 0) return;

        // 1. 应用重力
        for (size_t i = 0; i < bodyCount; ++i) {
            if (bodies_.invMass[i] > 0) {
                bodies_.forceY[i] += config_.gravity.y / bodies_.invMass[i];
            }
        }

        // 2. 积分速度和位置
#ifdef __AVX__
        bodies_.integrateAVX(dt);
#else
        bodies_.integrate(dt);
#endif

        // 3. 更新AABB（世界坐标）
        updateAABBs();

        // 4. 宽相位碰撞检测
        broadPhase_.update(
            bodies_.posX.data(), bodies_.posY.data(), bodies_.posZ.data(),
            bodyCount);

        auto pairs = broadPhase_.findPairsParallel(
            bodies_.aabbMinX.data(), bodies_.aabbMinY.data(), bodies_.aabbMinZ.data(),
            bodies_.aabbMaxX.data(), bodies_.aabbMaxY.data(), bodies_.aabbMaxZ.data(),
            bodyCount);

        // 5. 窄相位碰撞检测
        auto manifolds = narrowPhase_.detectCollisions(
            pairs,
            bodies_.posX.data(), bodies_.posY.data(), bodies_.posZ.data(),
            bodies_.restitution.data(), bodies_.friction.data());

        // 6. 碰撞求解
        for (int iter = 0; iter < config_.solverIterations; ++iter) {
            for (const auto& manifold : manifolds) {
                ImpulseSolver::resolveCollision(
                    manifold,
                    bodies_.velX.data(), bodies_.velY.data(), bodies_.velZ.data(),
                    bodies_.angVelX.data(), bodies_.angVelY.data(), bodies_.angVelZ.data(),
                    bodies_.invMass.data(),
                    bodies_.posX.data(), bodies_.posY.data(), bodies_.posZ.data());
            }
        }

        // 7. 调用碰撞回调
        if (onCollision_) {
            for (const auto& manifold : manifolds) {
                for (const auto& contact : manifold.contacts) {
                    onCollision_(manifold.bodyA, manifold.bodyB, contact);
                }
            }
        }

        // 8. 清除力
        bodies_.clearForces();
    }

    // 获取位置
    Vec3 getPosition(uint32_t body) const {
        return {bodies_.posX[body], bodies_.posY[body], bodies_.posZ[body]};
    }

    Vec3 getVelocity(uint32_t body) const {
        return {bodies_.velX[body], bodies_.velY[body], bodies_.velZ[body]};
    }

    size_t bodyCount() const { return bodies_.count(); }

    // 直接访问刚体系统（用于批量操作）
    RigidBodySystem& bodies() { return bodies_; }
    const RigidBodySystem& bodies() const { return bodies_; }

private:
    void updateAABBs() {
        const size_t n = bodies_.count();

        // 更新球体位置
        for (size_t i = 0; i < narrowPhase_.sphereBodyIndex.size(); ++i) {
            uint32_t body = narrowPhase_.sphereBodyIndex[i];
            narrowPhase_.spherePosX[i] = bodies_.posX[body];
            narrowPhase_.spherePosY[i] = bodies_.posY[body];
            narrowPhase_.spherePosZ[i] = bodies_.posZ[body];
        }

        // 更新世界空间AABB
        for (size_t i = 0; i < n; ++i) {
            // 找到对应的球体半径
            float radius = 0.5f;
            for (size_t j = 0; j < narrowPhase_.sphereBodyIndex.size(); ++j) {
                if (narrowPhase_.sphereBodyIndex[j] == i) {
                    radius = narrowPhase_.sphereRadius[j];
                    break;
                }
            }

            bodies_.aabbMinX[i] = bodies_.posX[i] - radius;
            bodies_.aabbMinY[i] = bodies_.posY[i] - radius;
            bodies_.aabbMinZ[i] = bodies_.posZ[i] - radius;
            bodies_.aabbMaxX[i] = bodies_.posX[i] + radius;
            bodies_.aabbMaxY[i] = bodies_.posY[i] + radius;
            bodies_.aabbMaxZ[i] = bodies_.posZ[i] + radius;
        }
    }
};

} // namespace physics
```

#### 6. 使用示例与基准测试 (main.cpp)

```cpp
#include "physics/world.hpp"
#include <iostream>
#include <chrono>
#include <random>

using namespace physics;

void benchmark() {
    constexpr size_t BODY_COUNTS[] = {1000, 5000, 10000, 50000};
    constexpr int FRAMES = 100;
    constexpr float DT = 1.0f / 60.0f;

    std::cout << "Physics Engine Benchmark\n";
    std::cout << "========================\n\n";

    for (size_t bodyCount : BODY_COUNTS) {
        WorldConfig config;
        config.worldMin = {-50, -50, -50};
        config.worldMax = {50, 50, 50};
        config.broadPhaseCellSize = 2.0f;

        PhysicsWorld world(config);

        // 创建随机分布的球体
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> posDist(-40, 40);
        std::uniform_real_distribution<float> velDist(-5, 5);
        std::uniform_real_distribution<float> radiusDist(0.3f, 0.8f);

        for (size_t i = 0; i < bodyCount; ++i) {
            Vec3 pos{posDist(rng), posDist(rng), posDist(rng)};
            uint32_t body = world.createBody(pos, BodyType::Dynamic);

            world.setVelocity(body, {velDist(rng), velDist(rng), velDist(rng)});
            world.addSphereCollider(body, radiusDist(rng));
            world.setMass(body, 1.0f);
            world.setRestitution(body, 0.8f);
        }

        // 预热
        for (int i = 0; i < 10; ++i) {
            world.step(DT);
        }

        // 基准测试
        auto start = std::chrono::high_resolution_clock::now();

        for (int frame = 0; frame < FRAMES; ++frame) {
            world.step(DT);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - start);

        double avgFrameTime = duration.count() / FRAMES;
        double fps = 1000.0 / avgFrameTime;

        std::cout << "Bodies: " << bodyCount << "\n";
        std::cout << "  Avg frame time: " << avgFrameTime << " ms\n";
        std::cout << "  FPS: " << fps << "\n\n";
    }
}

void demo() {
    WorldConfig config;
    PhysicsWorld world(config);

    // 创建地面（静态）
    uint32_t ground = world.createBody({0, -5, 0}, BodyType::Static);
    // 地面使用大的AABB代替平面

    // 创建掉落的球体
    for (int i = 0; i < 10; ++i) {
        Vec3 pos{static_cast<float>(i - 5) * 2.0f, 10.0f + i * 2.0f, 0};
        uint32_t ball = world.createBody(pos, BodyType::Dynamic);
        world.addSphereCollider(ball, 0.5f);
        world.setMass(ball, 1.0f);
        world.setRestitution(ball, 0.8f);
    }

    // 设置碰撞回调
    world.setCollisionCallback([](uint32_t a, uint32_t b, const ContactPoint& cp) {
        std::cout << "Collision: " << a << " <-> " << b
                  << " at (" << cp.position.x << ", "
                  << cp.position.y << ", " << cp.position.z << ")\n";
    });

    // 模拟
    std::cout << "Running simulation...\n";
    for (int frame = 0; frame < 300; ++frame) {
        world.step(1.0f / 60.0f);

        if (frame % 60 == 0) {
            std::cout << "Frame " << frame << ":\n";
            for (uint32_t i = 0; i < std::min(static_cast<size_t>(3),
                                               world.bodyCount()); ++i) {
                Vec3 pos = world.getPosition(i);
                std::cout << "  Body " << i << ": ("
                          << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
            }
        }
    }
}

int main() {
    std::cout << "Physics Engine Demo\n";
    std::cout << "===================\n\n";

    demo();
    std::cout << "\n";
    benchmark();

    return 0;
}
```

---

## 检验标准

### 知识检验
1. [ ] 能够解释不同空间划分方法的优缺点
2. [ ] 理解宽相位和窄相位碰撞检测的作用
3. [ ] 掌握冲量求解的数学原理
4. [ ] 理解多线程DOD设计的挑战与解决方案
5. [ ] 能够分析物理引擎的性能瓶颈

### 实践检验
1. [ ] 完成均匀网格宽相位实现
2. [ ] 实现球体碰撞检测与响应
3. [ ] 物理引擎能够正确模拟碰撞和弹跳
4. [ ] 多线程版本正确且有性能提升
5. [ ] 10000个物体达到60FPS

### 代码质量
1. [ ] 数据布局符合DOD原则
2. [ ] 无数据竞争和死锁
3. [ ] 代码可扩展，易于添加新碰撞器类型
4. [ ] 有完整的基准测试

---

## 输出物清单

1. **学习笔记**
   - [ ] 空间数据结构对比分析
   - [ ] 碰撞检测算法总结
   - [ ] 源码阅读笔记

2. **代码产出**
   - [ ] 高性能物理引擎核心
   - [ ] 多种碰撞器支持
   - [ ] 基准测试套件

3. **分析报告**
   - [ ] 性能分析报告
   - [ ] 多线程扩展性分析
   - [ ] 与商业引擎对比

---

## 时间分配表

| 周次 | 理论学习 | 源码阅读 | 项目实践 | 总计 |
|------|----------|----------|----------|------|
| Week 1 | 15h | 10h | 10h | 35h |
| Week 2 | 12h | 8h | 15h | 35h |
| Week 3 | 10h | 6h | 19h | 35h |
| Week 4 | 8h | 4h | 23h | 35h |
| **总计** | **45h** | **28h** | **67h** | **140h** |

---

## 下月预告

**Month 53: ECS架构（Entity-Component-System）**

下个月将学习游戏开发中最流行的DOD架构模式：
- ECS核心概念与设计原则
- 组件存储策略（Archetype vs Sparse Set）
- 系统调度与并行执行
- 实践项目：构建高性能ECS框架

建议提前：
1. 了解Unity DOTS/ECS概念
2. 阅读EnTT库文档
3. 复习位操作和稀疏集合
