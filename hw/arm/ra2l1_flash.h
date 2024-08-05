#ifndef __RA2L1_FLASH_H__
#define __RA2L1_FLASH_H__

#include "exec/hwaddr.h"
#include "qemu/osdep.h"

/** from r_flash_lp.c of FSP */
#define FLASH_LP_DATAFLASH_READ_BASE_ADDR (0x40100000U)
#define FLASH_LP_DATAFLASH_WRITE_BASE_ADDR (0xFE000000U)
#define FLASH_LP_DATAFLASH_ADDR_OFFSET                                         \
    (FLASH_LP_DATAFLASH_WRITE_BASE_ADDR - FLASH_LP_DATAFLASH_READ_BASE_ADDR)

#define FLASH_LP_DF_START_ADDRESS (0x40100000)

/*  flash mode definition (FENTRYR Register setting)*/
#define FLASH_LP_FENTRYR_DATAFLASH_PE_MODE (0xAA80U)
#define FLASH_LP_FENTRYR_CODEFLASH_PE_MODE (0xAA01U)
#define FLASH_LP_FENTRYR_READ_MODE (0xAA00U)

/*  flash mode definition (FPMCR Register setting)*/
#define FLASH_LP_DATAFLASH_PE_MODE (0x10U)
#define FLASH_LP_READ_MODE (0x08U)
#define FLASH_LP_LVPE_MODE (0x40U)
#define FLASH_LP_DISCHARGE_1 (0x12U)
#define FLASH_LP_DISCHARGE_2 (0x92U)
#define FLASH_LP_CODEFLASH_PE_MODE (0x82U)

/*  operation definition (FCR Register setting)*/
#define FLASH_LP_FCR_WRITE (0x81U)
#define FLASH_LP_FCR_ERASE (0x84U)
#define FLASH_LP_FCR_BLANKCHECK (0x83U)
#define FLASH_LP_FCR_CLEAR (0x00U)

/*  operation definition (FEXCR Register setting)*/
#define FLASH_LP_FEXCR_STARTUP (0x81U)
#define FLASH_LP_FEXCR_AW (0x82U)
#define FLASH_LP_FEXCR_OCDID1 (0x83U)
#define FLASH_LP_FEXCR_OCDID2 (0x84U)
#define FLASH_LP_FEXCR_OCDID3 (0x85U)
#define FLASH_LP_FEXCR_OCDID4 (0x86U)
#define FLASH_LP_FEXCR_CLEAR (0x00U)
#define FLASH_LP_FEXCR_MF4_CONTROL (0x81U)
#define FLASH_LP_FEXCR_MF4_AW_STARTUP (0x82U)

#define DATA_FLASH_LENGTH (0x2000)

uint64_t ra2l1_mmio_flash_read(void *opaque, hwaddr addr, unsigned size);

void ra2l1_mmio_flash_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size);
uint64_t ra2l1_dflash_read(void *opaque, hwaddr addr, unsigned size);
void ra2l1_dflash_write(void *opaque, hwaddr vaddr, uint64_t value,
                        unsigned size);
#endif