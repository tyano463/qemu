#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/datadir.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"

#define NUM_IRQ_LINES 32
#define NUM_PRIO_BITS 3
#define SYSCLK_FRQ 24000000ULL

#define TYPE_RA2L1_SYS "ra2l1-sys"
OBJECT_DECLARE_SIMPLE_TYPE(ra_state, RA2L1_SYS)

typedef struct ra_state {
    SysBusDevice parent_obj;
    ARMv7MState armv7m;
    MemoryRegion iomem;
    MemoryRegion sram;
    MemoryRegion flash;
    MemoryRegion flash_alias;
    qemu_irq irq;
    Clock *sysclk;
    Clock *refclk;
}RA2L1State;

static void ra2l1_soc_initfn(Object *obj)
{   
    RA2L1State *s = RA2L1_SYS(obj);  
    
    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);
    
    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
    s->refclk = qdev_init_clock_in(DEVICE(s), "refclk", NULL, NULL, 0);
}   

static void ra2l1_soc_realize(DeviceState *dev_soc, Error **errp)
{
    RA2L1State *s = RA2L1_SYS(dev_soc);
    DeviceState *armv7m;
    
    MemoryRegion *system_memory = get_system_memory();

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

    memory_region_init_rom(&s->flash, OBJECT(dev_soc), "ra2l1.flash",
                           0x20000, &error_fatal);
    memory_region_init_alias(&s->flash_alias, OBJECT(dev_soc),
                             "ra2l1.flash.alias", &s->flash, 0, 0x20000);
    memory_region_add_subregion(system_memory, 0, &s->flash);
    memory_region_add_subregion(system_memory, 0, &s->flash_alias);

    /* Init SRAM region */
    memory_region_init_ram(&s->sram, NULL, "ra2l1.sram", 0x8000,
                           &error_fatal);
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
}

static void ra2l1_soc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = ra2l1_soc_realize;
    /* No vmstate or reset required: device has no internal state */
}

static const TypeInfo ra2l1_soc_info = {
    .name          = TYPE_RA2L1_SYS,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RA2L1State),
    .instance_init = ra2l1_soc_initfn,
    .class_init    = ra2l1_soc_class_init,
};

static void ra2l1_soc_types(void)
{
    type_register_static(&ra2l1_soc_info);
}

type_init(ra2l1_soc_types)

static void ra2l1_init(MachineState *machine)
{
    DeviceState *dev;
    Clock *sysclk;
    
    /* This clock doesn't need migration because it is fixed-frequency */
    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FRQ);

    dev = qdev_new(TYPE_RA2L1_SYS);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(dev));
    qdev_connect_clock_in(dev, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    armv7m_load_kernel(ARM_CPU(first_cpu),
                       machine->kernel_filename,
                       0, 0x20000);
}

static void ra2l1_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = { 
        ARM_CPU_TYPE_NAME("cortex-m23"),
        ARM_CPU_TYPE_NAME("cortex-m33"),
        NULL
    };  

    mc->desc = "Renesas RA(Cortex-M23)";
    mc->init = ra2l1_init;
    mc->valid_cpu_types = valid_cpu_types;
}

DEFINE_MACHINE("ra2l1", ra2l1_machine_init)
