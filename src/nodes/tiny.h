#ifndef __TINY_H_
#define __TINY_H_

#define SIMDE_ENABLE_NATIVE_ALIASES
#ifdef __SSE__
#include <simde/x86/mmx.h>
#include <simde/x86/sse.h>
// #include <emmintrin.h>
#elif __ARM_NEON
#include <simde/arm/neon.h>
// #include <arm_neon.h>
#else
#error requires x86-64 sse or ARM neon
#endif

struct TINY
{
    uchar keys[7];
    uint8_t mask;
    JP nodes[7];
};

static bool _tiny_lookup(JP *node, uchar cc)
{
    struct TINY *tiny = (struct TINY *)decode(*node);

#ifdef __SSE__

    __m64 vec = _m_from_int64(*(uint64_t *)&tiny->keys);
    __m64 key = _mm_set1_pi8(cc);
    __m64 cmp = _mm_cmpeq_pi8(vec, key);

    uint64_t res = _mm_movemask_pi8(cmp) & tiny->mask;

#elif __ARM_NEON

    uint8x8_t vec = vcreate_u8(*(uint64_t *)&tiny->keys);
    uint8x8_t msk = vcreate_u8(0x00fffefdfcfbfa08ull);

    uint8x8_t tmp = vdup_n_u8(0x80);
    uint8x8_t key = vdup_n_u8(cc);

    uint8x8_t cmp = vceq_u8(vec, key);
    uint8x8_t msb = vand_u8(cmp, tmp);

    uint8x8_t mov = vshl_u8(msb, msk);

    uint64_t res = vaddv_u8(mov) & tiny->mask;

#endif

    if (!res)
        return false;

    uint64_t idx = __builtin_clz(res << 24);

    *node = tiny->nodes[idx];

    return true;
}

static bool _tiny_insert(JP **nodeptr, uchar cc)
{
    struct TINY *tiny = (struct TINY *)decode(**nodeptr);

#ifdef __SSE__

    __m64 vec = _m_from_int64(*(uint64_t *)&tiny->keys);
    __m64 key = _mm_set1_pi8(cc);
    __m64 cmp = _mm_cmpeq_pi8(vec, key);

    uint64_t res = _mm_movemask_pi8(cmp) & tiny->mask;

#elif __ARM_NEON

    uint8x8_t vec = vcreate_u8(*(uint64_t *)&tiny->keys);
    uint8x8_t msk = vcreate_u8(0x00fffefdfcfbfa08ull);

    uint8x8_t tmp = vdup_n_u8(0x80);
    uint8x8_t key = vdup_n_u8(cc);

    uint8x8_t cmp = vceq_u8(vec, key);
    uint8x8_t msb = vand_u8(cmp, tmp);

    uint8x8_t mov = vshl_u8(msb, msk);

    uint64_t res = vaddv_u8(mov) & tiny->mask;

#endif

    if (res)
    {
        *nodeptr = &tiny->nodes[__builtin_clz(res << 24)];
        return true;
    }

    // TODO: key not found...

    if (tiny->mask != 0xfe)
    {
        uint64_t idx = __builtin_clz(~((uint32_t)tiny->mask << 24));

        tiny->mask |= 0x80 >> idx;
        tiny->keys[idx] = cc;

        *nodeptr = &tiny->nodes[idx]; // implicit leaf
    }
    else
    {
        // node is full
    }


    return false;
}

#endif // __TINY_H_
