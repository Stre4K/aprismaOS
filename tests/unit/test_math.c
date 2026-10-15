// test_math.c
// <math.h>: src/libc/math/

#include "aprisma.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

// Deterministic random doubles from raw bit patterns: covers zeros,
// subnormals, infinities and NaNs as well as ordinary values
static uint64_t rng_state = 0x243F6A8885A308D3ULL;

static uint64_t next_random(void)
{
    rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return rng_state ^ (rng_state >> 29);
}

static double random_double_bits(void)
{
    uint64_t u = next_random();
    double d;
    memcpy(&d, &u, sizeof(d));
    return d;
}

static int same_double(double a, double b)
{
    if (isnan(a) || isnan(b))
        return isnan(a) && isnan(b);
    return memcmp(&a, &b, sizeof(a)) == 0;  // also tells +0 and -0 apart
}

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

TEST(frexp_splits_mantissa_and_exponent)
{
    int e;
    CHECK_NEAR(aprisma_frexp(8.0, &e), 0.5, 0);
    CHECK_INT_EQ(e, 4);
    CHECK_NEAR(aprisma_frexp(-3.0, &e), -0.75, 0);
    CHECK_INT_EQ(e, 2);
    CHECK_NEAR(aprisma_frexp(0.0, &e), 0.0, 0);
    CHECK_INT_EQ(e, 0);
}

TEST(frexp_matches_host_libc)
{
    for (int i = 0; i < 100000; i++) {
        double x = random_double_bits();
        int ours, host;
        double m = aprisma_frexp(x, &ours);
        double h = frexp(x, &host);
        if (!same_double(m, h) || (isfinite(x) && ours != host)) {
            test_fail(__FILE__, __LINE__, "frexp(%a): host %a * 2^%d, got %a * 2^%d", x, h, host, m, ours);
            return;
        }
    }
}

TEST(ldexp_scales_by_powers_of_two)
{
    CHECK_NEAR(aprisma_ldexp(0.75, 3), 6.0, 0);
    CHECK_NEAR(aprisma_ldexp(1.0, -1), 0.5, 0);
    CHECK(isinf(aprisma_ldexp(1.0, 5000)));
    CHECK(aprisma_ldexp(1.0, -5000) == 0.0);
}

TEST(ldexp_matches_host_libc)
{
    for (int i = 0; i < 100000; i++) {
        double x = random_double_bits();
        int n = (int)(next_random() % 4401) - 2200;
        double ours = aprisma_ldexp(x, n);
        double host = ldexp(x, n);
        // A subnormal result may be rounded twice while scaling, so allow
        // one unit in the last place there; everything else must be exact
        int ok = same_double(ours, host);
        if (!ok && fpclassify(host) == FP_SUBNORMAL)
            ok = fabs(ours - host) <= nextafter(fabs(host), INFINITY) - fabs(host);
        if (!ok) {
            test_fail(__FILE__, __LINE__, "ldexp(%a, %d): host %a, got %a", x, n, host, ours);
            return;
        }
    }
}
