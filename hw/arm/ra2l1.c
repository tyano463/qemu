#include "qemu/osdep.h"

#include "chardev/char-fe.h"
#include "exec/address-spaces.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "hw/irq.h"
#include "hw/loader.h"
#include "hw/misc/unimp.h"
#include "hw/qdev-clock.h"
#include "hw/qdev-properties-system.h"
#include "qapi/error.h"
#include "qemu/datadir.h"
#include "qemu/error-report.h"
#include "qemu/thread.h"
#include "qemu/units.h"
#include "sysemu/sysemu.h"
#include "target/arm/cpu.h"

#include "R7FA2L1AB.h"
#include "ra2l1_sc324_aes.h"
#include "renesas_common.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

#define NUM_IRQ_LINES 32
#define NUM_PRIO_BITS 3
#define SYSCLK_FRQ 24000000ULL
#define RA2L1_UART_NUM 10

#define TYPE_RA2L1_SYS "ra2l1-sys"
#define TYPE_RA2L1_UART "ra2l1-uart"

OBJECT_DECLARE_SIMPLE_TYPE(ra_state, RA2L1_SYS)

OBJECT_DECLARE_SIMPLE_TYPE(ra_uart, RA2L1_UART)

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
} RA2L1State;

static uint64_t ra2l1_flash_io_read(void *opaque, hwaddr, unsigned size);
static void ra2l1_flash_io_write(void *opaque, hwaddr addr, uint64_t value,
                                 unsigned size);
static uint64_t ra2l1_mmio_read(void *opaque, hwaddr, unsigned size);
static void ra2l1_mmio_write(void *opaque, hwaddr addr, uint64_t value,
                             unsigned size);
static void uart_write(void *opaque, int kind, uint8_t data);
static void register_irq(DeviceState *dev_soc, DeviceState *armv7m);
static void uart_receive(char *s, int n);

void im920_init(void (*received)(char *, int));
void im920_write(char *, int);

static MemoryRegionOps mmio_op = {.read = ra2l1_mmio_read,
                                  .write = ra2l1_mmio_write,
                                  .endianness = DEVICE_LITTLE_ENDIAN};
static MemoryRegionOps flash_io_op = {.read = ra2l1_flash_io_read,
                                      .write = ra2l1_flash_io_write,
                                      .endianness = DEVICE_LITTLE_ENDIAN};

static RA2L1State *g_state;
static uint64_t lp_fstatr1 = 0;
static uint8_t *uart_rbuf;
static uint16_t uart_rindex;
static uint16_t uart_rlen;
static uint8_t **uart_wbuf;
static uint16_t *uart_windex;

static void ra2l1_soc_initfn(Object *obj) {
    RA2L1State *s = RA2L1_SYS(obj);

    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);

    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
    s->refclk = qdev_init_clock_in(DEVICE(s), "refclk", NULL, NULL, 0);
}

static void ra2l1_soc_realize(DeviceState *dev_soc, Error **errp) {
    RA2L1State *s = RA2L1_SYS(dev_soc);
    DeviceState *armv7m;
    MemoryRegionOps *ops = &flash_io_op;
    MemoryRegionOps *mmio_ops = &mmio_op;
    MemoryRegion *system_memory = get_system_memory();

    g_state = s;
    /*
     * We use s->refclk internally and only define it with qdev_init_clock_in()
     * so it is correctly parented and not leaked on an init/deinit; it is not
     * intended as an externally exposed clock.
     */
    if (clock_has_source(s->refclk)) {
        error_setg(errp, "refclk clock must not be wired up by the board code");
        return;
    }

    if (!clock_has_source(s->sysclk)) {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    /*
     * TODO: ideally we should model the SoC RCC and its ability to
     * change the sysclk frequency and define different sysclk sources.
     */

    /* The refclk always runs at frequency HCLK / 8 */
    clock_set_mul_div(s->refclk, 8, 1);
    clock_set_source(s->refclk, s->sysclk);

    memory_region_init_rom(&s->flash, OBJECT(dev_soc), "ra2l1.flash", 0x20000,
                           &error_fatal);
    memory_region_init_alias(&s->flash_alias, OBJECT(dev_soc),
                             "ra2l1.flash.alias", &s->flash, 0, 0x20000);
    memory_region_add_subregion(system_memory, 0, &s->flash);
    memory_region_add_subregion(system_memory, 0, &s->flash_alias);

    memory_region_init_rom(&s->dflash, OBJECT(dev_soc), "ra2l1.dflash", 0x2000,
                           &error_fatal);
    memory_region_add_subregion(system_memory, 0x40100000, &s->dflash);

    /* Init SRAM region */
    memory_region_init_ram(&s->sram, NULL, "ra2l1.sram", 0x8000, &error_fatal);
    memory_region_add_subregion(system_memory, 0x20000000, &s->sram);

    /* Init ARMv7m */
    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 61);
    qdev_prop_set_uint8(armv7m, "num-prio-bits", 4);
    qdev_prop_set_string(armv7m, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m3"));
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_connect_clock_in(armv7m, "refclk", s->refclk);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(get_system_memory()), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }

    register_irq(dev_soc, armv7m);

    memory_region_init_io(&s->flash_io, OBJECT(dev_soc), ops, dev_soc,
                          "ra2l1.flash_io", 0x20000);
    memory_region_add_subregion(system_memory, 0x407E0000, &s->flash_io);

    memory_region_init_io(&s->mmio, OBJECT(dev_soc), mmio_ops, dev_soc,
                          "ra2l1.mmio", 0x100000);
    memory_region_add_subregion(system_memory, 0x40000000, &s->mmio);

    uart_wbuf = MALLOC2d(uint8_t, 1024, 10);
    uart_windex = calloc(sizeof(uint16_t) * 10, 1);

    uart_rbuf = malloc(1024);
    uart_rindex = 0;
    uart_rlen = 0;
    qemu_mutex_init(&s->lock);
}

static void register_irq(DeviceState *dev_soc, DeviceState *armv7m) {
    RA2L1State *s = RA2L1_SYS(dev_soc);
    DeviceState *dev;
    Error *errp = NULL;
    int i;
    SysBusDevice *busdev;
    struct {
        int txi;
        int tei;
        int rxi;
    } irqnum[] = {{1, 2, 8}, {-1}, {5, 6, 8}, {-1}, {-1},
                  {-1},      {-1}, {-1},      {-1}, {13, 14, 12}};

    // dlog("IN s:%p ds:%p a:%p", s, dev_soc, armv7m);

    for (i = 0; i < RA2L1_UART_NUM; i++) {
        object_initialize_child(OBJECT(dev_soc), "uart[*]", &s->uart[i],
                                TYPE_RA2L1_UART);
    }

    for (i = 0; i < RA2L1_UART_NUM; i++) {
        dev = DEVICE(&(s->uart[i]));
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->uart[i]), &errp)) {
            g_print("uart[%d] realize failed\n", i);
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);

        if (irqnum[i].txi != -1) {
            sysbus_connect_irq(busdev, 0,
                               qdev_get_gpio_in(armv7m, irqnum[i].txi));
            sysbus_connect_irq(busdev, 1,
                               qdev_get_gpio_in(armv7m, irqnum[i].tei));
            sysbus_connect_irq(busdev, 2,
                               qdev_get_gpio_in(armv7m, irqnum[i].rxi));
        }
    }
    // dlog("OUT");
}

static uint64_t frbef = 0;
/**
 * 0x407E0000 - 0x40800000
 */
static uint64_t ra2l1_flash_io_read(void *opaque, hwaddr addr, unsigned size) {
    uint64_t value = 0;
    uint64_t a_addr = addr | 0x407E0000;
    switch (a_addr) {
    case (uint64_t)&R_FACI_LP->FSTATR1:
        //            if (frbef != a_addr) {
        //                g_print("lp_fstatr1:%lx\n", lp_fstatr1);
        //            }
        value = lp_fstatr1 ^= 0x40;
        break;
    }
    frbef = a_addr;
    return value;
}

/**
 * 0x407E0000 - 0x40800000
 */
static void ra2l1_flash_io_write(void *opaque, hwaddr addr, uint64_t value,
                                 unsigned size) {
    uint64_t a_addr = addr | 0x40000000;
    //    g_print("fwrite %lx\n", addr);
    switch (a_addr) {
    case (uint64_t)&R_FACI_LP->FCR:
        //        g_print("%lx <- %lx\n", a_addr, value);
        if (value)
            lp_fstatr1 = 0x40;
        else
            lp_fstatr1 = 0;
        break;
    }
}

static uint64_t mosccr = 1;
static uint64_t can_str = 0x100;
static uint64_t rtc_rcr2 = 0x40;
static hwaddr bef;
/**
 * 0x40000000 - 0x40100000
 */
static uint64_t ra2l1_mmio_read(void *opaque, hwaddr addr, unsigned size) {
    RA2L1State *s = opaque;
    uint64_t a_addr = addr + 0x40000000;
    uint64_t value = 0;
    // if (bef != a_addr) {
        // g_print("read %lx\n", a_addr);
        // bef = a_addr;
    // }
    if (is_aes(a_addr)) {
        value = ra2l1_mmio_aes_read(opaque, a_addr, size);
        goto end;
    }
    switch (a_addr) {
    case (uint64_t)&R_SYSTEM->OSCSF:
        // g_print("OSCSF\n");
        value = 9;
        break;
    case (uint64_t)&R_SYSTEM->MOSCCR:
        // g_print("MOSCCR\n");
        value = mosccr;
        break;
    case (uint64_t)&R_SYSTEM->SCKDIVCR:
        value = 0x00000104;
        break;
    case (uint64_t)&R_CAN0->STR:
        // g_print("read can str %lx\n", can_str);
        value = can_str;
        break;
    case (uint64_t)&R_RTC->RCR2:
        value = rtc_rcr2;
        break;
    case (uint64_t)&R_SCI0->RDR:
        if (uart_rindex < uart_rlen) {
            value = uart_rbuf[uart_rindex++];
            if (uart_rindex == uart_rlen) {
                uart_rlen = 0;
                uart_rindex = 0;
                qemu_set_irq(s->uart[0].irq_rxi, 0);
            }
        } else {
            uart_rlen = 0;
            uart_rindex = 0;
            qemu_set_irq(s->uart[0].irq_rxi, 0);
        }
        break;
    }
end:
    return value;
}

/**
 * 0x40000000 - 0x40100000
 */
static void ra2l1_mmio_write(void *opaque, hwaddr addr, uint64_t value,
                             unsigned size) {
    uint64_t a_addr = addr + 0x40000000;
    if (is_aes(a_addr)) {
        ra2l1_mmio_aes_write(opaque, a_addr, value, size);
        return;
    }

    switch (a_addr) {
    case (uint64_t)&R_SYSTEM->MOSCCR:
        mosccr = value;
        break;
    case (uint64_t)&R_SYSTEM->SCKDIVCR:
        break;
    case (uint64_t)&R_CAN0->CTLR:
        // g_print("can0 ctlr %lx\n", value);
        can_str = (value & 0x300);
        break;
    case (uint64_t)&R_RTC->RCR2:
        rtc_rcr2 = (rtc_rcr2 & 0xfffffffe) | (value & 1);
        break;
    case (uint64_t)&R_SCI0->TDR:
        uart_write(opaque, 0, value & 0xff);
        break;
    case (uint64_t)&R_SCI9->TDR:
        uart_write(opaque, 9, value & 0xff);
        break;
    }
}

static void rchomp(char *s) {
    int i;
    int len = strlen(s);
    int found = -1;
    for (i = len - 1; i; i--) {
        switch (s[i]) {
        case '\r':
        case '\n':
            break;
        default:
            found = i;
        }
        if (found != -1)
            break;
    }

    if (found == -1)
        return;

    s[found + 1] = '\n';
    s[found + 2] = '\0';
}


static void uart_receive(char *s, int n) {
    uint16_t st = uart_rlen;
    memcpy(&uart_rbuf[uart_rlen], s, n);
    if (n) {
        uart_rlen += n;
        uart_rbuf[uart_rlen] = '\0';
        if (uart_rbuf[uart_rlen - 1] == 0xa) {
            qemu_set_irq(g_state->uart[0].irq_rxi, 1);
        }
        if (n == 1 && s[0] < 0x20) {
        } else {
            g_print("uart received(%d):%d %s\n%s", n, st,
                    b2s((uint8_t *)&uart_rbuf[st], n), &uart_rbuf[st]);
        }
    }
}
static void uart_write(void *opaque, int kind, uint8_t data) {
    RA2L1State *s = opaque;

    // g_print("%p k:%d %c\n", opaque, kind, data);
    uart_wbuf[kind][uart_windex[kind]++] = data;
    if (data == '\n') {
        uart_wbuf[kind][uart_windex[kind]++] = '\0';
        if (kind == 0) {
            im920_write((char *)uart_wbuf[0], strlen((char *)uart_wbuf[0]));
            g_print("to 920 %s", uart_wbuf[kind]);
        } else if (kind == 9) {
            g_print("%s", uart_wbuf[kind]);
        }
        uart_windex[kind] = 0;
    }
    qemu_irq_pulse(s->uart[kind].irq_tei);
}

static Property ra2l1_uart_properties[] = {
    DEFINE_PROP_CHR("chardev", RA2L1UartState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void ra2l1_update_irq(RA2L1UartState *s) {}

static int ra2l1_uart_can_receive(void *opaque) {
    RA2L1UartState *s = opaque;

    return 0;
}
static void ra2l1_uart_receive(void *opaque, const uint8_t *buf, int size) {
    RA2L1UartState *s = opaque;

    ra2l1_update_irq(s);
}

static void ra2l1_uart_init(Object *obj) {
    RA2L1UartState *s = RA2L1_UART(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq_txi);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq_tei);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq_rxi);
}

static void ra2l1_uart_reset(DeviceState *dev) {}
static void ra2l1_uart_realize(DeviceState *dev, Error **errp) {
    RA2L1UartState *s = RA2L1_UART(dev);

    qemu_chr_fe_set_handlers(&s->chr, ra2l1_uart_can_receive,
                             ra2l1_uart_receive, NULL, NULL, s, NULL, true);
}

static void ra2l1_uart_class_init(ObjectClass *klass, void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = ra2l1_uart_reset;
    device_class_set_props(dc, ra2l1_uart_properties);
    dc->realize = ra2l1_uart_realize;
}

static const TypeInfo ra2l1_uart_info = {
    .name = TYPE_RA2L1_UART,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RA2L1UartState),
    .instance_init = ra2l1_uart_init,
    .class_init = ra2l1_uart_class_init,
};

static void ra2l1_uart_register_types(void) {
    type_register_static(&ra2l1_uart_info);
}

type_init(ra2l1_uart_register_types)

    static void ra2l1_soc_class_init(ObjectClass *klass, void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = ra2l1_soc_realize;
    /* No vmstate or reset required: device has no internal state */
}

static const TypeInfo ra2l1_soc_info = {
    .name = TYPE_RA2L1_SYS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RA2L1State),
    .instance_init = ra2l1_soc_initfn,
    .class_init = ra2l1_soc_class_init,
};

static void ra2l1_soc_types(void) { type_register_static(&ra2l1_soc_info); }

type_init(ra2l1_soc_types)

    static void ra2l1_init(MachineState *machine) {
    DeviceState *dev;
    Clock *sysclk;

    /* This clock doesn't need migration because it is fixed-frequency */
    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FRQ);

    dev = qdev_new(TYPE_RA2L1_SYS);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(dev));
    qdev_connect_clock_in(dev, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename, 0,
                       0x20000);
    im920_init(uart_receive);
}

static void ra2l1_machine_init(MachineClass *mc) {
    static const char *const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m23"), ARM_CPU_TYPE_NAME("cortex-m33"), NULL};

    mc->desc = "Renesas RA(Cortex-M23)";
    mc->init = ra2l1_init;
    mc->valid_cpu_types = valid_cpu_types;
}
#pragma GCC diagnostic pop

DEFINE_MACHINE("ra2l1", ra2l1_machine_init)
