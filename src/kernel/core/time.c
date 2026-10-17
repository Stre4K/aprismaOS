// time.c
// <kernel/time.h>

#include <kernel/time.h>
#include <kernel/arch/timer.h>
#include <kernel/drivers/rtc.h>

static int64_t boot_seconds;  // the clock's time when it was read
static uint64_t boot_ticks;   // the timer then

bool time_init(void)
{
    date_t date;
    bool ok = rtc_read(&date);
    boot_seconds = ok ? time_from_date(&date) : 0;
    boot_ticks = pit_get_ticks();
    return ok;
}

int64_t time_now_ms(void)
{
    uint64_t ticks = pit_get_ticks() - boot_ticks;
    return boot_seconds * 1000 + (int64_t)(ticks * 1000 / PIT_FREQUENCY);
}

uint64_t time_now(void)
{
    return (uint64_t)(time_now_ms() / 1000);
}
