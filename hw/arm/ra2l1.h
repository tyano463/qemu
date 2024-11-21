#ifndef __RA2L1_H__
#define __RA2L1_H__

#include "chardev/char-fe.h"
#include "hw/arm/armv7m.h"
#include "hw/sysbus.h"
#include "qemu/osdep.h"

#include "ra2l1_sc324_aes.h"

#define RA2L1_UART_NUM 10

#define SCI_SCR_TIE_MASK (0x80U) ///< Transmit Interrupt Enable

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
    sc324_state_t *aes;
    Clock *sysclk;
    Clock *refclk;
    QemuMutex lock;
    qemu_irq irq_agt;
    Notifier shutdown_notifier;
} RA2L1State;

int local_uart_init(RA2L1State *s, int channel,
                    void (*cb)(int channel, const char *s, ssize_t n));
void local_uart_write(int channel, const char *s, size_t size);

#endif