#pragma once

#include <functional>
#include <iostream>
#include <source_location>
#include <string>
#include <syncstream>
#include <vector>

namespace fast_test {

struct TestResult {
    bool passed;
    std::string name;
    std::string file;
    int line;
    std::string message;
};

class TestRunner {
   public:
    static TestRunner& instance() {
        static TestRunner instance;
        return instance;
    }

    void register_test(const std::string& name,
                       std::function<void()> test_func) {
        tests_.push_back({name, test_func});
    }

    void run_all() {
        int passed = 0;
        int failed = 0;
        std::cout << "\n[==========] Running " << tests_.size() << " tests.\n";

        for (const auto& test : tests_) {
            current_test_name_ = test.name;
            current_test_failed_ = false;

            std::cout << "[ RUN      ] " << test.name << "\n";
            try {
                test.func();
            } catch (const std::exception& e) {
                fail("", 0, std::string("Unhandled exception: ") + e.what());
            } catch (...) {
                fail("", 0, "Unknown exception");
            }

            if (!current_test_failed_) {
                std::cout << "[       OK ] " << test.name << "\n";
                passed++;
            } else {
                std::cout << "[  FAILED  ] " << test.name << "\n";
                failed++;
            }
        }

        std::cout << "\n[==========] " << (passed + failed) << " tests ran.\n";
        std::cout << "[  PASSED  ] " << passed << " tests.\n";
        if (failed > 0) {
            std::cout << "[  FAILED  ] " << failed << " tests.\n";
        }
    }

    // Fixed: Removed test_name argument, use current_test_name_
    void fail(const std::string& file, int line, const std::string& msg) {
        std::osyncstream(std::cerr) << file << ":" << line << ": Failure\n"
                                    << "Value of: " << msg << "\n";
        current_test_failed_ = true;
    }

   private:
    struct TestEntry {
        std::string name;
        std::function<void()> func;
    };

    std::vector<TestEntry> tests_;
    std::string current_test_name_;
    bool current_test_failed_ = false;
};

// Helper macro to register tests
struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> func) {
        TestRunner::instance().register_test(name, func);
    }
};

// Assertion macros - Fixed: Removed current_test_name_
#define EXPECT_TRUE(condition)                                          \
    if (!(condition)) {                                                 \
        fast_test::TestRunner::instance().fail(__FILE__, __LINE__,      \
                                               #condition " is false"); \
    }

#define EXPECT_FALSE(condition)                                        \
    if (condition) {                                                   \
        fast_test::TestRunner::instance().fail(__FILE__, __LINE__,     \
                                               #condition " is true"); \
    }

#define EXPECT_EQ(a, b)                                                    \
    if ((a) != (b)) {                                                      \
        fast_test::TestRunner::instance().fail(__FILE__, __LINE__,         \
                                               std::string(#a " != " #b)); \
    }

#define TEST(TestSuiteName, TestName)                                \
    void TestSuiteName##_##TestName();                               \
    fast_test::TestRegistrar TestSuiteName##_##TestName##_registrar( \
        #TestSuiteName "." #TestName, TestSuiteName##_##TestName);   \
    void TestSuiteName##_##TestName()

}    // namespace fast_test

// Global runner access
inline int RUN_ALL_TESTS() {
    fast_test::TestRunner::instance().run_all();
    return 0;
}
