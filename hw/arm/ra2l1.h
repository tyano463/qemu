#ifndef __RA2L1_H__
#define __RA2L1_H__

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/arm/armv7m.h"
#include "chardev/char-fe.h"

#define RA2L1_UART_NUM 10

typedef struct ra_uart {
    SysBusDevice parent;
    CharBackend chr;
    qemu_irq irq_txi;
    qemu_irq irq_tei;
    qemu_irq irq_rxi;
} RA2L1UartState;

typedef struct ra_state {
    SysBusDevice parent_obj;
    ARMv7MState armv7m;
    MemoryRegion iomem;
    MemoryRegion sram;
    MemoryRegion flash;
    MemoryRegion dflash;
    MemoryRegion flash_alias;
    MemoryRegion flash_io;
    MemoryRegion mmio;
    RA2L1UartState uart[RA2L1_UART_NUM];
    Clock *sysclk;
    Clock *refclk;
    QemuMutex lock;
    qemu_irq irq_agt;
} RA2L1State;

#endif