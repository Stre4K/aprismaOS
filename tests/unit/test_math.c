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

// Distance in units in the last place between two finite doubles of the
// same sign (how many representable doubles lie between them)
static uint64_t ulp_distance(double a, double b)
{
    int64_t ia, ib;
    memcpy(&ia, &a, sizeof(a));
    memcpy(&ib, &b, sizeof(b));
    if ((ia < 0) != (ib < 0))
        return a == b ? 0 : UINT64_MAX;
    return ia > ib ? (uint64_t)(ia - ib) : (uint64_t)(ib - ia);
}

// Random positive double spread over the whole exponent range
static double random_positive_double(void)
{
    double d;
    do {
        d = fabs(random_double_bits());
    } while (!isfinite(d) || d == 0);
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

TEST(floor_beyond_int_range)
{
    CHECK_NEAR(aprisma_floor(3e9), 3e9, EPS);
    CHECK_NEAR(aprisma_ceil(-3e9), -3e9, EPS);
}

TEST(round_halfway_away_from_zero)
{
    CHECK_NEAR(aprisma_round(2.4), 2.0, EPS);
    CHECK_NEAR(aprisma_round(2.5), 3.0, EPS);
    CHECK_NEAR(aprisma_round(-2.5), -3.0, EPS);
}

TEST(round_just_below_half)
{
    // 0.49999999999999994 + 0.5 rounds up to 1.0 in floating point
    CHECK_NEAR(aprisma_round(0.49999999999999994), 0.0, 0);
}

TEST(floor_ceil_round_match_host_libc)
{
    for (int i = 0; i < 100000; i++) {
        // Half the inputs from raw bits, half in the range where fractions exist
        double x = (i % 2) ? random_double_bits()
                           : ((double)(int64_t)next_random() / 4294967296.0) / (double)(1 + next_random() % 1000);
        if (!same_double(aprisma_floor(x), floor(x)) ||
            !same_double(aprisma_ceil(x), ceil(x)) ||
            !same_double(aprisma_round(x), round(x))) {
            test_fail(__FILE__, __LINE__, "x=%a: floor %a/%a ceil %a/%a round %a/%a (ours/host)", x,
                      aprisma_floor(x), floor(x), aprisma_ceil(x), ceil(x), aprisma_round(x), round(x));
            return;
        }
    }
}

TEST(fmod_positive)
{
    CHECK_NEAR(aprisma_fmod(5.0, 3.0), 2.0, EPS);
    CHECK_NEAR(aprisma_fmod(7.5, 2.0), 1.5, EPS);
}

TEST(fmod_takes_sign_of_dividend)
{
    CHECK_NEAR(aprisma_fmod(-5.0, 3.0), -2.0, EPS);
}

TEST(fmod_special_values)
{
    CHECK(isnan(aprisma_fmod(1.0, 0.0)));
    CHECK(isnan(aprisma_fmod(INFINITY, 2.0)));
    CHECK_NEAR(aprisma_fmod(5.0, INFINITY), 5.0, 0);
}

TEST(fmod_matches_host_libc)
{
    for (int i = 0; i < 20000; i++) {
        double x = random_double_bits();
        double y = (i % 2) ? random_double_bits() : (double)(1 + next_random() % 1000) / 7.0;
        double ours = aprisma_fmod(x, y);
        double host = fmod(x, y);
        if (!same_double(ours, host)) {
            test_fail(__FILE__, __LINE__, "fmod(%a, %a): host %a, got %a", x, y, host, ours);
            return;
        }
    }
}

TEST(sqrt_basic)
{
    CHECK_NEAR(aprisma_sqrt(16.0), 4.0, 1e-6);
    CHECK_NEAR(aprisma_sqrt(2.0), 1.41421356237, 1e-6);
}

TEST(sqrt_small_values)
{
    CHECK_NEAR(aprisma_sqrt(1e-8), 1e-4, 1e-6);
}

TEST(sqrt_large_values_terminate)
{
    CHECK_NEAR(aprisma_sqrt(2e13 + 7), sqrt(2e13 + 7), 1e-6);
}

TEST(sqrt_special_values)
{
    CHECK(isnan(aprisma_sqrt(-1.0)));
    CHECK(isnan(aprisma_sqrt(NAN)));
    CHECK(isinf(aprisma_sqrt(INFINITY)));
    CHECK(aprisma_sqrt(0.0) == 0.0);
}

TEST(sqrt_matches_host_libc)
{
    for (int i = 0; i < 100000; i++) {
        double x = random_positive_double();
        uint64_t ulps = ulp_distance(aprisma_sqrt(x), sqrt(x));
        if (ulps > 1) {
            test_fail(__FILE__, __LINE__, "sqrt(%a): host %a, got %a (%llu ulp)",
                      x, sqrt(x), aprisma_sqrt(x), (unsigned long long)ulps);
            return;
        }
    }
}

// glibc's cbrt() is itself up to 3 ulp off, so compare with the 80-bit
// cbrtl() rounded to double instead
TEST(cbrt_matches_long_double_reference)
{
    for (int i = 0; i < 100000; i++) {
        double x = random_positive_double();
        if (i % 2) x = -x;
        double reference = (double)cbrtl((long double)x);
        uint64_t ulps = ulp_distance(aprisma_cbrt(x), reference);
        if (ulps > 1) {
            test_fail(__FILE__, __LINE__, "cbrt(%a): reference %a, got %a (%llu ulp)",
                      x, reference, aprisma_cbrt(x), (unsigned long long)ulps);
            return;
        }
    }
}

TEST(cbrt_positive)
{
    CHECK_NEAR(aprisma_cbrt(27.0), 3.0, 1e-6);
}

TEST(cbrt_negative)
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
