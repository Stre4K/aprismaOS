// test_math.c
// <math.h>: src/libc/math/

#include "aprisma.h"
#include "test.h"

#include <math.h>

#define EPS 1e-9

TEST(abs_and_labs)
{
    CHECK_INT_EQ(aprisma_abs(-5), 5);
    CHECK_INT_EQ(aprisma_abs(5), 5);
    CHECK_INT_EQ(aprisma_labs(-5L), 5);
}

TEST(min_and_max)
{
    CHECK_INT_EQ(aprisma_min(3, -2), -2);
    CHECK_INT_EQ(aprisma_max(3, -2), 3);
}

TEST(fabs_drops_sign)
{
    CHECK_NEAR(aprisma_fabs(-2.5), 2.5, EPS);
    CHECK_NEAR(aprisma_fabs(2.5), 2.5, EPS);
}

TEST(floor_and_ceil)
{
    CHECK_NEAR(aprisma_floor(2.7), 2.0, EPS);
    CHECK_NEAR(aprisma_floor(-2.5), -3.0, EPS);
    CHECK_NEAR(aprisma_ceil(2.1), 3.0, EPS);
    CHECK_NEAR(aprisma_ceil(-2.1), -2.0, EPS);
}

TEST_KNOWN_BUG(floor_beyond_int_range,
               "floor/ceil convert through int, so values beyond INT_MAX overflow")
{
    CHECK_NEAR(aprisma_floor(3e9), 3e9, EPS);
    CHECK_NEAR(aprisma_ceil(-3e9), -3e9, EPS);
}

TEST_KNOWN_BUG(round_halfway_away_from_zero,
               "round(-2.5) rounds toward zero instead of away from it")
{
    CHECK_NEAR(aprisma_round(2.4), 2.0, EPS);
    CHECK_NEAR(aprisma_round(2.5), 3.0, EPS);
    CHECK_NEAR(aprisma_round(-2.5), -3.0, EPS);
}

TEST(fmod_positive)
{
    CHECK_NEAR(aprisma_fmod(5.0, 3.0), 2.0, EPS);
    CHECK_NEAR(aprisma_fmod(7.5, 2.0), 1.5, EPS);
}

TEST_KNOWN_BUG(fmod_takes_sign_of_dividend,
               "fmod uses floor(), so the result takes the sign of the divisor")
{
    CHECK_NEAR(aprisma_fmod(-5.0, 3.0), -2.0, EPS);
}

TEST_KNOWN_BUG(sqrt_basic,
               "Newton iteration stops at an absolute 1e-5 tolerance, so results are only accurate to ~1e-6")
{
    CHECK_NEAR(aprisma_sqrt(16.0), 4.0, 1e-6);
    CHECK_NEAR(aprisma_sqrt(2.0), 1.41421356237, 1e-6);
}

TEST_KNOWN_BUG(sqrt_small_values,
               "sqrt returns x unchanged when x is below the 1e-5 tolerance")
{
    CHECK_NEAR(aprisma_sqrt(1e-8), 1e-4, 1e-6);
}

TEST_KNOWN_BUG(sqrt_large_values_terminate,
               "the absolute tolerance is below one ULP for large x, so the loop never ends")
{
    CHECK_NEAR(aprisma_sqrt(2e13 + 7), sqrt(2e13 + 7), 1e-6);
}

TEST(cbrt_positive)
{
    CHECK_NEAR(aprisma_cbrt(27.0), 3.0, 1e-6);
}

TEST_KNOWN_BUG(cbrt_negative,
               "cbrt does not handle negative input")
{
    CHECK_NEAR(aprisma_cbrt(-8.0), -2.0, 1e-6);
}

TEST(pow_integer_exponents)
{
    CHECK_NEAR(aprisma_pow(2.0, 10.0), 1024.0, EPS);
    CHECK_NEAR(aprisma_pow(2.0, -1.0), 0.5, EPS);
    CHECK_NEAR(aprisma_pow(5.0, 0.0), 1.0, EPS);
}

TEST_KNOWN_BUG(pow_fractional_exponent,
               "computes exp(base * log(exponent)) instead of exp(exponent * log(base))")
{
    CHECK_NEAR(aprisma_pow(2.0, 0.5), 1.41421356237, 1e-6);
    CHECK_NEAR(aprisma_pow(9.0, 1.5), 27.0, 1e-6);
}

TEST(exp_basic)
{
    CHECK_NEAR(aprisma_exp(0.0), 1.0, 1e-9);
    CHECK_NEAR(aprisma_exp(1.0), 2.71828182846, 1e-6);
    CHECK_NEAR(aprisma_exp(-1.0), 0.36787944117, 1e-6);
}

TEST_KNOWN_BUG(exp_overflow_returns_infinity,
               "(int)x overflows for huge x and the Taylor series loop never ends")
{
    CHECK(isinf(aprisma_exp(3e9)));
}

TEST(log_basic)
{
    CHECK_NEAR(aprisma_log(1.0), 0.0, 1e-9);
    CHECK_NEAR(aprisma_log(2.71828182846), 1.0, 1e-6);
}

TEST_KNOWN_BUG(log_large_values,
               "the initial guess x-1 overflows exp() for x above ~710, giving NaN")
{
    CHECK_NEAR(aprisma_log(1000.0), 6.90775527898, 1e-6);
}
