#include "qemu/osdep.h"

#include "R7FA2L1AB.h"
#include "ra2l1.h"
#include "ra2l1_gpt.h"

#include "renesas_common.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-label"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define TYPE_RA2L1_GPT "ra2l1-gpt"

OBJECT_DECLARE_SIMPLE_TYPE(ra_gpt, RA2L1_GPT)

static uint64_t ra2l1_gpt_mmio_read(void *opaque, hwaddr _addr, unsigned size);
static void ra2l1_gpt_mmio_write(void *opaque, hwaddr _addr, uint64_t value, unsigned size);

static MemoryRegionOps ops = {
    .read = ra2l1_gpt_mmio_read,
    .write = ra2l1_gpt_mmio_write,
};

static uint64_t ra2l1_gpt_mmio_read(void *opaque, hwaddr _addr, unsigned size)
{
    uint64_t value = 0;
    return value;
}

static void ra2l1_gpt_mmio_write(void *opaque, hwaddr _addr, uint64_t value, unsigned size)
{
}

static void ra2l1_gpt_init(Object *obj)
{
    RA2L1GPTState *s = RA2L1_GPT(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
}

RA2L1GPTState *ra2l1_gpt_add(MemoryRegion *system_memory, RA2L1State *s, DeviceState *dev_soc, int channel)
{
    RA2L1GPTState *p = NULL;
    hwaddr base_addr;
    hwaddr reg_size;
    Error *errp = NULL;
    bool ret;

    p = g_new0(RA2L1GPTState, 1);
    object_initialize_child(OBJECT(s), "gpt[*]", p, TYPE_RA2L1_GPT);

    reg_size = ((hwaddr)R_GPT1 - (hwaddr)R_GPT0);
    base_addr = ((hwaddr)R_GPT0) + (reg_size * channel);

    ret = sysbus_realize(SYS_BUS_DEVICE(p), &errp);
    ERR_RET(!ret, "gpt[%d] realize failed\n", channel);

    memory_region_init_io(&p->mmio, OBJECT(dev_soc), &ops, p, "renesas.gpt", reg_size);
    memory_region_add_subregion(system_memory, base_addr, &p->mmio);

    p->opaque = s;
    dlog("gpt %d initialized %p", channel, p);

error_return:

    return p;
}

static void ra2l1_gpt_reset(DeviceState *dev)
{
}

static void ra2l1_gpt_realize(DeviceState *dev, Error **errp)
{
}

static void ra2l1_gpt_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = ra2l1_gpt_reset;
    dc->realize = ra2l1_gpt_realize;
}

static const TypeInfo ra2l1_gpt_info = {
    .name = TYPE_RA2L1_GPT,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RA2L1GPTState),
    .instance_init = ra2l1_gpt_init,
    .class_init = ra2l1_gpt_class_init,
};

static void ra2l1_gpt_register_types(void)
{
    type_register_static(&ra2l1_gpt_info);
}

type_init(ra2l1_gpt_register_types)

#pragma GCC diagnostic pop