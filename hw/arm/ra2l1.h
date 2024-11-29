#ifndef __RA2L1_H__
#define __RA2L1_H__

#include "chardev/char-fe.h"
#include "hw/arm/armv7m.h"
#include "hw/sysbus.h"
#include "qemu/osdep.h"

typedef struct ra_state RA2L1State;
#include "ra2l1_agt.h"
#include "ra2l1_sc324_aes.h"

#define RENESAS_LOCAL_UART 1

#define RA2L1_UART_NUM 10
#define RA2L1_AGT_NUM 10

#define SCI_SCR_TIE_MASK (0x80U) ///< Transmit Interrupt Enable

typedef struct {
    Object parent_obj;

    qemu_irq_handler handler;
    void *opaque;
    int n;
} irq_t;

typedef struct ra_uart {
    SysBusDevice parent;
    CharBackend chr;
    MemoryRegion mmio;
    qemu_irq irq_txi;
    qemu_irq irq_tei;
    qemu_irq irq_rxi;
    bool rx_full;
    int channel;

#ifdef RENESAS_LOCAL_UART
    pthread_t th;
    int fd;
    int running;
    void (*callback)(int channel, const char *s, size_t n);
    char rx_buf[0x400];
    size_t rx_pos;
#endif
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
    renesas_agt_t *agt[RA2L1_AGT_NUM];
    Clock *sysclk;
    Clock *refclk;
    QemuMutex lock;
    Notifier shutdown_notifier;
} RA2L1State;

int local_uart_init(MemoryRegion *sysmem, RA2L1State *s, DeviceState *, hwaddr, int channel);

#endif