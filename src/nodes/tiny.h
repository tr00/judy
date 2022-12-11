#ifndef __TINY_H_
#define __TINY_H_

#include <simde/x86/mmx.h>
#include <simde/x86/sse.h>

struct TINY
{
    uchar keys[7];
    uint8_t mask;
    JP nodes[7];
};

static bool _tiny_lookup(JP *node, uchar cc)
{
    struct TINY *tiny = (struct TINY *)decode(*node);

    __m64 vec = _m_from_int64(*(uint64_t *)&tiny->keys);
    __m64 key = _mm_set1_pi8(cc);
    __m64 cmp = _mm_cmpeq_pi8(vec, key);

    uint64_t res = _mm_movemask_pi8(cmp) & tiny->mask;

    if (!res)
        return false;

    uint64_t idx = __builtin_clz(res) - 24;

    *node = tiny->nodes[idx];

    return true;
}

static bool _tiny_insert(JP **nodeptr, uchar cc)
{
    struct TINY *tiny = (struct TINY *)decode(**nodeptr);

    __m64 vec = _m_from_int64(*(uint64_t *)&tiny->keys);
    __m64 key = _mm_set1_pi8(cc);
    __m64 cmp = _mm_cmpeq_pi8(vec, key);

    uint64_t res = _mm_movemask_pi8(cmp) & tiny->mask;

    if (res)
    {
        *nodeptr = &tiny->nodes[__builtin_clz(res) - 24];
        return true;
    }

    // TODO: key not found

    return false;
}

#endif // __TINY_H_
