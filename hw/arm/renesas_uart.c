#include "qemu/osdep.h"

#include "chardev/char.h"
#include "sysemu/runstate.h"

#include "ra2l1.h"
#include "renesas_common.h"

#define DEVPATH "/dev/ttyQEMU"
#define DEV_MAJ_VER 511
#define MAX_MIN_VER (RA2L1_UART_NUM - 1)
#define MAX_BUF_SIZE 256

typedef struct str_renesas_uart {
    pthread_t th;
    int fd;
    int running;
    void (*callback)(int channel, const char *s, ssize_t n);
    char rx_buf[0x400];
    size_t rx_pos;
} renesas_uart_t;

static void virtual_uart_cleanup(Notifier *n, void *opaque);
static void *local_uart_receive_main(void *arg);

renesas_uart_t uart[RA2L1_UART_NUM];

int local_uart_init(RA2L1State *s, int channel,
                    void (*cb)(int channel, const char *s, ssize_t n)) {
    int ret = -1;
    char device_name[64];
    renesas_uart_t *p;

    ERR_RET((channel < 0 || channel > MAX_MIN_VER), "invalid uart channel %d",
            channel);
    p = &uart[channel];
    sprintf(device_name, DEVPATH "%d", channel);

    ret = mknod(device_name, S_IFCHR | 0666, makedev(DEV_MAJ_VER, channel));
    ERR_RET(ret, "mknod failed");

    s->shutdown_notifier.notify = virtual_uart_cleanup;
    qemu_register_shutdown_notifier(&s->shutdown_notifier);

    p->fd = open(device_name, O_RDWR);
    ERR_RETn(p->fd < 0);

    p->callback = cb;
    p->running = true;
    ret = pthread_create(&p->th, NULL, local_uart_receive_main,
                         (void *)(intptr_t)channel);
    ERR_RET(ret != 0, "pthread_create error");

error_return:
    return ret;
}

static void virtual_uart_cleanup(Notifier *n, void *opaque) { remove(DEVPATH); }

void local_uart_write(int channel, const char *s, size_t size) {
    renesas_uart_t *p;
    ERR_RETn(channel < 0 || channel > MAX_MIN_VER);

    p = &uart[channel];
    if (p->fd >= 0)
        write(p->fd, s, size);
error_return:
    return;
}

static void *local_uart_receive_main(void *arg) {
    int channel = (int)(intptr_t)arg;
    static char buf[MAX_BUF_SIZE];
    static renesas_uart_t *p;

    ERR_RETn(channel < 0 || channel > MAX_MIN_VER);
    p = &uart[channel];

    while (p->running) {
        ssize_t len = read(p->fd, buf, MAX_BUF_SIZE);
        ERR_RET(len < 0, "uart read error");

        if (!len)
            continue;

        int start = 0;
        buf[len] = '\0';
        // g_print("buf:%s\n", buf);
        for (int i = start; i < len; i++) {
            if (buf[i] == ASCII_CODE_CR || buf[i] == ASCII_CODE_LF) {
                buf[i] = '\n';
                if ((p->rx_pos + i) > start) {
                    memcpy(&p->rx_buf[p->rx_pos], buf, i + 1);
                    if (p->callback)
                        p->callback(channel, p->rx_buf, p->rx_pos + i + 1);
                    p->rx_pos = 0;
                    start = i + 1;
                }
            }
        }
        if (start < len) {
            memcpy(&p->rx_buf[p->rx_pos], &buf[start], len - start);
            p->rx_pos += len - start;
        }
    }
error_return:
    return NULL;
}