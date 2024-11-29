#include "qemu/osdep.h"

#include "ra2l1.h"
#include "renesas_common.h"

static char debug_str[0x800];
static char b2c(uint8_t b) {
    if (b < 10) {
        return '0' + b;
    } else if (b < 0x10) {
        return 'A' + b - 0xa;
    } else {
        return '_';
    }
}
char *b2s(const uint8_t *data, int n) {
    int i;
    uint8_t upper, lower;
    char *p = debug_str;
    for (i = 0; i < n; i++) {
        upper = (data[i] & 0xf0) >> 4;
        lower = (data[i] & 0x0f) >> 0;
        *p++ = b2c(upper);
        *p++ = b2c(lower);
    }
    *p = 0;
    return debug_str;
}
