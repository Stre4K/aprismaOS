// abi/time.h
// Dates and times: seconds since 1970-01-01 00:00:00 UTC ("Unix time") and
// calendar dates, both ways. Shared by the kernel and user programs (src/user
// includes this file too); everything is static inline, so nothing has to be
// linked. The calendar is the Gregorian one; there are no time zones (UTC).

#ifndef __KERNEL_ABI_TIME_H__
#define __KERNEL_ABI_TIME_H__

typedef struct {
    int year;      // e.g. 2026
    int month;     // 1-12
    int day;       // 1-31
    int hour;      // 0-23
    int minute;    // 0-59
    int second;    // 0-59
    int weekday;   // 0 = Sunday ... 6 = Saturday (filled in by time_to_date)
} date_t;

// Days since 1970-01-01 (H. Hinnant's days_from_civil: an era is the 400
// years after which the calendar repeats, March is taken as the first
// month so that February's leap day comes last)
static inline long long days_from_date(int year, int month, int day)
{
    long long y = year - (month <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;                                  // [0, 399]
    long long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
    return era * 146097 + doe - 719468;
}

static inline long long time_from_date(const date_t *d)
{
    return days_from_date(d->year, d->month, d->day) * 86400LL +
           d->hour * 3600LL + d->minute * 60LL + d->second;
}

static inline void time_to_date(long long t, date_t *d)
{
    long long days = t >= 0 ? t / 86400 : (t - 86399) / 86400;  // round down
    long long secs = t - days * 86400;
    d->hour = (int)(secs / 3600);
    d->minute = (int)(secs / 60 % 60);
    d->second = (int)(secs % 60);
    d->weekday = (int)((days % 7 + 11) % 7);  // 1970-01-01 was a Thursday

    long long z = days + 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    d->day = (int)(doy - (153 * mp + 2) / 5 + 1);
    d->month = (int)(mp < 10 ? mp + 3 : mp - 9);
    d->year = (int)(yoe + era * 400 + (d->month <= 2));
}

#endif // __KERNEL_ABI_TIME_H__
