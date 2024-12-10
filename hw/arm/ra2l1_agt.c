#include "qemu/osdep.h"

#include "hw/irq.h"
#include <time.h>

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
static uint8_t agtcr;

bool is_agt(hwaddr addr)
{
    return ((uintptr_t)R_AGT0 <= addr) && (addr < ((uintptr_t)R_AGT0 + sizeof(R_AGTX0_Type)));
}

uint64_t ra2l1_mmio_agt_read(void *opaque, hwaddr _addr, unsigned size)
{
    uint64_t value = 0;

    renesas_agt_t *s = (renesas_agt_t *)opaque;
    hwaddr addr = s->baseaddr + _addr - (s->channel * ((intptr_t)R_AGT1 - (intptr_t)R_AGT0));

    switch (addr)
    {
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTCR:
        value = agtcr;
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
        // agtcr_log("%x", agtcr);
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

static uint64_t getfreq(void)
{
    uint64_t candidates[] = {PCLKB, PCLKB / 8, 0, PCLKB / 2, 0, 0, 0, 0};
    return candidates[tck];
}

static void *timer_main(void *arg)
{
    struct timespec interval;
    uint64_t ns;
    uint64_t freq;

    renesas_agt_t *s = (renesas_agt_t *)arg;

    freq = getfreq();
    if (counter == 0)
    {
        counter = 0xffff;
    }
    ns = (uint64_t)(1e9 * (double)counter / freq);
    ns *= 100;
    interval.tv_sec = ns / 1e9;
    interval.tv_nsec = ns % (uint64_t)1e9;
    typedef struct IRQState
    {
        Object parent_obj;

        qemu_irq_handler handler;
        void *opaque;
        int n;
    } irq_t;
    dlog("timer started handler:%p interval:%f", ((irq_t *)s->irq_agt)->handler,
         ((double)ns) / 1e9);
    while (running)
    {
        nanosleep(&interval, NULL);
        qemu_irq_pulse(s->irq_agt);
        if (tmod != 1)
        {
            dlog("timer stopped");
            running = false;
        }
    }
    return NULL;
}

static void start_timer(void *opaque)
{
    pthread_t th;
    int ret;

    if (!running)
    {
        running = true;
        ret = pthread_create(&th, NULL, timer_main, opaque);
        if (ret)
        {
            dlog("timer thread create failed");
        }
    }
}

static void proc_timer(void *opaque, uint64_t value)
{
    switch (value)
    {
    case AGT_PRV_AGTCR_START_TIMER:
        agtcr |= (0x3);
        start_timer(opaque);
        break;
    case AGT_PRV_AGTCR_STOP_TIMER:
        agtcr &= (~((uint8_t)0x3));
        break;
    default:
        break;
    }
}
static void setmode(uint64_t value, int kind)
{
    if (kind)
    {
        dr = value & 7;
    }
    else
    {
        tmod = value & 7;
        // dlog("update tmod:%x", tmod);
        tck = (value & 0x70) >> 4;
    }
}

void ra2l1_mmio_agt_write(void *opaque, hwaddr _addr, uint64_t value, unsigned size)
{
    renesas_agt_t *s = (renesas_agt_t *)opaque;

    hwaddr addr = s->baseaddr + _addr - (s->channel * ((intptr_t)R_AGT1 - (intptr_t)R_AGT0));
    // CPUState *cpu = (CPUState *)s->s->armv7m.cpu;

    switch (addr)
    {
    case (uintptr_t)&R_AGT0->AGT16.AGT:
        counter = (uint16_t)value & 0xffff;
        dlog("%08lx <- %08lx sz:%d", addr, value, size);
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTCR:
        agtcr = (uint8_t)(value & 0xff);
        // dlog("%08lx <- %08lx sz:%d", addr, value, size);
        proc_timer(opaque, value);
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTMR1:
        setmode(value, 0);
        dlog("%08lx <- %08lx sz:%d", addr, value, size);
        break;
    case (uintptr_t)&R_AGT0->AGT16.CTRL.AGTMR2:
        setmode(value, 1);
        dlog("%08lx <- %08lx sz:%d", addr, value, size);
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

static MemoryRegionOps ops = {
    .read = ra2l1_mmio_agt_read,
    .write = ra2l1_mmio_agt_write,
};

renesas_agt_t *ra2l1_agt_init(MemoryRegion *system_memory, RA2L1State *s, DeviceState *dev_soc, hwaddr baseaddr,
                              int channel)
{
    renesas_agt_t *agt = g_new0(renesas_agt_t, 1);
    DeviceState *armv7m = DEVICE(&s->armv7m);
    agt->channel = channel;
    agt->baseaddr = baseaddr;
    sysbus_init_irq(SYS_BUS_DEVICE(dev_soc), &agt->irq_agt);
    memory_region_init_io(&agt->mmio, OBJECT(dev_soc), &ops, agt, "renesas.agt", sizeof(R_AGTX0_Type));
    memory_region_add_subregion(system_memory, baseaddr, &agt->mmio);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev_soc), 0, qdev_get_gpio_in(armv7m, 3));
    agt->s = s;
    return agt;
}
