#include "qemu/osdep.h"
#include <dlfcn.h>

#include "ra2l1_gpio.h"

#include "./gpio_gui_lib/x11_gpio.h"
#include "R7FA2L1AB.h"
#include "renesas_common.h"

#define GPIO_GUI_LIB_DIR "gpio_gui_lib"
#define MONITOR_COMMAND "run_gui"
#define LIB_GPIO_VAR "renesas_gpio"
#define LIB_MONITOR_LAUNCHED "monitor_launched"
#define LIBRARY_FILE "./libx11_gpio.so"

#define GPIO_PORT_NUM 9

#define THREAD_WAIT_TIMEOUT 1
#define TO_MICROSEC(s) (s * 1000000)
#define THREAD_WAIT_TIMEOUT_MS TO_MICROSEC(THREAD_WAIT_TIMEOUT)
#define THREAD_CHECK_PERIOD_MS 1000

typedef enum {
    PCNTR1,
    PCNTR2,
    PCNTR3,
    PCNTR4,
    INVAL = -1,
} RegAddr;

typedef struct {
    int port;
    hwaddr addr;
    RegAddr reg;
} gpio_reg_t;

typedef void (*gui_func_t)(void);

static renesas_gpio_t *gpio;
static bool (*launched)(void);

bool is_gpio(hwaddr addr) {
    hwaddr reg_size = (hwaddr)R_PORT1 - (hwaddr)R_PORT0;
    return ((hwaddr)R_PORT0 < addr) && (addr < ((hwaddr)R_PORT8) + reg_size);
}

static void *launch(void *arg) {
    void *handle = dlopen(LIBRARY_FILE, RTLD_LAZY);
    ERR_RET(!handle, "%s", dlerror());

    gui_func_t run_gui = (gui_func_t)dlsym(handle, MONITOR_COMMAND);
    ERR_RET(!run_gui, "%s", dlerror());

    renesas_gpio_t **_gpio = (renesas_gpio_t **)dlsym(handle, LIB_GPIO_VAR);
    ERR_RET(!_gpio, "%s", dlerror());
    gpio = *_gpio;

    launched = (bool (*)(void))dlsym(handle, LIB_MONITOR_LAUNCHED);
    ERR_RET(!launched, "%s", dlerror());

    dlog("launch gui");
    run_gui();
    dlog("gui fin");

error_return:
    if (handle)
        dlclose(handle);
    return NULL;
}

void gpio_monitor_launch(void) {
    pthread_t th;
    int ret = pthread_create(&th, NULL, launch, NULL);
    ERR_RET(ret, "thread create failed");

    for (int i = 0; i < THREAD_WAIT_TIMEOUT_MS / THREAD_CHECK_PERIOD_MS; i++) {
        if (launched)
            break;
        usleep(THREAD_CHECK_PERIOD_MS);
    }
    ERR_RET(!launched, "thread initialize failed");

    while (!launched())
        usleep(10000);
error_return:
    return;
}

static RegAddr get_register(hwaddr addr) {
    R_PORT0_Type *p = NULL;
    if (addr == (hwaddr)&p->PCNTR1)
        return PCNTR1;
    if (addr == (hwaddr)&p->PCNTR2)
        return PCNTR2;
    if (addr == (hwaddr)&p->PCNTR3)
        return PCNTR3;
    if (addr == (hwaddr)&p->PCNTR4)
        return PCNTR4;
    return INVAL;
}

static int addr_to_reg(gpio_reg_t *reg, hwaddr addr) {
    int ret = NG;
    size_t reg_size;
    hwaddr gpio_addr;

    reg_size = ((hwaddr)R_PORT1_BASE - (hwaddr)R_PORT0_BASE);
    gpio_addr = addr - (hwaddr)R_PORT0_BASE;

    reg->port = gpio_addr / reg_size;
    ERR_RETn(reg->port >= GPIO_PORT_NUM);

    reg->addr = gpio_addr % reg_size;
    ERR_RETn(reg->addr >= reg_size);

    reg->reg = get_register(reg->addr);
    ERR_RETn(reg->reg == INVAL);

    ret = OK;
error_return:
    return ret;
}

uint64_t ra2l1_gpio_read(void *opaque, hwaddr addr, unsigned size) {
    uint64_t value = 0;
    int ret;
    gpio_reg_t reg;

    dlog("addr:%lx sz:%d", addr, size);
    ret = addr_to_reg(&reg, addr);
    ERR_RETn(ret != OK);

    switch (reg.reg) {
    case PCNTR1:
        // PDRとPODRの両方がある。
        value = (uint64_t)((((uint16_t)(gpio[reg.port].val & 0xffff)) << 16) |
                           ((uint16_t)((gpio[reg.port].pdr & 0xffff))));
        dlog("v:%lx", value);
        break;
    case PCNTR2:
        value = (uint64_t)((uint16_t)(gpio[reg.port].val & 0xffff));
        dlog("v:%lx", value);
        break;
    case PCNTR3:
        break;
    case PCNTR4:
        break;
    case INVAL:
        break;
    }
error_return:
    return value;
}

void ra2l1_gpio_write(void *opaque, hwaddr addr, uint64_t value,
                      unsigned size) {

    int ret;
    gpio_reg_t reg;
    uint16_t upper, lower;

    ret = addr_to_reg(&reg, addr);
    ERR_RETn(ret != OK);

    upper = (value & 0xffff0000) >> 16;
    lower = (value & 0x0000ffff) >> 0;

    switch (reg.reg) {
    case PCNTR1:
        gpio[reg.port].pdr = lower;
        gpio[reg.port].val = upper;
        break;
    case PCNTR2:
        gpio[reg.port].val = lower;
        break;
    case PCNTR3:
        break;
    case PCNTR4:
        break;
    case INVAL:
        break;
    }

    dlog("%lx <- %lx", addr, value);
error_return:
    return;
}