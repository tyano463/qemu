#ifndef __RA2L1_AGT_H__
#define __RA2L1_AGT_H__

#include "exec/hwaddr.h"
#include "qemu/osdep.h"

#define AGT_PRV_AGTCR_FORCE_STOP                (0xF4U)
#define AGT_PRV_AGTCR_FORCE_STOP_CLEAR_FLAGS    (0x4U)
#define AGT_PRV_AGTCR_STATUS_FLAGS              (0xF0U)
#define AGT_PRV_AGTCR_STOP_TIMER                (0xF0U)
#define AGT_PRV_AGTCR_START_TIMER               (0xF1U)


bool is_agt(hwaddr addr);

uint64_t ra2l1_mmio_agt_read(void *opaque, hwaddr addr, unsigned size);
void ra2l1_mmio_agt_write(void *opaque, hwaddr addr, uint64_t value,
                          unsigned size);
#endif