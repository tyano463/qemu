#include "qemu/osdep.h"
#include <time.h>
#include "hw/irq.h"

#include "R7FA2L1AB.h"
#include "ra2l1.h"
#include "ra2l1_agt.h"
#include "renesas_common.h"

#define BSP_HOCO_HZ 48000000
#define PCLKB (BSP_HOCO_HZ >> 1)

static int running;
static uint16_t counter;
static uint8_t tmod;
static uint8_t tck;
static uint8_t dr;

bool is_agt(hwaddr addr) {
    return ((uintptr_t)R_AGT0 <= addr) &&
           (addr < ((uintptr_t)R_AGT0 + sizeof(R_AGTX0_Type)));
}

uint64_t ra2l1_mmio_agt_read(void *opaque, hwaddr addr, unsigned size) {
    uint64_t value = 0;
    switch (addr) {
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTCR:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTMR1:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTMR2:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTIOSEL_ALT:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTIOC:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTISR:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTCMSR:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTIOSEL:
        break;
    default:
        break;
    }
    return value;
}

static uint64_t getfreq(void) {
    uint64_t candidates[] = {PCLKB, PCLKB / 8, 0, PCLKB / 2, 0, 0, 0, 0};
    return candidates[tck];
}

static void *timer_main(void *arg) {
    struct timespec interval;
    uint64_t ns;
    uint64_t freq;

    RA2L1State *s = (RA2L1State *)arg;

    freq = getfreq();
    ns = (uint64_t)(1e9 * (double)counter / freq);
    interval.tv_sec = ns / 1e9;
    interval.tv_nsec = ns % (uint64_t)1e9;
    dlog("timer started");
    while (running) {
        nanosleep(&interval, NULL);
        qemu_irq_pulse(s->irq_agt);
        if (tmod != 1) {
            dlog("timer stopped");
            running = false;
        }
    }
    return NULL;
}

static void start_timer(void *opaque) {
    pthread_t th;
    int ret;

    running = true;
    ret = pthread_create(&th, NULL, timer_main, opaque);
    if (ret) {
        dlog("timer thread create failed");
    }
}

static void proc_timer(void *opaque, uint64_t value) {
    switch (value) {
    case AGT_PRV_AGTCR_START_TIMER:
        start_timer(opaque);
        break;

    default:
        break;
    }
}
static void setmode(uint64_t value, int kind) {
    if (kind) {
        dr = value & 7;
    } else {
        tmod = value & 7;
        dlog("update tmod:%x", tmod);
        tck = (value & 0x70) >> 4;
    }
}

void ra2l1_mmio_agt_write(void *opaque, hwaddr addr, uint64_t value,
                          unsigned size) {
    // dlog("a:%lx v:%lx", addr, value);
    switch (addr) {
    case (uintptr_t)&R_AGT0->AGT16.AGT:
        counter = (uint16_t)value & 0xffff;
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTCR:
        proc_timer(opaque, value);
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTMR1:
        setmode(value, 0);
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTMR2:
        setmode(value, 1);
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTIOSEL_ALT:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTIOC:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTISR:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTCMSR:
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTIOSEL:
        break;

    default:
        break;
    }
}