#include "qemu/osdep.h"

#include "R7FA2L1AB.h"
#include "ra2l1_flash.h"
#include "renesas_common.h"

static uint8_t dflash[DATA_FLASH_LENGTH];
static uint32_t start_addr;
static uint32_t end_addr;

static uint64_t lp_fstatr1 = 0;

static uint8_t write_buffer;

uint64_t ra2l1_dflash_read(void *opaque, hwaddr addr, unsigned size) {
    uint8_t *p = &dflash[addr];
    switch (size) {
    case 1:
        return (uint64_t)*p;
    case 2:
        if ((uintptr_t)p % size)
            return 0;
        return (uint64_t) * (uint16_t *)p;
    case 8:
        if ((uintptr_t)p % size)
            return 0;
        return *(uint64_t *)p;
    case 4:
    default:
        if ((uintptr_t)p % size)
            return 0;
        return (uint64_t) * (uint32_t *)p;
        break;
    }
}

void ra2l1_dflash_write(void *opaque, hwaddr vaddr, uint64_t value,
                        unsigned size) {
    dlog("");
}
uint64_t ra2l1_mmio_flash_read(void *opaque, hwaddr vaddr, unsigned size) {
    hwaddr addr = vaddr | 0x407E0000;
    uint64_t value = 0;

    switch (addr) {
    case (uintptr_t)&R_FACI_LP->FSTATR1:
        value = lp_fstatr1 ^= 0x40;
        break;
    }
    return value;
}

static bool valid_addr(hwaddr addr) { return addr < DATA_FLASH_LENGTH; }

static bool valid_size(size_t size) { return size <= DATA_FLASH_LENGTH; }

static void process_command(uint64_t command) {
    hwaddr addr, eaddr;
    size_t size;
    switch (command) {
    case FLASH_LP_FCR_ERASE:
        addr = start_addr - FLASH_LP_DATAFLASH_WRITE_BASE_ADDR;
        eaddr = end_addr - FLASH_LP_DATAFLASH_WRITE_BASE_ADDR;
        size = eaddr - addr + 1;
        if (valid_addr(addr) && valid_addr(eaddr) && valid_size(size)) {
            memset(&dflash[addr], 0, size);
        }
        break;
    case FLASH_LP_FCR_WRITE:
        addr = start_addr - FLASH_LP_DATAFLASH_WRITE_BASE_ADDR;
        if (!valid_addr(addr))
            break;
        dflash[addr] = write_buffer;
        break;
    }
}

void ra2l1_mmio_flash_write(void *opaque, hwaddr vaddr, uint64_t value,
                            unsigned size) {

    uint64_t addr = vaddr | 0x407E0000;

    // dlog("a:%lx v:%lx", addr, value);

    switch (addr) {
    case (uintptr_t)&R_FACI_LP->FSTATR1:
        value = lp_fstatr1 ^= 0x40;
        break;
    case (uintptr_t)&R_FACI_LP->FASR:
        break;
    case (uintptr_t)&R_FACI_LP->FSARH:
        start_addr &= 0xffff;
        start_addr |= ((value & 0xffff) << 16);
        break;
    case (uintptr_t)&R_FACI_LP->FSARL:
        start_addr &= 0xffff0000;
        start_addr |= ((value & 0xffff) << 0);
        break;
    case (uintptr_t)&R_FACI_LP->FEARH:
        end_addr &= 0xffff;
        end_addr |= ((value & 0xffff) << 16);
        break;
    case (uintptr_t)&R_FACI_LP->FEARL:
        end_addr &= 0xffff0000;
        end_addr |= ((value & 0xffff) << 0);
        break;
    case (uintptr_t)&R_FACI_LP->FWBL0:
        write_buffer = value & 0xff;
        break;
    case (uintptr_t)&R_FACI_LP->FCR:
        process_command(value);
        if (value)
            lp_fstatr1 = 0x40;
        else
            lp_fstatr1 = 0;
        break;
    }
}