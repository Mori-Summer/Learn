# Month 42: 单元测试——Google Test与Catch2实践

## 本月主题概述

本月深入学习C++单元测试，重点掌握两大主流测试框架：Google Test和Catch2。学习测试驱动开发（TDD）的实践方法，掌握Mock对象、参数化测试、测试夹具等高级技术。

**学习目标**：
- 掌握Google Test和Catch2的使用方法
- 理解单元测试的设计原则（FIRST原则）
- 学会使用Mock进行依赖隔离
- 能够实现代码覆盖率分析

---

## 理论学习内容

### 第一周：Google Test基础

**学习目标**：掌握Google Test的核心功能

**阅读材料**：
- [ ] Google Test官方文档
- [ ] 《Google软件测试之道》相关章节
- [ ] gtest-primer

**核心概念**：

```cpp
// ==========================================
// 基本测试结构
// ==========================================
#include <gtest/gtest.h>

// 简单测试
TEST(TestSuiteName, TestName) {
    // Arrange - 准备
    int a = 1, b = 2;

    // Act - 执行
    int result = a + b;

    // Assert - 断言
    EXPECT_EQ(result, 3);
}

// ==========================================
// 常用断言宏
// ==========================================

// 基本断言（失败时继续执行）
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);
EXPECT_EQ(val1, val2);      // ==
EXPECT_NE(val1, val2);      // !=
EXPECT_LT(val1, val2);      // <
EXPECT_LE(val1, val2);      // <=
EXPECT_GT(val1, val2);      // >
EXPECT_GE(val1, val2);      // >=

// 致命断言（失败时终止当前测试）
ASSERT_TRUE(condition);
ASSERT_EQ(val1, val2);
// ... 其他ASSERT_*与EXPECT_*对应

// 字符串断言
EXPECT_STREQ(str1, str2);   // C字符串相等
EXPECT_STRNE(str1, str2);   // C字符串不等
EXPECT_STRCASEEQ(s1, s2);   // 忽略大小写相等

// 浮点数断言
EXPECT_FLOAT_EQ(val1, val2);
EXPECT_DOUBLE_EQ(val1, val2);
EXPECT_NEAR(val1, val2, abs_error);

// 异常断言
EXPECT_THROW(statement, exception_type);
EXPECT_ANY_THROW(statement);
EXPECT_NO_THROW(statement);

// 谓词断言
EXPECT_PRED1(pred, val1);
EXPECT_PRED2(pred, val1, val2);
// 或使用lambda
EXPECT_PRED_FORMAT1(formatter, val1);
```

**测试夹具（Test Fixture）**：

```cpp
// ==========================================
// 测试夹具 - 共享测试设置
// ==========================================
class VectorTest : public ::testing::Test {
protected:
    // 每个测试前执行
    void SetUp() override {
        vec_.push_back(1);
        vec_.push_back(2);
        vec_.push_back(3);
    }

    // 每个测试后执行
    void TearDown() override {
        vec_.clear();
    }

    // 可以添加辅助函数
    bool contains(int value) {
        return std::find(vec_.begin(), vec_.end(), value) != vec_.end();
    }

    std::vector<int> vec_;
};

// 使用TEST_F而不是TEST
TEST_F(VectorTest, InitialSize) {
    EXPECT_EQ(vec_.size(), 3);
}

TEST_F(VectorTest, ContainsValue) {
    EXPECT_TRUE(contains(2));
    EXPECT_FALSE(contains(10));
}

TEST_F(VectorTest, PushBackIncreasesSize) {
    vec_.push_back(4);
    EXPECT_EQ(vec_.size(), 4);
}

// ==========================================
// 共享夹具（所有测试共享一次设置）
// ==========================================
class DatabaseTest : public ::testing::Test {
protected:
    // 整个测试套件执行一次
    static void SetUpTestSuite() {
        db_ = std::make_unique<Database>();
        db_->connect("test_db");
    }

    static void TearDownTestSuite() {
        db_->disconnect();
        db_.reset();
    }

    static std::unique_ptr<Database> db_;
};

std::unique_ptr<Database> DatabaseTest::db_;
```

### 第二周：参数化测试与类型测试

**学习目标**：掌握高级测试技术

**阅读材料**：
- [ ] GTest: Advanced Guide
- [ ] 参数化测试最佳实践

```cpp
// ==========================================
// 值参数化测试
// ==========================================
class PrimeTest : public ::testing::TestWithParam<int> {};

TEST_P(PrimeTest, IsPrime) {
    int n = GetParam();
    EXPECT_TRUE(is_prime(n)) << n << " should be prime";
}

// 单个值列表
INSTANTIATE_TEST_SUITE_P(
    PrimeNumbers,
    PrimeTest,
    ::testing::Values(2, 3, 5, 7, 11, 13)
);

// 范围
INSTANTIATE_TEST_SUITE_P(
    Range,
    PrimeTest,
    ::testing::Range(2, 10)  // 2到9
);

// 组合
class MultiplicationTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(MultiplicationTest, Multiply) {
    auto [a, b, expected] = GetParam();
    EXPECT_EQ(a * b, expected);
}

INSTANTIATE_TEST_SUITE_P(
    MulTable,
    MultiplicationTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1),
        std::make_tuple(2, 3, 6),
        std::make_tuple(4, 5, 20)
    )
);

// 笛卡尔积
INSTANTIATE_TEST_SUITE_P(
    CartesianProduct,
    SomeTest,
    ::testing::Combine(
        ::testing::Values(1, 2, 3),
        ::testing::Values("a", "b"),
        ::testing::Bool()
    )
);

// ==========================================
// 类型参数化测试
// ==========================================
template <typename T>
class ContainerTest : public ::testing::Test {
protected:
    T container_;
};

using ContainerTypes = ::testing::Types<
    std::vector<int>,
    std::list<int>,
    std::deque<int>
>;

TYPED_TEST_SUITE(ContainerTest, ContainerTypes);

TYPED_TEST(ContainerTest, EmptyOnConstruction) {
    EXPECT_TRUE(this->container_.empty());
}

TYPED_TEST(ContainerTest, SizeAfterPushBack) {
    this->container_.push_back(1);
    EXPECT_EQ(this->container_.size(), 1);
}

// ==========================================
// 运行时类型参数化
// ==========================================
template <typename T>
class TypedContainerTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(TypedContainerTest);

TYPED_TEST_P(TypedContainerTest, Empty) {
    TypeParam container;
    EXPECT_TRUE(container.empty());
}

TYPED_TEST_P(TypedContainerTest, Size) {
    TypeParam container;
    container.push_back(typename TypeParam::value_type{});
    EXPECT_EQ(container.size(), 1);
}

REGISTER_TYPED_TEST_SUITE_P(TypedContainerTest, Empty, Size);

using MyTypes = ::testing::Types<std::vector<int>, std::vector<double>>;
INSTANTIATE_TYPED_TEST_SUITE_P(My, TypedContainerTest, MyTypes);
```

### 第三周：Catch2框架

**学习目标**：掌握Catch2的特色功能

**阅读材料**：
- [ ] Catch2官方文档
- [ ] Catch2 vs Google Test对比

```cpp
// ==========================================
// Catch2基础
// ==========================================
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/generators/catch_generators.hpp>

// 基本测试（BDD风格）
TEST_CASE("Vector operations", "[vector]") {
    std::vector<int> v;

    SECTION("empty on construction") {
        REQUIRE(v.empty());
        REQUIRE(v.size() == 0);
    }

    SECTION("push_back increases size") {
        v.push_back(1);
        REQUIRE(v.size() == 1);

        SECTION("push_back again") {
            v.push_back(2);
            REQUIRE(v.size() == 2);
        }
    }

    SECTION("clear empties the vector") {
        v.push_back(1);
        v.push_back(2);
        v.clear();
        REQUIRE(v.empty());
    }
}

// ==========================================
// Catch2断言
// ==========================================

// 基本断言
REQUIRE(expr);      // 失败时终止
CHECK(expr);        // 失败时继续

// 比较
REQUIRE(a == b);
REQUIRE(a != b);
REQUIRE(a < b);

// 浮点数
REQUIRE(value == Catch::Approx(3.14).epsilon(0.01));
REQUIRE(value == Catch::Approx(3.14).margin(0.001));

// 异常
REQUIRE_THROWS(expression);
REQUIRE_THROWS_AS(expression, ExceptionType);
REQUIRE_THROWS_WITH(expression, "message");
REQUIRE_NOTHROW(expression);

// Matcher风格
using namespace Catch::Matchers;

REQUIRE_THAT(str, StartsWith("Hello"));
REQUIRE_THAT(str, EndsWith("World"));
REQUIRE_THAT(str, ContainsSubstring("middle"));
REQUIRE_THAT(str, Matches("H.*d"));  // 正则

REQUIRE_THAT(vec, Contains(42));
REQUIRE_THAT(vec, VectorContains(42));
REQUIRE_THAT(vec, IsEmpty());
REQUIRE_THAT(vec, SizeIs(3));
```

```cpp
// ==========================================
// Catch2生成器（数据驱动测试）
// ==========================================
TEST_CASE("Generators", "[generators]") {
    // 简单生成器
    auto i = GENERATE(1, 2, 3, 4, 5);
    REQUIRE(i > 0);

    // 范围生成器
    auto j = GENERATE(range(1, 10));
    REQUIRE(j >= 1);
    REQUIRE(j < 10);

    // 过滤器
    auto even = GENERATE(filter([](int n) { return n % 2 == 0; }, range(1, 100)));
    REQUIRE(even % 2 == 0);

    // 映射
    auto squared = GENERATE(map([](int n) { return n * n; }, range(1, 5)));

    // 组合
    auto a = GENERATE(1, 2, 3);
    auto b = GENERATE(10, 20);
    CAPTURE(a, b);  // 打印当前值
    REQUIRE(a + b > 0);

    // 随机生成
    auto random = GENERATE(take(100, random(1, 1000)));
    REQUIRE(random >= 1);
    REQUIRE(random <= 1000);
}

// ==========================================
// BDD风格测试
// ==========================================
SCENARIO("Bank account operations", "[account]") {
    GIVEN("A bank account with initial balance") {
        BankAccount account(1000);

        WHEN("Depositing money") {
            account.deposit(500);

            THEN("Balance increases") {
                REQUIRE(account.balance() == 1500);
            }
        }

        WHEN("Withdrawing money") {
            bool result = account.withdraw(300);

            THEN("Withdrawal succeeds") {
                REQUIRE(result);
                REQUIRE(account.balance() == 700);
            }
        }

        WHEN("Withdrawing more than balance") {
            bool result = account.withdraw(2000);

            THEN("Withdrawal fails") {
                REQUIRE_FALSE(result);
                REQUIRE(account.balance() == 1000);
            }
        }
    }
}

// ==========================================
// Catch2测试夹具
// ==========================================
class DatabaseFixture {
protected:
    Database db;

public:
    DatabaseFixture() {
        db.connect("test_db");
    }

    ~DatabaseFixture() {
        db.disconnect();
    }
};

TEST_CASE_METHOD(DatabaseFixture, "Database operations", "[database]") {
    SECTION("Insert and retrieve") {
        db.insert("key", "value");
        REQUIRE(db.get("key") == "value");
    }

    SECTION("Delete") {
        db.insert("key", "value");
        db.remove("key");
        REQUIRE_FALSE(db.exists("key"));
    }
}
```

### 第四周：Mock对象与测试设计

**学习目标**：掌握Mock测试和测试设计原则

**阅读材料**：
- [ ] Google Mock文档
- [ ] 《单元测试的艺术》
- [ ] FIRST原则

```cpp
// ==========================================
// Google Mock基础
// ==========================================
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// 接口定义
class HttpClient {
public:
    virtual ~HttpClient() = default;
    virtual std::string get(const std::string& url) = 0;
    virtual int post(const std::string& url, const std::string& body) = 0;
};

// Mock类
class MockHttpClient : public HttpClient {
public:
    MOCK_METHOD(std::string, get, (const std::string& url), (override));
    MOCK_METHOD(int, post, (const std::string& url, const std::string& body), (override));
};

// 使用Mock
TEST(ApiClientTest, FetchData) {
    MockHttpClient mockHttp;

    // 设置期望
    EXPECT_CALL(mockHttp, get("https://api.example.com/data"))
        .Times(1)
        .WillOnce(::testing::Return(R"({"status": "ok"})"));

    ApiClient client(&mockHttp);
    auto result = client.fetchData();

    EXPECT_EQ(result.status, "ok");
}

// ==========================================
// Mock高级用法
// ==========================================

// 参数匹配器
using namespace ::testing;

EXPECT_CALL(mock, method(_));                    // 任意参数
EXPECT_CALL(mock, method(Eq(42)));              // 等于42
EXPECT_CALL(mock, method(Gt(0)));               // 大于0
EXPECT_CALL(mock, method(HasSubstr("test")));   // 包含子串
EXPECT_CALL(mock, method(StartsWith("http"))); // 以...开头
EXPECT_CALL(mock, method(MatchesRegex(".*")));  // 正则匹配
EXPECT_CALL(mock, method(AllOf(Gt(0), Lt(100)))); // 组合条件

// 调用次数
EXPECT_CALL(mock, method()).Times(0);           // 不被调用
EXPECT_CALL(mock, method()).Times(1);           // 恰好1次
EXPECT_CALL(mock, method()).Times(AtLeast(1));  // 至少1次
EXPECT_CALL(mock, method()).Times(AtMost(3));   // 最多3次
EXPECT_CALL(mock, method()).Times(Between(2, 5)); // 2-5次

// 返回值设置
EXPECT_CALL(mock, method())
    .WillOnce(Return(42))
    .WillOnce(Return(100))
    .WillRepeatedly(Return(0));

// 副作用
EXPECT_CALL(mock, method(_))
    .WillOnce(DoAll(
        SaveArg<0>(&saved_value),  // 保存参数
        SetArgReferee<1>(100),     // 设置引用参数
        Return(true)
    ));

// 调用序列
{
    InSequence seq;
    EXPECT_CALL(mock, init());
    EXPECT_CALL(mock, process());
    EXPECT_CALL(mock, cleanup());
}

// 自定义行为
EXPECT_CALL(mock, compute(_))
    .WillOnce([](int x) { return x * 2; });
```

```cpp
// ==========================================
// 测试设计示例
// ==========================================

// FIRST原则：
// Fast - 快速执行
// Independent - 测试间独立
// Repeatable - 可重复执行
// Self-validating - 自验证
// Timely - 及时编写

// 被测类
class OrderProcessor {
public:
    explicit OrderProcessor(
        std::shared_ptr<Database> db,
        std::shared_ptr<PaymentGateway> payment,
        std::shared_ptr<EmailService> email
    ) : db_(db), payment_(payment), email_(email) {}

    enum class Result {
        Success,
        PaymentFailed,
        InsufficientStock,
        DatabaseError
    };

    Result processOrder(const Order& order) {
        // 检查库存
        auto product = db_->getProduct(order.productId);
        if (!product || product->stock < order.quantity) {
            return Result::InsufficientStock;
        }

        // 处理支付
        auto paymentResult = payment_->charge(
            order.customerId,
            order.quantity * product->price
        );
        if (!paymentResult.success) {
            return Result::PaymentFailed;
        }

        // 更新库存
        product->stock -= order.quantity;
        if (!db_->updateProduct(*product)) {
            payment_->refund(paymentResult.transactionId);
            return Result::DatabaseError;
        }

        // 发送确认邮件
        email_->sendOrderConfirmation(order.customerId, order);

        return Result::Success;
    }

private:
    std::shared_ptr<Database> db_;
    std::shared_ptr<PaymentGateway> payment_;
    std::shared_ptr<EmailService> email_;
};

// Mock接口
class MockDatabase : public Database {
public:
    MOCK_METHOD(std::optional<Product>, getProduct, (int id), (override));
    MOCK_METHOD(bool, updateProduct, (const Product& p), (override));
};

class MockPaymentGateway : public PaymentGateway {
public:
    MOCK_METHOD(PaymentResult, charge, (int customerId, double amount), (override));
    MOCK_METHOD(bool, refund, (const std::string& transactionId), (override));
};

class MockEmailService : public EmailService {
public:
    MOCK_METHOD(void, sendOrderConfirmation, (int customerId, const Order& order), (override));
};

// 测试类
class OrderProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockDb_ = std::make_shared<MockDatabase>();
        mockPayment_ = std::make_shared<MockPaymentGateway>();
        mockEmail_ = std::make_shared<MockEmailService>();

        processor_ = std::make_unique<OrderProcessor>(
            mockDb_, mockPayment_, mockEmail_
        );

        // 默认产品
        defaultProduct_ = Product{1, "Test Product", 10.0, 100};
    }

    std::shared_ptr<MockDatabase> mockDb_;
    std::shared_ptr<MockPaymentGateway> mockPayment_;
    std::shared_ptr<MockEmailService> mockEmail_;
    std::unique_ptr<OrderProcessor> processor_;
    Product defaultProduct_;
};

TEST_F(OrderProcessorTest, SuccessfulOrder) {
    Order order{1, 1, 5};  // customerId=1, productId=1, quantity=5

    EXPECT_CALL(*mockDb_, getProduct(1))
        .WillOnce(Return(defaultProduct_));

    EXPECT_CALL(*mockPayment_, charge(1, 50.0))
        .WillOnce(Return(PaymentResult{true, "tx123"}));

    EXPECT_CALL(*mockDb_, updateProduct(_))
        .WillOnce(Return(true));

    EXPECT_CALL(*mockEmail_, sendOrderConfirmation(1, _))
        .Times(1);

    auto result = processor_->processOrder(order);
    EXPECT_EQ(result, OrderProcessor::Result::Success);
}

TEST_F(OrderProcessorTest, InsufficientStock) {
    Order order{1, 1, 200};  // 超过库存

    EXPECT_CALL(*mockDb_, getProduct(1))
        .WillOnce(Return(defaultProduct_));

    // 支付和邮件不应被调用
    EXPECT_CALL(*mockPayment_, charge(_, _)).Times(0);
    EXPECT_CALL(*mockEmail_, sendOrderConfirmation(_, _)).Times(0);

    auto result = processor_->processOrder(order);
    EXPECT_EQ(result, OrderProcessor::Result::InsufficientStock);
}

TEST_F(OrderProcessorTest, PaymentFailed) {
    Order order{1, 1, 5};

    EXPECT_CALL(*mockDb_, getProduct(1))
        .WillOnce(Return(defaultProduct_));

    EXPECT_CALL(*mockPayment_, charge(1, 50.0))
        .WillOnce(Return(PaymentResult{false, ""}));

    // 库存不应更新，邮件不应发送
    EXPECT_CALL(*mockDb_, updateProduct(_)).Times(0);
    EXPECT_CALL(*mockEmail_, sendOrderConfirmation(_, _)).Times(0);

    auto result = processor_->processOrder(order);
    EXPECT_EQ(result, OrderProcessor::Result::PaymentFailed);
}

TEST_F(OrderProcessorTest, DatabaseErrorTriggersRefund) {
    Order order{1, 1, 5};

    EXPECT_CALL(*mockDb_, getProduct(1))
        .WillOnce(Return(defaultProduct_));

    EXPECT_CALL(*mockPayment_, charge(1, 50.0))
        .WillOnce(Return(PaymentResult{true, "tx123"}));

    EXPECT_CALL(*mockDb_, updateProduct(_))
        .WillOnce(Return(false));  // 数据库错误

    // 应该触发退款
    EXPECT_CALL(*mockPayment_, refund("tx123"))
        .WillOnce(Return(true));

    auto result = processor_->processOrder(order);
    EXPECT_EQ(result, OrderProcessor::Result::DatabaseError);
}
```

---

## 源码阅读任务

### 本月源码阅读

1. **Google Test源码**
   - 仓库：https://github.com/google/googletest
   - 重点：`googletest/src/gtest.cc`
   - 学习目标：理解断言宏的实现

2. **Catch2源码**
   - 仓库：https://github.com/catchorg/Catch2
   - 重点：`src/catch2/`
   - 学习目标：理解SECTION的实现原理

3. **知名项目的测试**
   - fmt库的测试
   - nlohmann/json的测试

---

## 实践项目

### 项目：测试驱动开发的JSON解析器

使用TDD方法开发一个简单的JSON解析器。

**项目结构**：

```
json-parser/
├── CMakeLists.txt
├── vcpkg.json
├── include/
│   └── jsonparser/
│       ├── json.hpp
│       ├── parser.hpp
│       └── value.hpp
├── src/
│   ├── parser.cpp
│   └── value.cpp
└── tests/
    ├── CMakeLists.txt
    ├── test_value.cpp
    ├── test_parser.cpp
    └── test_integration.cpp
```

**tests/CMakeLists.txt**：

```cmake
find_package(GTest CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# Google Test测试
add_executable(gtest_tests
    test_value.cpp
    test_parser.cpp
)

target_link_libraries(gtest_tests
    PRIVATE
        jsonparser
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(gtest_tests)

# Catch2测试
add_executable(catch2_tests
    test_integration.cpp
)

target_link_libraries(catch2_tests
    PRIVATE
        jsonparser
        Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(catch2_tests)
```

**include/jsonparser/value.hpp**：

```cpp
#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <stdexcept>

namespace jsonparser {

class JsonValue;

using JsonNull = std::nullptr_t;
using JsonBool = bool;
using JsonNumber = double;
using JsonString = std::string;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

class JsonError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class JsonValue {
public:
    using ValueType = std::variant<
        JsonNull,
        JsonBool,
        JsonNumber,
        JsonString,
        JsonArray,
        JsonObject
    >;

    // 构造函数
    JsonValue() : value_(nullptr) {}
    JsonValue(std::nullptr_t) : value_(nullptr) {}
    JsonValue(bool b) : value_(b) {}
    JsonValue(int n) : value_(static_cast<double>(n)) {}
    JsonValue(double n) : value_(n) {}
    JsonValue(const char* s) : value_(std::string(s)) {}
    JsonValue(std::string s) : value_(std::move(s)) {}
    JsonValue(JsonArray arr) : value_(std::move(arr)) {}
    JsonValue(JsonObject obj) : value_(std::move(obj)) {}

    // 类型检查
    bool is_null() const { return std::holds_alternative<JsonNull>(value_); }
    bool is_bool() const { return std::holds_alternative<JsonBool>(value_); }
    bool is_number() const { return std::holds_alternative<JsonNumber>(value_); }
    bool is_string() const { return std::holds_alternative<JsonString>(value_); }
    bool is_array() const { return std::holds_alternative<JsonArray>(value_); }
    bool is_object() const { return std::holds_alternative<JsonObject>(value_); }

    // 值获取
    template<typename T>
    T& get() { return std::get<T>(value_); }

    template<typename T>
    const T& get() const { return std::get<T>(value_); }

    // 便捷访问
    bool as_bool() const { return get<JsonBool>(); }
    double as_number() const { return get<JsonNumber>(); }
    const std::string& as_string() const { return get<JsonString>(); }
    const JsonArray& as_array() const { return get<JsonArray>(); }
    const JsonObject& as_object() const { return get<JsonObject>(); }

    // 数组访问
    JsonValue& operator[](size_t index);
    const JsonValue& operator[](size_t index) const;

    // 对象访问
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](const std::string& key) const;

    // 大小
    size_t size() const;

    // 比较
    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const { return !(*this == other); }

    // 序列化
    std::string dump(int indent = -1) const;

private:
    ValueType value_;

    std::string dump_impl(int indent, int current_indent) const;
};

} // namespace jsonparser
```

**tests/test_value.cpp**：

```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <jsonparser/value.hpp>

using namespace jsonparser;
using namespace ::testing;

class JsonValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

// ==========================================
// 构造函数测试
// ==========================================
TEST_F(JsonValueTest, DefaultConstructorCreatesNull) {
    JsonValue v;
    EXPECT_TRUE(v.is_null());
}

TEST_F(JsonValueTest, NullptrConstructor) {
    JsonValue v(nullptr);
    EXPECT_TRUE(v.is_null());
}

TEST_F(JsonValueTest, BoolConstructor) {
    JsonValue t(true);
    JsonValue f(false);

    EXPECT_TRUE(t.is_bool());
    EXPECT_TRUE(f.is_bool());
    EXPECT_TRUE(t.as_bool());
    EXPECT_FALSE(f.as_bool());
}

TEST_F(JsonValueTest, NumberConstructor) {
    JsonValue i(42);
    JsonValue d(3.14);

    EXPECT_TRUE(i.is_number());
    EXPECT_TRUE(d.is_number());
    EXPECT_DOUBLE_EQ(i.as_number(), 42.0);
    EXPECT_DOUBLE_EQ(d.as_number(), 3.14);
}

TEST_F(JsonValueTest, StringConstructor) {
    JsonValue s1("hello");
    JsonValue s2(std::string("world"));

    EXPECT_TRUE(s1.is_string());
    EXPECT_TRUE(s2.is_string());
    EXPECT_EQ(s1.as_string(), "hello");
    EXPECT_EQ(s2.as_string(), "world");
}

TEST_F(JsonValueTest, ArrayConstructor) {
    JsonArray arr = {1, 2, 3};
    JsonValue v(arr);

    EXPECT_TRUE(v.is_array());
    EXPECT_EQ(v.size(), 3);
}

TEST_F(JsonValueTest, ObjectConstructor) {
    JsonObject obj = {{"key", "value"}};
    JsonValue v(obj);

    EXPECT_TRUE(v.is_object());
    EXPECT_EQ(v.size(), 1);
}

// ==========================================
// 类型检查测试
// ==========================================
class JsonValueTypeTest : public ::testing::TestWithParam<std::tuple<JsonValue, std::string>> {};

TEST_P(JsonValueTypeTest, OnlyOneTypeReturnsTrue) {
    auto [value, expected_type] = GetParam();

    if (expected_type == "null") {
        EXPECT_TRUE(value.is_null());
    } else {
        EXPECT_FALSE(value.is_null());
    }

    if (expected_type == "bool") {
        EXPECT_TRUE(value.is_bool());
    } else {
        EXPECT_FALSE(value.is_bool());
    }

    if (expected_type == "number") {
        EXPECT_TRUE(value.is_number());
    } else {
        EXPECT_FALSE(value.is_number());
    }

    if (expected_type == "string") {
        EXPECT_TRUE(value.is_string());
    } else {
        EXPECT_FALSE(value.is_string());
    }

    if (expected_type == "array") {
        EXPECT_TRUE(value.is_array());
    } else {
        EXPECT_FALSE(value.is_array());
    }

    if (expected_type == "object") {
        EXPECT_TRUE(value.is_object());
    } else {
        EXPECT_FALSE(value.is_object());
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllTypes,
    JsonValueTypeTest,
    ::testing::Values(
        std::make_tuple(JsonValue(nullptr), "null"),
        std::make_tuple(JsonValue(true), "bool"),
        std::make_tuple(JsonValue(42), "number"),
        std::make_tuple(JsonValue("str"), "string"),
        std::make_tuple(JsonValue(JsonArray{}), "array"),
        std::make_tuple(JsonValue(JsonObject{}), "object")
    )
);

// ==========================================
// 访问器测试
// ==========================================
TEST_F(JsonValueTest, ArrayAccess) {
    JsonValue arr(JsonArray{1, 2, 3});

    EXPECT_DOUBLE_EQ(arr[0].as_number(), 1);
    EXPECT_DOUBLE_EQ(arr[1].as_number(), 2);
    EXPECT_DOUBLE_EQ(arr[2].as_number(), 3);
}

TEST_F(JsonValueTest, ArrayAccessOutOfBounds) {
    JsonValue arr(JsonArray{1});

    EXPECT_THROW(arr[10], std::out_of_range);
}

TEST_F(JsonValueTest, ObjectAccess) {
    JsonValue obj(JsonObject{{"name", "John"}, {"age", 30}});

    EXPECT_EQ(obj["name"].as_string(), "John");
    EXPECT_DOUBLE_EQ(obj["age"].as_number(), 30);
}

TEST_F(JsonValueTest, ObjectAccessNonExistent) {
    JsonValue obj(JsonObject{});

    // 访问不存在的键应该返回null
    EXPECT_TRUE(obj["missing"].is_null());
}

// ==========================================
// 比较测试
// ==========================================
TEST_F(JsonValueTest, EqualityNull) {
    EXPECT_EQ(JsonValue(nullptr), JsonValue(nullptr));
}

TEST_F(JsonValueTest, EqualityBool) {
    EXPECT_EQ(JsonValue(true), JsonValue(true));
    EXPECT_NE(JsonValue(true), JsonValue(false));
}

TEST_F(JsonValueTest, EqualityNumber) {
    EXPECT_EQ(JsonValue(42), JsonValue(42));
    EXPECT_EQ(JsonValue(42), JsonValue(42.0));
    EXPECT_NE(JsonValue(42), JsonValue(43));
}

TEST_F(JsonValueTest, EqualityString) {
    EXPECT_EQ(JsonValue("hello"), JsonValue("hello"));
    EXPECT_NE(JsonValue("hello"), JsonValue("world"));
}

TEST_F(JsonValueTest, EqualityArray) {
    EXPECT_EQ(
        JsonValue(JsonArray{1, 2, 3}),
        JsonValue(JsonArray{1, 2, 3})
    );
    EXPECT_NE(
        JsonValue(JsonArray{1, 2, 3}),
        JsonValue(JsonArray{1, 2, 4})
    );
}

TEST_F(JsonValueTest, EqualityDifferentTypes) {
    EXPECT_NE(JsonValue(42), JsonValue("42"));
    EXPECT_NE(JsonValue(nullptr), JsonValue(false));
}

// ==========================================
// 序列化测试
// ==========================================
TEST_F(JsonValueTest, DumpNull) {
    EXPECT_EQ(JsonValue(nullptr).dump(), "null");
}

TEST_F(JsonValueTest, DumpBool) {
    EXPECT_EQ(JsonValue(true).dump(), "true");
    EXPECT_EQ(JsonValue(false).dump(), "false");
}

TEST_F(JsonValueTest, DumpNumber) {
    EXPECT_EQ(JsonValue(42).dump(), "42");
    EXPECT_EQ(JsonValue(3.14).dump(), "3.14");
}

TEST_F(JsonValueTest, DumpString) {
    EXPECT_EQ(JsonValue("hello").dump(), "\"hello\"");
    EXPECT_EQ(JsonValue("hello\nworld").dump(), "\"hello\\nworld\"");
}

TEST_F(JsonValueTest, DumpEmptyArray) {
    EXPECT_EQ(JsonValue(JsonArray{}).dump(), "[]");
}

TEST_F(JsonValueTest, DumpArray) {
    JsonValue arr(JsonArray{1, 2, 3});
    EXPECT_EQ(arr.dump(), "[1,2,3]");
}

TEST_F(JsonValueTest, DumpEmptyObject) {
    EXPECT_EQ(JsonValue(JsonObject{}).dump(), "{}");
}

TEST_F(JsonValueTest, DumpObject) {
    JsonValue obj(JsonObject{{"key", "value"}});
    EXPECT_EQ(obj.dump(), "{\"key\":\"value\"}");
}

TEST_F(JsonValueTest, DumpPrettyPrint) {
    JsonValue obj(JsonObject{
        {"name", "John"},
        {"age", 30}
    });

    std::string expected = R"({
  "age": 30,
  "name": "John"
})";

    EXPECT_EQ(obj.dump(2), expected);
}
```

**tests/test_integration.cpp**（Catch2）：

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <jsonparser/json.hpp>

using namespace jsonparser;
using namespace Catch::Matchers;

TEST_CASE("JSON parsing and roundtrip", "[integration]") {
    SECTION("Simple values") {
        auto json_str = GENERATE(
            "null",
            "true",
            "false",
            "42",
            "3.14",
            "\"hello\"",
            "[]",
            "{}"
        );

        CAPTURE(json_str);

        auto parsed = Json::parse(json_str);
        auto dumped = parsed.dump();

        // 重新解析应该得到相同结果
        auto reparsed = Json::parse(dumped);
        REQUIRE(parsed == reparsed);
    }

    SECTION("Complex nested structure") {
        std::string json = R"({
            "name": "John",
            "age": 30,
            "active": true,
            "address": {
                "city": "New York",
                "zip": "10001"
            },
            "tags": ["developer", "c++"],
            "score": null
        })";

        auto parsed = Json::parse(json);

        REQUIRE(parsed["name"].as_string() == "John");
        REQUIRE(parsed["age"].as_number() == Catch::Approx(30));
        REQUIRE(parsed["active"].as_bool() == true);
        REQUIRE(parsed["address"]["city"].as_string() == "New York");
        REQUIRE(parsed["tags"][0].as_string() == "developer");
        REQUIRE(parsed["score"].is_null());
    }
}

SCENARIO("Building JSON programmatically", "[builder]") {
    GIVEN("An empty JSON object") {
        JsonObject obj;

        WHEN("Adding string values") {
            obj["name"] = "Alice";
            obj["email"] = "alice@example.com";

            THEN("Object contains the values") {
                JsonValue json(obj);
                REQUIRE(json["name"].as_string() == "Alice");
                REQUIRE(json["email"].as_string() == "alice@example.com");
            }
        }

        WHEN("Adding nested objects") {
            obj["user"] = JsonObject{
                {"id", 1},
                {"name", "Bob"}
            };

            THEN("Nested values are accessible") {
                JsonValue json(obj);
                REQUIRE(json["user"]["id"].as_number() == Catch::Approx(1));
                REQUIRE(json["user"]["name"].as_string() == "Bob");
            }
        }

        WHEN("Adding arrays") {
            obj["numbers"] = JsonArray{1, 2, 3, 4, 5};

            THEN("Array elements are accessible") {
                JsonValue json(obj);
                REQUIRE(json["numbers"].size() == 5);
                REQUIRE(json["numbers"][2].as_number() == Catch::Approx(3));
            }
        }
    }
}

TEST_CASE("Error handling", "[errors]") {
    SECTION("Invalid JSON throws exception") {
        REQUIRE_THROWS_AS(Json::parse("{invalid}"), JsonError);
        REQUIRE_THROWS_AS(Json::parse("[1,2,]"), JsonError);
        REQUIRE_THROWS_AS(Json::parse(""), JsonError);
    }

    SECTION("Type mismatch throws exception") {
        auto json = Json::parse(R"({"value": "string"})");

        REQUIRE_THROWS_AS(json["value"].as_number(), std::bad_variant_access);
    }
}

TEST_CASE("Unicode handling", "[unicode]") {
    SECTION("Unicode strings") {
        std::string json = R"({"text": "Hello, \u4e16\u754c"})";
        auto parsed = Json::parse(json);

        REQUIRE_THAT(parsed["text"].as_string(), ContainsSubstring("Hello"));
    }

    SECTION("Emoji") {
        std::string json = R"({"emoji": "\ud83d\ude00"})";
        auto parsed = Json::parse(json);

        // 应该正确处理代理对
        REQUIRE_FALSE(parsed["emoji"].as_string().empty());
    }
}

TEST_CASE("Large JSON handling", "[performance]") {
    SECTION("Large array") {
        JsonArray large_array;
        for (int i = 0; i < 10000; ++i) {
            large_array.push_back(i);
        }

        JsonValue json(large_array);
        auto serialized = json.dump();
        auto reparsed = Json::parse(serialized);

        REQUIRE(reparsed.size() == 10000);
    }

    SECTION("Deeply nested structure") {
        std::string json = "0";
        for (int i = 0; i < 100; ++i) {
            json = "[" + json + "]";
        }

        auto parsed = Json::parse(json);
        REQUIRE(parsed.is_array());
    }
}
```

---

## 检验标准

- [ ] 掌握Google Test的核心功能
- [ ] 掌握Catch2的BDD风格测试
- [ ] 能够使用Mock进行依赖隔离
- [ ] 能够编写参数化测试
- [ ] 理解测试设计的FIRST原则
- [ ] 能够配置代码覆盖率分析

### 知识检验问题

1. EXPECT和ASSERT断言的区别是什么？
2. Catch2的SECTION如何实现测试隔离？
3. 什么时候应该使用Mock？
4. 如何设计可测试的代码？

---

## 输出物清单

1. **项目代码**
   - `json-parser/` - 完整的TDD项目
   - 测试覆盖率报告

2. **测试模板**
   - Google Test项目模板
   - Catch2项目模板

3. **文档**
   - `notes/month42_testing.md` - 学习笔记
   - `notes/testing_patterns.md` - 测试模式总结

4. **CMake配置**
   - 测试集成CMake配置
   - 覆盖率配置

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | Google Test基础 | 15h | 15h | 5h |
| 第2周 | 参数化测试 | 12h | 18h | 5h |
| 第3周 | Catch2框架 | 10h | 20h | 5h |
| 第4周 | Mock与测试设计 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 43将学习**Clang-Tidy静态分析**，掌握代码质量检查和自动修复技术。
