// Tiny dependency-free test harness for host-side (native g++) tests of the
// platform-independent parts of this library (SCEX_Easing, SCEX_Yaml).
// Not a general-purpose framework -- just enough to assert and report.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>

namespace scex_test {

inline int& failureCount() {
    static int count = 0;
    return count;
}

inline void reportFailure(const char* file, int line, const std::string& message) {
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, message.c_str());
    failureCount()++;
}

inline int finish() {
    if (failureCount() == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d assertion(s) FAILED\n", failureCount());
    return 1;
}

}  // namespace scex_test

#define SCEX_RUN(fn)                    \
    do {                                \
        std::printf("running %s\n", #fn); \
        fn();                           \
    } while (0)

#define SCEX_ASSERT_TRUE(cond)                                                    \
    do {                                                                          \
        if (!(cond)) scex_test::reportFailure(__FILE__, __LINE__, "expected true: " #cond); \
    } while (0)

#define SCEX_ASSERT_FALSE(cond)                                                     \
    do {                                                                            \
        if (cond) scex_test::reportFailure(__FILE__, __LINE__, "expected false: " #cond); \
    } while (0)

#define SCEX_ASSERT_EQ_INT(expected, actual)                                                  \
    do {                                                                                      \
        long e = static_cast<long>(expected);                                                 \
        long a = static_cast<long>(actual);                                                    \
        if (e != a) {                                                                         \
            scex_test::reportFailure(__FILE__, __LINE__,                                       \
                                      std::string(#actual " == " #expected " (") +             \
                                          std::to_string(a) + " != " + std::to_string(e) + ")"); \
        }                                                                                       \
    } while (0)

#define SCEX_ASSERT_EQ_STR(expected, actual)                                                  \
    do {                                                                                      \
        std::string e = (expected);                                                            \
        std::string a = (actual);                                                              \
        if (e != a) {                                                                          \
            scex_test::reportFailure(__FILE__, __LINE__,                                        \
                                      std::string(#actual " == " #expected " (\"") + a +        \
                                          "\" != \"" + e + "\")");                               \
        }                                                                                        \
    } while (0)

#define SCEX_ASSERT_NEAR(expected, actual, tolerance)                                          \
    do {                                                                                       \
        double e = (expected);                                                                  \
        double a = (actual);                                                                     \
        if (std::fabs(e - a) > (tolerance)) {                                                    \
            scex_test::reportFailure(__FILE__, __LINE__,                                          \
                                      std::string(#actual " ~= " #expected " (") +                \
                                          std::to_string(a) + " vs " + std::to_string(e) + ")");    \
        }                                                                                          \
    } while (0)
