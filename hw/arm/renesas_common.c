#include "qemu/osdep.h"

#include "ra2l1.h"
#include "renesas_common.h"

static char debug_str[0x800];
static char s_datetime[0x20];
static char b2c(uint8_t b)
{
    if (b < 10)
    {
        return '0' + b;
    }
    else if (b < 0x10)
    {
        return 'A' + b - 0xa;
    }
    else
    {
        return '_';
    }
}

char *b2s(const uint8_t *data, int n)
{
    int i;
    uint8_t upper, lower;
    char *p = debug_str;
    for (i = 0; i < n; i++)
    {
        upper = (data[i] & 0xf0) >> 4;
        lower = (data[i] & 0x0f) >> 0;
        *p++ = b2c(upper);
        *p++ = b2c(lower);
    }
    *p = 0;
    return debug_str;
}

char *current_datetime_str(void)
{
    static struct timespec ts;
    static struct tm tm;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    sprintf(s_datetime, "%d%02d%02d_%02d%02d%02d.%03ld", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000);
    return s_datetime;
}
