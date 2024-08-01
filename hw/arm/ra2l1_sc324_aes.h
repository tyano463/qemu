#ifndef __RA2L1_SC324_AES_H__
#define __RA2L1_SC324_AES_H__

#include "qemu/osdep.h"
#include "exec/hwaddr.h"

#define BLOCK_LENGTH 16

typedef enum e_hw_sc324_aes_modes {
    SC324_AES_ECB = 0,
    SC324_AES_CBC = 1,
    SC324_AES_CTR = 2,
} hw_sc324_aes_modes_t;

typedef enum e_hw_sc324_aes_encrypt_flag
{
    SC324_AES_ENCRYPT = 0,
    SC324_AES_DECRYPT = 1
} hw_sc324_aes_encrypt_flag_t;

bool is_aes(hwaddr addr);
uint64_t ra2l1_mmio_aes_read(void *opaque, hwaddr addr, unsigned size);
void ra2l1_mmio_aes_write(void *opaque, hwaddr addr, uint64_t value,
                          unsigned size);
#endif
