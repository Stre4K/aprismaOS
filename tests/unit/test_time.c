// test_time.c
// Dates <-> seconds (src/kernel/include/kernel/abi/time.h, inline, so it is
// tested as it is) and decoding the real-time clock's registers
// (src/kernel/drivers/rtc.c), checked against the host's gmtime.

#include "aprisma.h"
#include "test.h"

#include <kernel/abi/time.h>
#include <kernel/drivers/rtc.h>
#include <stdint.h>
#include <time.h>

bool aprisma_rtc_decode(const uint8_t raw[RTC_FIELDS], uint8_t status_b, date_t *out);

#define CHECK_DATE(d, y, mo, da, h, mi, s, wd)                                          \
    do {                                                                               \
        CHECK_INT_EQ((d).year, y);                                                     \
        CHECK_INT_EQ((d).month, mo);                                                   \
        CHECK_INT_EQ((d).day, da);                                                     \
        CHECK_INT_EQ((d).hour, h);                                                     \
        CHECK_INT_EQ((d).minute, mi);                                                  \
        CHECK_INT_EQ((d).second, s);                                                   \
        CHECK_INT_EQ((d).weekday, wd);                                                 \
    } while (0)

TEST(time_known_dates)
{
    date_t d;
    time_to_date(0, &d);
    CHECK_DATE(d, 1970, 1, 1, 0, 0, 0, 4);              // a Thursday
    time_to_date(951782400, &d);
    CHECK_DATE(d, 2000, 2, 29, 0, 0, 0, 2);             // 2000 is a leap year
    time_to_date(2147483648LL, &d);
    CHECK_DATE(d, 2038, 1, 19, 3, 14, 8, 2);            // past 32-bit time_t
    time_to_date(4107542400LL, &d);
    CHECK_DATE(d, 2100, 3, 1, 0, 0, 0, 1);              // 2100 is not: Feb 28 is followed by Mar 1
    time_to_date(4107542400LL - 1, &d);
    CHECK_DATE(d, 2100, 2, 28, 23, 59, 59, 0);
    time_to_date(-1, &d);
    CHECK_DATE(d, 1969, 12, 31, 23, 59, 59, 3);         // before 1970
    time_to_date(981173106, &d);
    CHECK_DATE(d, 2001, 2, 3, 4, 5, 6, 6);              // the tests' fixed clock
}

TEST(time_round_trips_and_matches_the_host)
{
    uint64_t seed = 42;
    for (int i = 0; i < 200000; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        long long t = (long long)(seed >> 26) - (1LL << 37);  // about +-4000 years around 1970
        date_t d;
        time_to_date(t, &d);
        CHECK_INT_EQ(time_from_date(&d), t);

        time_t host_t = (time_t)t;
        struct tm tm;
        CHECK(gmtime_r(&host_t, &tm) != NULL);
        if (d.year != tm.tm_year + 1900 || d.month != tm.tm_mon + 1 || d.day != tm.tm_mday ||
            d.hour != tm.tm_hour || d.minute != tm.tm_min || d.second != tm.tm_sec || d.weekday != tm.tm_wday) {
            CHECK_INT_EQ(t, -1);  // shows which time differs
        }
    }
}

static const uint8_t BINARY = 0x04, H24 = 0x02;

TEST(rtc_decode_bcd_and_binary)
{
    date_t d;
    // BCD, 24 hours: 2026-10-17 23:59:58, century register 0x20
    uint8_t bcd[RTC_FIELDS] = { 0x58, 0x59, 0x23, 0x17, 0x10, 0x26, 0x20 };
    CHECK(aprisma_rtc_decode(bcd, H24, &d));
    CHECK_DATE(d, 2026, 10, 17, 23, 59, 58, 6);

    uint8_t bin[RTC_FIELDS] = { 58, 59, 23, 17, 10, 26, 20 };
    CHECK(aprisma_rtc_decode(bin, H24 | BINARY, &d));
    CHECK_DATE(d, 2026, 10, 17, 23, 59, 58, 6);
}

TEST(rtc_decode_12_hour_clock)
{
    date_t d;
    uint8_t raw[RTC_FIELDS] = { 0x00, 0x00, 0x12, 0x01, 0x01, 0x26, 0x20 };
    CHECK(aprisma_rtc_decode(raw, 0, &d));          // 12 AM
    CHECK_INT_EQ(d.hour, 0);
    raw[RTC_HOURS] = 0x80 | 0x12;                   // 12 PM
    CHECK(aprisma_rtc_decode(raw, 0, &d));
    CHECK_INT_EQ(d.hour, 12);
    raw[RTC_HOURS] = 0x80 | 0x07;                   // 7 PM
    CHECK(aprisma_rtc_decode(raw, 0, &d));
    CHECK_INT_EQ(d.hour, 19);
    raw[RTC_HOURS] = 0x13;                          // there is no 13 o'clock
    CHECK(!aprisma_rtc_decode(raw, 0, &d));
}

TEST(rtc_decode_century_and_bad_values)
{
    date_t d;
    uint8_t raw[RTC_FIELDS] = { 0, 0, 0, 0x01, 0x01, 0x99, 0x00 };  // no century register
    CHECK(aprisma_rtc_decode(raw, H24, &d));
    CHECK_INT_EQ(d.year, 1999);
    raw[RTC_YEAR] = 0x05;
    CHECK(aprisma_rtc_decode(raw, H24, &d));
    CHECK_INT_EQ(d.year, 2005);

    uint8_t bad[RTC_FIELDS] = { 0, 0, 0, 0x31, 0x02, 0x26, 0x20 };  // 31 February
    CHECK(!aprisma_rtc_decode(bad, H24, &d));
    bad[RTC_DAY] = 0x01;
    bad[RTC_MONTH] = 0x13;
    CHECK(!aprisma_rtc_decode(bad, H24, &d));
    bad[RTC_MONTH] = 0x01;
    bad[RTC_SECONDS] = 0x60;
    CHECK(!aprisma_rtc_decode(bad, H24, &d));
}
