#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/datadir.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/arm/armv7m.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"

#define NUM_IRQ_LINES 32
#define NUM_PRIO_BITS 3

#define TYPE_RA2L1_SYS "ra2l1-sys"
OBJECT_DECLARE_SIMPLE_TYPE(ra_state, RA2L1_SYS)

struct ra_state {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    Clock *sysclk;
};

static void ra2l1_init(MachineState *machine) {
    char *sysboot_filename;
    Object *soc_container;
    DeviceState *ra_dev,*nvic;
    MemoryRegion *sysmem;
    MemoryRegion *sram;
    MemoryRegion *pflash;
    MemoryRegion *dflash;

    soc_container = object_new("container");
    object_property_add_child(OBJECT(machine), "soc", soc_container);

    sysmem = get_system_memory();
    sram = g_new(MemoryRegion, 1);
    pflash = g_new(MemoryRegion, 1);
    dflash = g_new(MemoryRegion, 1);

    memory_region_init_ram(sram, NULL, "ra2l1.sram", 0x0020000, &error_fatal);
    memory_region_init_rom(pflash, NULL, "ra2l1.pflash", 0x0008000, &error_fatal);
    memory_region_init_ram(dflash, NULL, "ra2l1.dflash", 0x0100000, &error_fatal);

    memory_region_add_subregion(sysmem, 0, pflash);
    memory_region_add_subregion(sysmem, 0x20000000, sram);
    memory_region_add_subregion(sysmem, 0x40100000, dflash);

    ra_dev = qdev_new(TYPE_RA2L1_SYS);
    object_property_add_child(soc_container, "sys", OBJECT(ra_dev));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(ra_dev), &error_fatal);

    nvic = qdev_new(TYPE_ARMV7M);
    object_property_add_child(soc_container, "v7m", OBJECT(nvic));
    qdev_prop_set_uint32(nvic, "num-irq", NUM_IRQ_LINES);
    qdev_prop_set_uint8(nvic, "num-prio-bits", NUM_PRIO_BITS);
    qdev_prop_set_string(nvic, "cpu-type", machine->cpu_type);
    qdev_prop_set_bit(nvic, "enable-bitband", true);
    qdev_connect_clock_in(nvic, "cpuclk",
                          qdev_get_clock_out(ra_dev, "SYSCLK"));
    /* This SoC does not connect the systick reference clock */
    object_property_set_link(OBJECT(nvic), "memory",
                             OBJECT(get_system_memory()), &error_abort);
    /* This will exit with an error if the user passed us a bad cpu_type */
    sysbus_realize_and_unref(SYS_BUS_DEVICE(nvic), &error_fatal);

    if (machine->firmware) {
        sysboot_filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, machine->firmware);
        if (sysboot_filename != NULL) {
            if (load_image_targphys(sysboot_filename, 0, 0x10000) < 0) {
                error_report("%s load failed", sysboot_filename);
                exit(1);
            }
            g_free(sysboot_filename);
        } else {
            error_report("%s load failed", sysboot_filename);
            exit(1);
        }
    }
}

static void r7fa2l1a_class_init(ObjectClass *oc, void *data) {
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Renesas RA2L1 (Cortex-M23)";
    mc->init = ra2l1_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m33");
}

static uint64_t ra_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void ra_write(void *opaque, hwaddr offset,
                       uint64_t value, unsigned size) {
}


static const MemoryRegionOps ra_ops = {
    .read = ra_read,
    .write = ra_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const TypeInfo rf7fa_type = {
    .name = MACHINE_TYPE_NAME("ra2l1"),
    .parent = TYPE_MACHINE,
    .class_init = r7fa2l1a_class_init,
};

static void ra2l1_machine_init(void)
{
    type_register_static(&rf7fa_type);
}

type_init(ra2l1_machine_init)

static void ra_sys_instance_init(Object *obj) {
    ra_state *s = RA2L1_SYS(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);

    memory_region_init_io(&s->iomem, obj, &ra_ops, s, "mmio", 0x100000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    s->sysclk = qdev_init_clock_out(DEVICE(s), "SYSCLK");
}

static void ra_sys_class_init(ObjectClass *klass, void *data) {
}

static const TypeInfo ra2l1_sys_info = {
    .name = TYPE_RA2L1_SYS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ra_state),
    .instance_init = ra_sys_instance_init,
    .class_init = ra_sys_class_init,
};

static void ra2l1_register_types(void) {
    type_register_static(&ra2l1_sys_info);
}

type_init(ra2l1_register_types)
