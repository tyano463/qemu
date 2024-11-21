#include "qemu/osdep.h"

#include "hw/boards.h"

#include "R7FA2L1AB.h"
#include "ra2l1_sc324_aes.h"
#include "renesas_common.h"

// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wunused-variable"
// #pragma GCC diagnostic ignored "-Wunused-function"

#define AES_MODE ((aescmd & 0x30) >> 4)
#define AES_ENCRYPT_FLAG (aescmd & 1)

#define NK_MAX (8)
#define NR_MAX (14)

#define Nb (4)

struct key_round {
    uint8_t Nk;
    uint8_t Nr;
} const key_round_table = {4, 10};

/* Figure 7. S-box: substitution values for the byte xy (in hexadecimal format).
 */
const uint8_t sbox[] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
    0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
    0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
    0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
    0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
    0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
    0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
    0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
    0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
    0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
    0xb0, 0x54, 0xbb, 0x16,
};

/* Figure 14. Inverse S-box: substitution values for the byte xy (in hexadecimal
 * format). */
const uint8_t inv_sbox[] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e,
    0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
    0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32,
    0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
    0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50,
    0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05,
    0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
    0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41,
    0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8,
    0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
    0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
    0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59,
    0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
    0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d,
    0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63,
    0x55, 0x21, 0x0c, 0x7d,
};

/* x^(i-1) mod x^8 + x^4 + x^3 + x + 1 */
const uint32_t rcon[] = {
    0x00000000, /* invalid */
    0x00000001, /* x^0 */
    0x00000002, /* x^1 */
    0x00000004, /* x^2 */
    0x00000008, /* x^3 */
    0x00000010, /* x^4 */
    0x00000020, /* x^5 */
    0x00000040, /* x^6 */
    0x00000080, /* x^7 */
    0x0000001B, /* x^4 + x^3 + x^1 + x^0 */
    0x00000036, /* x^5 + x^4 + x^2 + x^1 */
};

static int dindex;

uint32_t in[4];
uint32_t out[4];
uint32_t iv[4];
uint32_t key[4];
uint32_t aescmd;

static uint8_t gmult(uint8_t a, uint8_t b) {
    uint8_t c = 0, i, msb;

    for (i = 0; i < 8; i++) {
        if (b & 1)
            c ^= a;

        msb = a & 0x80;
        a <<= 1;
        if (msb)
            a ^= 0x1b;
        b >>= 1;
    }

    return c;
}

static uint32_t rot_word(uint32_t word) {
    /* a3 a2 a1 a0 -> a0 a3 a2 a1 */
    return word << 24 | word >> 8;
}

static uint32_t sub_word(uint32_t word) {
    uint32_t val = word;
    uint8_t *p = (uint8_t *)&val;
    p[0] = sbox[p[0]];
    p[1] = sbox[p[1]];
    p[2] = sbox[p[2]];
    p[3] = sbox[p[3]];
    return val;
}

static void add_round_key(uint8_t *state /*4*Nb*/,
                          const uint32_t *w /*Nb*(Nr+1)*/) {
    int i;
    uint32_t *s = (uint32_t *)state;
    for (i = 0; i < Nb; i++) {
        s[i] ^= w[i];
    }
}

static void sub_bytes(uint8_t *state /*4*Nb*/) {
    int i;
    for (i = 0; i < 4 * Nb; i++) {
        state[i] = sbox[state[i]];
    }
}

static void inv_sub_bytes(uint8_t *state /*4*Nb*/) {
    int i;
    for (i = 0; i < 4 * Nb; i++) {
        state[i] = inv_sbox[state[i]];
    }
}

static void shift_rows(uint8_t *state /*4*Nb*/) {
    /*
       00 04 08 12 => 00 04 08 12
       01 05 09 13 => 05 09 13 01
       02 06 10 14 => 10 14 02 06
       03 07 11 15 => 15 03 07 11
     */
    uint8_t tmp[3];
    tmp[0] = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = tmp[0];
    tmp[0] = state[2];
    tmp[1] = state[6];
    state[2] = state[10];
    state[6] = state[14];
    state[10] = tmp[0];
    state[14] = tmp[1];
    tmp[0] = state[3];
    tmp[1] = state[7];
    tmp[2] = state[11];
    state[3] = state[15];
    state[7] = tmp[0];
    state[11] = tmp[1];
    state[15] = tmp[2];
}

static void inv_shift_rows(uint8_t *state /*4*Nb*/) {
    /*
       00 04 08 12 => 00 04 08 12
       01 05 09 13 => 13 01 05 09
       02 06 10 14 => 10 14 02 06
       03 07 11 15 => 07 11 15 03
     */
    uint8_t tmp[3];
    tmp[0] = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = tmp[0];
    tmp[0] = state[14];
    tmp[1] = state[10];
    state[14] = state[6];
    state[10] = state[2];
    state[6] = tmp[0];
    state[2] = tmp[1];
    tmp[0] = state[15];
    tmp[1] = state[11];
    tmp[2] = state[7];
    state[15] = state[3];
    state[11] = tmp[0];
    state[7] = tmp[1];
    state[3] = tmp[2];
}

static void mix_columns(uint8_t *state /*4*Nb*/) {
    int i;
    uint8_t tmp[4], *s = state;

    for (i = 0; i < Nb; i++) {
        tmp[0] = gmult(0x02, s[0]) ^ gmult(0x03, s[1]) ^ s[2] ^ s[3];
        tmp[1] = s[0] ^ gmult(0x02, s[1]) ^ gmult(0x03, s[2]) ^ s[3];
        tmp[2] = s[0] ^ s[1] ^ gmult(0x02, s[2]) ^ gmult(0x03, s[3]);
        tmp[3] = gmult(0x03, s[0]) ^ s[1] ^ s[2] ^ gmult(0x02, s[3]);
        memcpy(s, tmp, 4);
        s += 4;
    }
}

static void inv_mix_columns(uint8_t *state /*4*Nb*/) {
    int i;
    uint8_t tmp[4], *s = state;

    for (i = 0; i < Nb; i++) {
        tmp[0] = gmult(0x0e, s[0]) ^ gmult(0x0b, s[1]) ^ gmult(0x0d, s[2]) ^
                 gmult(0x09, s[3]);
        tmp[1] = gmult(0x09, s[0]) ^ gmult(0x0e, s[1]) ^ gmult(0x0b, s[2]) ^
                 gmult(0x0d, s[3]);
        tmp[2] = gmult(0x0d, s[0]) ^ gmult(0x09, s[1]) ^ gmult(0x0e, s[2]) ^
                 gmult(0x0b, s[3]);
        tmp[3] = gmult(0x0b, s[0]) ^ gmult(0x0d, s[1]) ^ gmult(0x09, s[2]) ^
                 gmult(0x0e, s[3]);
        memcpy(s, tmp, 4);
        s += 4;
    }
}

static void key_expansion(const uint32_t *key /*Nk*/,
                          uint32_t *w /*Nb*(Nr+1)*/) {
    int i;
    uint8_t Nr = key_round_table.Nr;
    uint8_t Nk = key_round_table.Nk;

    memcpy(w, key, Nk * 4);
    for (i = Nk; i < Nb * (Nr + 1); i++) {
        uint32_t temp = w[i - 1];
        if (i % Nk == 0) {
            temp = sub_word(rot_word(temp)) ^ rcon[i / Nk];
        } else if (6 < Nk && i % Nk == 4) {
            temp = sub_word(temp);
        }
        w[i] = w[i - Nk] ^ temp;
    }
}

static void cipher(const uint8_t *in /*4*Nb*/, uint8_t *out /*4*Nb*/,
                   const uint32_t *w /*Nb*(Nr+1)*/) {
    int i;
    uint8_t Nr = key_round_table.Nr, *state = out;

    memcpy(state, in, 4 * Nb);
    add_round_key(state, &w[0]);
    for (i = 1; i < Nr; i++) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, &w[Nb * i]);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, &w[Nb * Nr]);
}

static void inv_cipher(const uint8_t *in /*4*Nb*/, uint8_t *out /*4*Nb*/,
                       const uint32_t *w /*Nb*(Nr+1)*/) {
    int i;
    uint8_t Nr = key_round_table.Nr, *state = out;

    memcpy(state, in, 4 * Nb);
    add_round_key(state, &w[Nb * Nr]);
    for (i = Nr - 1; 1 <= i; i--) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, &w[Nb * i]);
        inv_mix_columns(state);
    }
    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, &w[0]);
}

static int deccount = 0;
static void proc(void) {
    int i;
    uint32_t w[Nb * (NR_MAX + 1)];
    key_expansion(key, w);

    uint32_t mode = AES_MODE;
    uint8_t src[BLOCK_LENGTH];
    if (deccount < 2) {
        dlog("flg:%d md:%d", AES_ENCRYPT_FLAG, mode);
    }

    if (AES_ENCRYPT_FLAG == SC324_AES_ENCRYPT) {
        switch (mode) {
        case SC324_AES_CBC:
            for (i = 0; i < BLOCK_LENGTH; i++) {
                src[i] = ((uint8_t *)iv)[i] ^ ((uint8_t *)in)[i];
            }
            // dlog("in:%s", b2s((uint8_t *)in, 16));
            // dlog("iv:%s", b2s((uint8_t *)iv, 16));
            // dlog("src:%s", b2s(src, 16));
            // dlog("key:%s", b2s((uint8_t *)key, 16));
            cipher(src, (uint8_t *)out, w);
            // dlog("out:%s", b2s((uint8_t *)out, 16));
            memcpy(iv, out, BLOCK_LENGTH);
            break;
        case SC324_AES_ECB:
            cipher((uint8_t *)in, (uint8_t *)out, w);
            break;
        default:
            break;
        }
    } else {
        switch (mode) {
        case SC324_AES_CBC:
            inv_cipher((uint8_t *)in, src, w);
            if (deccount < 2) {
                dlog("iv:%s", b2s((uint8_t *)iv, 16));
                dlog("key:%s", b2s((uint8_t *)key, 16));
                dlog("in:%s", b2s((uint8_t *)in, 16));
                dlog("out:%s", b2s((uint8_t *)src, 16));
            }
            for (i = 0; i < BLOCK_LENGTH; i++) {
                ((uint8_t *)out)[i] = ((uint8_t *)iv)[i] ^ src[i];
            }
            if (deccount < 2) {
                dlog("out:%s", b2s((uint8_t *)out, 16));
            }
            memcpy(iv, in, 16);
            deccount++;
            break;
        case SC324_AES_ECB:
            inv_cipher((uint8_t *)in, (uint8_t *)out, w);
            break;
        default:
            break;
        }
    }
}

bool is_aes(hwaddr addr) {
    return (R_AES_BASE <= addr) && (addr < (R_AES_BASE + sizeof(R_AES_Type)));
}

static hwaddr bef;
uint64_t ra2l1_mmio_aes_read(void *opaque, hwaddr _addr, unsigned size) {
    uint64_t value = 0;
    hwaddr addr = (intptr_t)(((uint8_t *)R_AES) + _addr);
    if (bef != addr) {
        // g_print("aes read %lx\n", addr);
        bef = addr;
    }
    switch (addr) {
    case (uint64_t)&R_AES->AESCMD:
        value = 0x7F000000;
        dindex = 0;
        break;
    case (uint64_t)&R_AES->AESDW:
        value = __builtin_bswap32(out[dindex++]);
    }
    // g_print("aes read %lx at %lx\n", value, addr);
    return value;
}

void ra2l1_mmio_aes_write(void *opaque, hwaddr _addr, uint64_t value,
                          unsigned size) {

    hwaddr addr = (intptr_t)(((uint8_t *)R_AES) + _addr);
    // g_print("aes write %lx <- %lx\n", addr, value);

    switch (addr) {
    case (uint64_t)&R_AES->AESMOD:
        aescmd = 0;
        memset(iv, 0, 16);
        break;
    case (uint64_t)&R_AES->AESCMD:
        aescmd |= U32(value);
        dlog("aescmd:%x", aescmd);
        break;
    case (uint64_t)&R_AES->AESKW0:
        key[dindex++] = __builtin_bswap32(U32(value));
        break;
    case (uint64_t)&R_AES->AESIVW:
        iv[dindex++] = __builtin_bswap32(U32(value));
        break;
    case (uint64_t)&R_AES->AESDW:
        in[dindex++] = __builtin_bswap32(U32(value));
        if (dindex == 4) {
            proc();
        }
        break;
    }
}

static MemoryRegionOps ops = {
    .read = ra2l1_mmio_aes_read,
    .write = ra2l1_mmio_aes_write,
};

sc324_state_t *renesas_aes_init(MemoryRegion *system_memory,
                                DeviceState *dev_soc, hwaddr baseaddr) {
    sc324_state_t *s = g_new0(sc324_state_t, 1);
    memory_region_init_io(&s->mmio, OBJECT(dev_soc), &ops, s, "renesas.sc324",
                          sizeof(R_AES_Type));
    memory_region_add_subregion(system_memory, (hwaddr)R_AES, &s->mmio);
    return s;
}
// #pragma GCC diagnostic pop