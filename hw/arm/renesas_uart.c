#include "qemu/osdep.h"

#include "chardev/char.h"
#include "hw/irq.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties-system.h"
#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"

#include "R7FA2L1AB.h"
#include "ra2l1.h"
#include "renesas_common.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-label"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define DEVPATH "/dev/ttyQEMU"
#define DEV_MAJ_VER 511
#define MAX_MIN_VER (RA2L1_UART_NUM - 1)
#define MAX_BUF_SIZE 0x400

#define TYPE_RA2L1_UART "ra2l1-uart"

OBJECT_DECLARE_SIMPLE_TYPE(ra_uart, RA2L1_UART)

typedef struct str_uart_buf
{
    uint16_t st;
    uint16_t cur;
    uint8_t *buf;
} uart_buf_t;

// static void virtual_uart_cleanup(Notifier *n, void *opaque);
static void *local_uart_receive_main(void *arg);
static void local_uart_write(RA2L1UartState *uart, const char *str, size_t size);
static void local_uart_receive(int channel, const char *buf, size_t len);

static pthread_mutex_t mutex[RA2L1_UART_NUM];
static uint64_t scr[RA2L1_UART_NUM];
static uart_buf_t *uart_rbuf;

static uint64_t renesas_uart_mmio_read(void *opaque, hwaddr _addr, unsigned size)
{
    hwaddr addr = _addr + (intptr_t)R_SCI0;
    RA2L1UartState *uart = (RA2L1UartState *)opaque;
    int channel = uart->channel;
    uint64_t value = 0;
    uart_buf_t *p = &uart_rbuf[channel];

    switch (addr)
    {
    case (uint64_t)&R_SCI0->RDR:
        pthread_mutex_lock(&mutex[channel]);
        if (p->st != p->cur)
        {
            value = p->buf[p->st++];
            p->st %= MAX_BUF_SIZE;
        }

        if (p->st == p->cur)
        {
            qemu_set_irq(uart->irq_rxi, 0);
            dlog("channel:%d irq_rxi off", channel);
        }
        pthread_mutex_unlock(&mutex[channel]);
        break;
    }
    return value;
}

static void renesas_uart_mmio_write(void *opaque, hwaddr _addr, uint64_t value, unsigned size)
{
    RA2L1UartState *uart = (RA2L1UartState *)opaque;
    hwaddr addr = _addr + (intptr_t)R_SCI0;
    int channel = uart->channel;
    static uint8_t data;

    // dlog("ch:%d %lx <- %lx", channel,
    //  (hwaddr)_addr + ((hwaddr)R_SCI1_BASE - (hwaddr)R_SCI0_BASE) * channel, value);
    switch (addr)
    {
    case (uint64_t)&R_SCI0->TDR:
        data = (uint8_t)(value & 0xff);
#if RENESAS_LOCAL_UART
        local_uart_write(uart, (const char *)&data, 1);
#endif
        if (scr[channel] & SCI_SCR_TIE_MASK)
        {
            dlog("channel:%d irq_txi %02X", uart->channel, data);
            // qemu_irq_pulse(uart->irq_txi);
            qemu_set_irq(uart->irq_txi, 1);
        }
        break;
    case (uint64_t)&R_SCI0->SCR:
        if ((scr[channel] & SCI_SCR_TIE_MASK) && (!(value & SCI_SCR_TIE_MASK)))
        {
            dlog("channel:%d irq_tei", uart->channel);
            qemu_set_irq(uart->irq_txi, 0);
            qemu_irq_pulse(uart->irq_tei);
        }
        scr[channel] = value;
        break;
    }
}

MemoryRegionOps ops = {
    .read = renesas_uart_mmio_read,
    .write = renesas_uart_mmio_write,
};

static RA2L1UartState *renesas_uart_device_init(MemoryRegion *system_memory, RA2L1State *s,
                                                DeviceState *dev_soc, hwaddr base_addr,
                                                int channel)
{
    RA2L1UartState *uart = NULL;
    DeviceState *dev;
    Error *errp = NULL;
    SysBusDevice *busdev;
    DeviceState *armv7m;
    bool ret;
    struct
    {
        int txi;
        int tei;
        int rxi;
        // } irqnum[] = {{1, 2, 0}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {5, 6, 4}};
    } irqnum[] = {{1, 2, 0}, {-1}, {5, 6, 4}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}};

    ERR_RET((channel < 0 || channel > MAX_MIN_VER), "invalid uart channel %d", channel);
    object_initialize_child(OBJECT(s), "uart[*]", &s->uart[channel], TYPE_RA2L1_UART);

    uart = &s->uart[channel];
    uart->channel = channel;
    armv7m = DEVICE(&s->armv7m);
    dev = DEVICE(uart);
    qdev_prop_set_chr(dev, "chardev", serial_hd(channel));
    ret = sysbus_realize(SYS_BUS_DEVICE(uart), &errp);
    ERR_RET(!ret, "uart[%d] realize failed\n", channel);

    busdev = SYS_BUS_DEVICE(dev);

    if (irqnum[channel].txi != -1)
    {
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, irqnum[channel].txi));
        sysbus_connect_irq(busdev, 1, qdev_get_gpio_in(armv7m, irqnum[channel].tei));
        sysbus_connect_irq(busdev, 2, qdev_get_gpio_in(armv7m, irqnum[channel].rxi));

        dlog("txi:%p tei:%p rxi:%p", ((irq_t *)uart->irq_txi)->handler,
             ((irq_t *)uart->irq_tei)->handler, ((irq_t *)uart->irq_rxi)->handler);
    }

    size_t reg_size = (((intptr_t)R_SCI1) - ((intptr_t)R_SCI0));
    memory_region_init_io(&uart->mmio, OBJECT(dev_soc), &ops, uart, "renesas.uart", reg_size);
    memory_region_add_subregion(system_memory, base_addr, &uart->mmio);
    // dlog("added register %lx(+%lx)", base_addr, reg_size);

error_return:
    return uart;
}

int local_uart_init(MemoryRegion *system_memory, RA2L1State *s, DeviceState *dev_soc,
                    hwaddr base_addr, int channel)
{
    int ret = -1;
    char device_name[64];
    RA2L1UartState *p;

    p = renesas_uart_device_init(system_memory, s, dev_soc, base_addr, channel);
    ERR_RET(!p, "renesas_uart_device_init failed");
    ret = 0;

    if (!uart_rbuf)
    {
        uart_rbuf = malloc(sizeof(uart_buf_t) * RA2L1_UART_NUM + MAX_BUF_SIZE * RA2L1_UART_NUM);
    }

    for (int i = 0; i < RA2L1_UART_NUM; i++)
    {
        uart_rbuf[i].buf = ((uint8_t *)&uart_rbuf[RA2L1_UART_NUM]) + (MAX_BUF_SIZE * i);
        uart_rbuf[i].buf[0] = '\0';
        uart_rbuf[i].cur = 0;
        uart_rbuf[i].st = 0;
    }

#if RENESAS_LOCAL_UART
    sprintf(device_name, DEVPATH "%d", channel);
    // ret = mknod(device_name, S_IFCHR | 0666, makedev(DEV_MAJ_VER, channel));
    // ERR_RET(ret, "mknod failed");

    // s->shutdown_notifier.notify = virtual_uart_cleanup;
    // qemu_register_shutdown_notifier(&s->shutdown_notifier);

    while (1)
    {
        int fd = open(device_name, O_RDWR);
        if (fd >= 0)
        {
            close(fd);
            break;
        }
        usleep(10000);
    }

    p->fd = open(device_name, O_RDWR);
    ERR_RET(p->fd < 0, "%s open failed %d", device_name, channel);

    p->callback = local_uart_receive;
    p->running = true;
    p->channel = channel;

    pthread_mutex_init(&mutex[channel], NULL);
    ret = pthread_create(&p->th, NULL, local_uart_receive_main, (void *)p);
    ERR_RET(ret != 0, "pthread_create error");

#endif

error_return:
    return ret;
}

// static void virtual_uart_cleanup(Notifier *n, void *opaque)
// {
//     remove(DEVPATH);
// }

static void local_uart_write(RA2L1UartState *uart, const char *str, size_t size)
{
    static char buf[256];
    memcpy(buf, str, size);
    if (uart->fd >= 0)
    {
        write(uart->fd, str, size);
        // buf[size] = '\0';
        // dlog("%s", buf);
    }
    return;
}

static void local_uart_receive(int channel, const char *buf, size_t len)
{
    static char temp_buf[256];
    if (sizeof(temp_buf) <= len)
        return;
    memcpy(temp_buf, buf, len);
    temp_buf[len] = '\0';
    dlog("ch:%d receive:%s", channel, temp_buf);
}

static void to_rbuf(int channel, const char *buf, size_t len)
{
    // dlog("IN len:%ld", len);
    uart_buf_t *p = &uart_rbuf[channel];
    if (p->cur + len < MAX_BUF_SIZE)
    {
        // dlog("short");
        memcpy(&p->buf[p->cur], buf, len + 1);
        p->cur += len;
        p->buf[p->cur] = '\0';
    }
    else
    {
        // dlog("loop");
        int copied = p->cur + len - MAX_BUF_SIZE - 1;
        memcpy(&p->buf[p->cur], buf, copied);
        memcpy(p->buf, &buf[copied], len - copied);
        p->buf[len - copied] = '\0';
        p->cur = len - copied;
    }
}

static void *local_uart_receive_main(void *arg)
{

    char buf[MAX_BUF_SIZE];
    RA2L1UartState *p = (RA2L1UartState *)arg;
    int channel = p->channel;

    ERR_RETn(channel < 0 || channel > MAX_MIN_VER);

    dlog("uart receiver launched %p ch:%d fd:%d tid:%lx", p, p->channel, p->fd, pthread_self());

    while (p->running)
    {
        ssize_t len = read(p->fd, buf, MAX_BUF_SIZE);
        ERR_RET(len < 0, "uart read error");
        if (!len)
            continue;

        // dlog("received st");
        to_rbuf(channel, buf, len);

        int start = 0;
        buf[len] = '\0';
        dlog("ch:%d fd:%d (%lu) buf:%s", channel, p->fd, len, buf);
        for (int i = start; i < len; i++)
        {
            if (buf[i] == ASCII_CODE_CR || buf[i] == ASCII_CODE_LF)
            {
                buf[i] = '\n';
                if ((p->rx_pos + i) > start)
                {
                    memcpy(&p->rx_buf[p->rx_pos], buf, i + 1);
                    if (p->callback)
                        p->callback(channel, p->rx_buf, p->rx_pos + i + 1);
                    p->rx_pos = 0;
                    start = i + 1;
                }
            }
        }
        if (start < len)
        {
            memcpy(&p->rx_buf[p->rx_pos], &buf[start], len - start);
            p->rx_pos += len - start;
        }
        qemu_set_irq(p->irq_rxi, 1);
        dlog("ch:%d received irq_rxi on", channel);
    }
error_return:
    return NULL;
}

static Property ra2l1_uart_properties[] = {
    DEFINE_PROP_CHR("chardev", RA2L1UartState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void ra2l1_update_irq(RA2L1UartState *s)
{
}

static int ra2l1_uart_can_receive(void *opaque)
{
    RA2L1UartState *s = opaque;
    (void)s;
    return 0;
}

static void ra2l1_uart_receive(void *opaque, const uint8_t *buf, int size)
{
    RA2L1UartState *s = opaque;

    ra2l1_update_irq(s);
}

static void ra2l1_uart_init(Object *obj)
{
    RA2L1UartState *s = RA2L1_UART(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq_txi);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq_tei);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq_rxi);
}

static void ra2l1_uart_reset(DeviceState *dev)
{
}

static void ra2l1_uart_realize(DeviceState *dev, Error **errp)
{
    RA2L1UartState *s = RA2L1_UART(dev);

    qemu_chr_fe_set_handlers(&s->chr, ra2l1_uart_can_receive, ra2l1_uart_receive, NULL, NULL, s,
                             NULL, true);
}

static void ra2l1_uart_class_init(ObjectClass *klass, void *data)
{
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

static void ra2l1_uart_register_types(void)
{
    type_register_static(&ra2l1_uart_info);
}

type_init(ra2l1_uart_register_types)

#pragma GCC diagnostic pop