// test.h
// Minimal unit test framework for host-side tests of AprismaOS code.
//
//   TEST(name) { CHECK_INT_EQ(1 + 1, 2); }
//   TEST_KNOWN_BUG(name, "why it fails") { ... }
//
// Tests register themselves at startup. A failed check reports where and why
// and ends the test. Tests marked TEST_KNOWN_BUG are expected to fail: the
// suite stays green while they do, and reports an error once they pass, so
// the marker gets removed together with the fix.

#ifndef APRISMA_TEST_H
#define APRISMA_TEST_H

#include <stddef.h>
#include <string.h>

typedef void (*test_fn_t)(void);

void test_register(const char *file, const char *name, test_fn_t fn, const char *known_bug);
void test_fail(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define TEST_DEFINE(name, known_bug)                                          \
    static void name(void);                                                   \
    __attribute__((constructor)) static void test_register_##name(void) {     \
        test_register(__FILE__, #name, name, known_bug);                      \
    }                                                                         \
    static void name(void)

#define TEST(name) TEST_DEFINE(name, NULL)
#define TEST_KNOWN_BUG(name, reason) TEST_DEFINE(name, reason)

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            test_fail(__FILE__, __LINE__, "check failed: %s", #cond);         \
            return;                                                           \
        }                                                                     \
    } while (0)

#define CHECK_INT_EQ(actual, expected)                                        \
    do {                                                                      \
        long long a_ = (long long)(actual), e_ = (long long)(expected);       \
        if (a_ != e_) {                                                       \
            test_fail(__FILE__, __LINE__, "%s: expected %lld, got %lld",      \
                      #actual, e_, a_);                                       \
            return;                                                           \
        }                                                                     \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                        \
    do {                                                                      \
        const char *a_ = (actual), *e_ = (expected);                          \
        if (a_ == NULL || strcmp(a_, e_) != 0) {                              \
            test_fail(__FILE__, __LINE__, "%s: expected \"%s\", got \"%s\"",  \
                      #actual, e_, a_ ? a_ : "(null)");                       \
            return;                                                           \
        }                                                                     \
    } while (0)

#define CHECK_MEM_EQ(actual, expected, size)                                  \
    do {                                                                      \
        if (memcmp((actual), (expected), (size)) != 0) {                      \
            test_fail(__FILE__, __LINE__, "%s differs from %s",               \
                      #actual, #expected);                                    \
            return;                                                           \
        }                                                                     \
    } while (0)

// Floating point comparison with a relative tolerance
#define CHECK_NEAR(actual, expected, tolerance)                               \
    do {                                                                      \
        double a_ = (actual), e_ = (expected);                                \
        double d_ = a_ > e_ ? a_ - e_ : e_ - a_;                              \
        double m_ = e_ < 0 ? -e_ : e_;                                        \
        if (!(d_ <= (tolerance) * (m_ > 1 ? m_ : 1))) {                       \
            test_fail(__FILE__, __LINE__, "%s: expected %.10g, got %.10g",    \
                      #actual, e_, a_);                                       \
            return;                                                           \
        }                                                                     \
    } while (0)

#endif // APRISMA_TEST_H
