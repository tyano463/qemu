#ifndef __RENESAS_COMMON_H__
#define __RENESAS_COMMON_H__

#define ASCII_CODE_CR 0xd
#define ASCII_CODE_LF 0xa

#define OK (0)
#define NG (-1)

#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define DEBUG_MODE 0
#if DEBUG_MODE
#define dlog(s, ...)                                                   \
    do                                                                 \
    {                                                                  \
        g_print("%s(%d) %s " s "\n", __FILENAME__, __LINE__, __func__, \
                ##__VA_ARGS__);                                        \
        fflush(stdout);                                                \
    } while (0)
#else
#define dlog(s, ...) (void)0
#endif

#define MALLOC2d(_type, x, y)                                               \
    ({                                                                      \
        int _i;                                                             \
        _type **_out = (_type **)malloc(sizeof(_type *) * (y) +             \
                                        sizeof(_type) * (x) * (y));         \
        for (_i = 0; _i < (y); _i++)                                        \
        {                                                                   \
            _out[_i] =                                                      \
                (_type *)((unsigned char *)_##out + sizeof(_type *) * (y) + \
                          sizeof(_type) * (x) * _i);                        \
        }                                                                   \
        (_out);                                                             \
    })

#if DEBUG_MODE
#define ERR_RET(c, s, ...)                                                 \
    do                                                                     \
    {                                                                      \
        if (c)                                                             \
        {                                                                  \
            g_print("%s(%d) %s " s "\n", __FILENAME__, __LINE__, __func__, \
                    ##__VA_ARGS__);                                        \
            goto error_return;                                             \
        }                                                                  \
    } while (0)
#else
#define ERR_RET(c, s, ...)

#endif

#define ERR_RETn(c)            \
    do                         \
    {                          \
        if (c)                 \
            goto error_return; \
    } while (0)

#define U32(v) ((v) & (~(uint32_t)0))

char *b2s(const uint8_t *data, int n);
char *current_datetime_str(void);

#endif
