#ifndef __RENESAS_COMMON_H__
#define __RENESAS_COMMON_H__

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define dlog(s, ...)                                                                  \
    do                                                                                \
    {                                                                                 \
        g_print("%s(%d) %s " s "\n", __FILENAME__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

#define MALLOC2d(_type, x, y)                                                                             \
    ({                                                                                                    \
        int _i;                                                                                           \
        _type **_out = (_type **)malloc(sizeof(_type *) * (y) + sizeof(_type) * (x) * (y));                     \
        for (_i = 0; _i < (y); _i++)                                                                        \
        {                                                                                                 \
            _out[_i] = (_type *)((unsigned char *)_##out + sizeof(_type *) * (y) + sizeof(_type) * (x) * _i); \
        }                                                                                                 \
        (_out);                                                                                           \
    })

#endif
