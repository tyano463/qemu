#ifndef __RA2L1_GPIO_H__
#define __RA2L1_GPIO_H__

#include "qemu/osdep.h"

#include "exec/hwaddr.h"

bool is_gpio(hwaddr addr);
void gpio_monitor_launch(void);
uint64_t ra2l1_gpio_read(void *opaque, hwaddr addr, unsigned size);

void ra2l1_gpio_write(void *opaque, hwaddr addr, uint64_t value, unsigned size);
#endif