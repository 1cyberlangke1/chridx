// 输入：无（用例通过 TEST 宏注册）。
// 输出：每个用例一行 [ OK ]/[FAIL]，末尾一行汇总；有失败时进程退出码为 1。
// 预期行为：断言失败只记录并继续跑，不抛异常、不崩，保证一次跑完全部用例。
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace chridx_test {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failure_count() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back(Case{name, std::move(fn)});
    }
};

// 输入：文件名、行号、断言表达式文本
// 输出：打印一行 [FAIL] 并把失败计数 +1
// 预期行为：永不抛异常，让当前用例继续跑完
inline void report_failure(const char* file, int line, const std::string& what) {
    ++failure_count();
    std::printf("    [FAIL] %s:%d  %s\n", file, line, what.c_str());
}

// 输入：无
// 输出：跑完所有用例，返回 0（全绿）或 1（有失败）
// 预期行为：一个用例里多处断言失败只算一个失败用例，但失败条数逐条累计
inline int run_all() {
    int failed_cases = 0;
    for (const Case& c : registry()) {
        const int before = failure_count();
        c.fn();
        const bool failed = failure_count() > before;
        if (failed) {
            ++failed_cases;
        }
        std::printf("[%s] %s\n", failed ? "FAIL" : " OK ", c.name);
    }
    std::printf("\n%d test cases, %d failed cases, %d failed assertions\n",
                static_cast<int>(registry().size()), failed_cases, failure_count());
    return failure_count() == 0 ? 0 : 1;
}

}

#define CHRIDX_TEST_CAT_(a, b) a##b
#define CHRIDX_TEST_CAT(a, b) CHRIDX_TEST_CAT_(a, b)

#define TEST(name)                                                          \
    static void CHRIDX_TEST_CAT(chridx_test_case_, __LINE__)();             \
    static const ::chridx_test::Registrar CHRIDX_TEST_CAT(                  \
        chridx_test_reg_, __LINE__)(                                        \
        name, &CHRIDX_TEST_CAT(chridx_test_case_, __LINE__));               \
    static void CHRIDX_TEST_CAT(chridx_test_case_, __LINE__)()

#define CHECK(expr)                                                         \
    do {                                                                    \
        if (!(expr)) {                                                      \
            ::chridx_test::report_failure(__FILE__, __LINE__,               \
                                          "CHECK(" #expr ")");              \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)                                                      \
    do {                                                                    \
        if (!((a) == (b))) {                                                \
            ::chridx_test::report_failure(__FILE__, __LINE__,               \
                                          "CHECK_EQ(" #a ", " #b ")");      \
        }                                                                   \
    } while (0)

#define CHECK_THROWS(expr, exception_type)                                  \
    do {                                                                    \
        bool chridx_test_threw_expected_ = false;                           \
        bool chridx_test_threw_other_ = false;                              \
        try {                                                               \
            (void)(expr);                                                   \
        } catch (const exception_type&) {                                   \
            chridx_test_threw_expected_ = true;                             \
        } catch (...) {                                                     \
            chridx_test_threw_other_ = true;                                \
        }                                                                   \
        if (chridx_test_threw_other_) {                                     \
            ::chridx_test::report_failure(__FILE__, __LINE__,               \
                                          "CHECK_THROWS(" #expr            \
                                          "): threw a different exception type"); \
        } else if (!chridx_test_threw_expected_) {                          \
            ::chridx_test::report_failure(__FILE__, __LINE__,               \
                                          "CHECK_THROWS(" #expr            \
                                          "): did not throw");              \
        }                                                                   \
    } while (0)
