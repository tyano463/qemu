#ifndef __RA2L1_GPT_H__
#define __RA2L1_GPT_H__

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "exec/hwaddr.h"

typedef struct ra_gpt
{
    SysBusDevice parent;
    qemu_irq irq;
    void *opaque;
    MemoryRegion mmio;

} RA2L1GPTState;

RA2L1GPTState *ra2l1_gpt_add(MemoryRegion *system_memory, RA2L1State *s, DeviceState *dev_soc, int channel);
#endif