// rtc.c
// <kernel/drivers/rtc.h>
//
// CMOS registers are read through ports 0x70 (register number) and 0x71
// (value). The clock updates itself once a second; while status register A
// bit 7 is set an update is under way, and a read could mix old and new
// values (e.g. 12:59:59 becoming 12:00:00). So wait for that bit to clear,
// read everything, and repeat until two reads agree.

#include <kernel/drivers/rtc.h>
#include <kernel/arch/portio.h>

#define CMOS_INDEX    0x70
#define CMOS_DATA     0x71
#define CMOS_STATUS_A 0x0A
#define CMOS_STATUS_B 0x0B
#define STATUS_A_UPDATING 0x80
#define STATUS_B_24_HOUR  0x02
#define STATUS_B_BINARY   0x04
#define HOUR_PM           0x80

static const uint8_t registers[RTC_FIELDS] = { 0x00, 0x02, 0x04, 0x07, 0x08, 0x09, 0x32 };

static uint8_t cmos(uint8_t reg)
{
    outb(CMOS_INDEX, reg);
    return inb(CMOS_DATA);
}

static int from_bcd(uint8_t value, bool binary)
{
    return binary ? value : (value >> 4) * 10 + (value & 0x0F);
}

bool rtc_decode(const uint8_t raw[RTC_FIELDS], uint8_t status_b, date_t *out)
{
    bool binary = status_b & STATUS_B_BINARY;
    int hour = from_bcd(raw[RTC_HOURS] & (uint8_t)~HOUR_PM, binary);
    if (!(status_b & STATUS_B_24_HOUR)) {
        // 12-hour clock: 12 AM is 0:00, 12 PM is 12:00
        if (hour < 1 || hour > 12)
            return false;
        hour %= 12;
        if (raw[RTC_HOURS] & HOUR_PM)
            hour += 12;
    }

    int year = from_bcd(raw[RTC_YEAR], binary);
    int century = raw[RTC_CENTURY] ? from_bcd(raw[RTC_CENTURY], binary) : (year >= 70 ? 19 : 20);

    out->second = from_bcd(raw[RTC_SECONDS], binary);
    out->minute = from_bcd(raw[RTC_MINUTES], binary);
    out->hour = hour;
    out->day = from_bcd(raw[RTC_DAY], binary);
    out->month = from_bcd(raw[RTC_MONTH], binary);
    out->year = century * 100 + year;

    if (out->second > 59 || out->minute > 59 || out->hour > 23 || out->day < 1 || out->day > 31 ||
        out->month < 1 || out->month > 12 || year > 99 || century < 19 || century > 99)
        return false;

    // weekday from the date itself (the clock's own register is unreliable)
    date_t full;
    time_to_date(time_from_date(out), &full);
    out->weekday = full.weekday;
    return full.day == out->day;  // no 31 February
}

static void read_raw(uint8_t raw[RTC_FIELDS])
{
    for (int tries = 0; tries < 1000000 && (cmos(CMOS_STATUS_A) & STATUS_A_UPDATING); tries++)
        ;
    for (int i = 0; i < RTC_FIELDS; i++)
        raw[i] = cmos(registers[i]);
}

bool rtc_read(date_t *out)
{
    uint8_t raw[RTC_FIELDS], again[RTC_FIELDS];
    read_raw(raw);
    for (int tries = 0; tries < 10; tries++) {
        read_raw(again);
        bool same = true;
        for (int i = 0; i < RTC_FIELDS; i++)
            same &= raw[i] == again[i];
        if (same)
            return rtc_decode(raw, cmos(CMOS_STATUS_B), out);
        for (int i = 0; i < RTC_FIELDS; i++)
            raw[i] = again[i];
    }
    return false;  // never settled
}
