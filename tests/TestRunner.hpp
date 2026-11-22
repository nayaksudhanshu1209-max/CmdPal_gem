#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cmdpal::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline void expectTrue(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::ostringstream oss;
        oss << file << ":" << line << " ASSERT_TRUE failed: " << expr;
        throw std::runtime_error(oss.str());
    }
}

template <typename A, typename B>
inline void expectEq(const A& a, const B& b, const char* exprA, const char* exprB, const char* file, int line) {
    if (!(a == b)) {
        std::ostringstream oss;
        oss << file << ":" << line << " ASSERT_EQ failed: " << exprA << " != " << exprB;
        throw std::runtime_error(oss.str());
    }
}

inline int runAllTests() {
    int passed = 0;
    int failed = 0;
    for (const auto& tc : registry()) {
        try {
            tc.fn();
            ++passed;
            std::cout << "[PASS] " << tc.name << "\n";
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[FAIL] " << tc.name << " -> " << ex.what() << "\n";
        }
    }
    std::cout << "\nTotal: " << (passed + failed) << ", Passed: " << passed << ", Failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace cmdpal::test

#define TEST_CASE(name) \
    static void test_fn_##name(); \
    static ::cmdpal::test::Registrar test_reg_##name(#name, test_fn_##name); \
    static void test_fn_##name()

#define ASSERT_TRUE(expr) ::cmdpal::test::expectTrue((expr), #expr, __FILE__, __LINE__)
#define ASSERT_FALSE(expr) ::cmdpal::test::expectTrue(!(expr), "!" #expr, __FILE__, __LINE__)
#define ASSERT_EQ(a, b) ::cmdpal::test::expectEq((a), (b), #a, #b, __FILE__, __LINE__)
