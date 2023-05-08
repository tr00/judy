#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <stdbool.h>

#include <sys/mman.h>

#include <immintrin.h>

#include "judyx64.h"

#define PAGESIZE 65536 // 2^15 == 64KiB

#define SUCCESS 0
#define FAILURE 1

typedef uintptr_t judyptr_t;

#define JUDY_MASK_PTR 0x0000fffffffffff8ull
#define JUDY_MASK_TAG 0x0000000000000007ull

#define TAG(ptr) ((ptr)&JUDY_MASK_TAG)
#define PTR(ptr) ((ptr)&JUDY_MASK_PTR)

#define MIX(ptr, tag) (((judyptr_t)ptr) | ((judyptr_t)tag))

enum
{
    LEAF,
    SPAN,
    FORK,
    TINY,
    MASK,
    TRIE,
};

typedef struct page
{
    int offset;
    struct page *next;
} page_t;

typedef struct alloc
{
    page_t *root;

    int nbytes;
    int nallocs;

    void *reuse;
} alloc_t;

struct judy
{
    judyptr_t root;

    alloc_t alloc;
};

typedef struct
{
    judyptr_t nodes[256];
} trie_t;

typedef struct
{
    uchar keys[7];
    uint8_t mask;
    judyptr_t nodes[7];
} tiny_t;

typedef struct
{
    struct
    {
        uint64_t map;
        judyptr_t *vec; // sizes: 0, 8, 16, 32, 64
    } sub[4];
} mask_t;

judy_t judy_create()
{
    judy_t judy = calloc(1, sizeof(*judy));

    return judy;
}

void judy_delete(judy_t judy)
{
    printf("[info]: %d bytes allocated\n", judy->alloc.nbytes);
    printf("[info]: %d total allocations\n", judy->alloc.nallocs);
}

static void *_judy_alloc_page()
{
    int size = PAGESIZE;
    int prot = PROT_READ | PROT_WRITE;
    int flag = MAP_PRIVATE | MAP_ANON;

    void *page = mmap(NULL, size, prot, flag, -1, 0);

    if (page == MAP_FAILED)
    {
        fprintf(stderr, "[error]: failed to mmap page from os!\n");
        exit(1);
    }

    return page;
}

void *_judy_alloc_node(judy_t judy, int type)
{
    size_t size;

    switch (type)
    {
    case TRIE:
        size = 2048;
        break;
    case TINY:
        size = 64;
        break;
    case MASK:
        size = 64;
        break;
    default:
        assert(0);
    }

    judy->alloc.nallocs += 1;
    judy->alloc.nbytes += size;

    return calloc(1, size);
}

void *judy_lookup(judy_t judy, const uchar *key)
{
    judyptr_t node = judy->root;

    while (1)
    {
        switch (TAG(node))
        {
        case LEAF:
            return (void *)node;
        case TRIE:
        {
            trie_t *trie = (trie_t *)PTR(node);

            node = trie->nodes[*key];

            if (!node)
                return NULL;

            ++key;

            break;
        }
        case TINY:
        {
            tiny_t *tiny = (tiny_t *)PTR(node);

            __m64 v0 = _m_from_int64(*(uint64_t *)&tiny->keys);
            __m64 v1 = _mm_set1_pi8(*key);
            __m64 v2 = _mm_cmpeq_pi8(v0, v1);

            uint64_t res = _mm_movemask_pi8(v2) & tiny->mask;

            if (!res)
                return NULL;

            node = tiny->nodes[__builtin_ctz(res)];

            ++key;

            break;
        }
        case MASK:
        {
            mask_t *mask = (mask_t *)PTR(node);

            uint64_t hi, lo, cc = *key;

            hi = cc >> 6;
            cc = cc & 63;

            uint64_t map = mask->sub[hi].map;

            if (!(map & (1ull << cc)))
                return NULL;

            uint64_t tmp = _bzhi_u64(map, cc);
            lo = __builtin_popcountll(tmp);

            node = mask->sub[hi].vec[lo];

            ++key;

            break;
        }
        default:
            assert(0);
        }
    }

    __builtin_unreachable();
}

/**
 * unsafely insert an element into the mask node.
 *
 * it is assumed that element is not already in the node
 * and this node won't promote.
 */
static void _mask_push(judy_t judy, mask_t *mask, uchar cc, judyptr_t val)
{
    uint64_t hi, lo;

    hi = cc >> 6;
    cc = cc & 63;

    uint64_t map = mask->sub[hi].map;
    uint64_t **vec = &mask->sub[hi].vec;

    assert(!(map & (1ull << cc)));

    int cnt = __builtin_popcountll(map);

    switch (cnt)
    {
    case 0:
        *vec = calloc(8, 8);
        break;
    case 8:
        *vec = realloc(*vec, 16 * 8);
        break;
    case 16:
        *vec = realloc(*vec, 32 * 8);
        break;
    case 32:
        *vec = realloc(*vec, 64 * 8);
        break;
    }

    uint64_t tmp = _bzhi_u64(map, cc);

    lo = __builtin_popcountll(tmp);

    judyptr_t *ptr = &mask->sub[hi].vec[lo];

    memmove(ptr + 1, ptr, 8 * (cnt - lo));

    *ptr = val;
    mask->sub[hi].map |= 1ull << cc;
}

static mask_t *_tiny_grow(judy_t judy, tiny_t *tiny)
{
    mask_t *mask = _judy_alloc_node(judy, MASK);

    uchar c0, c1, c2, c3, c4, c5, c6;
    uchar h0, h1, h2, h3, h4, h5, h6;

    uint64_t m0, m1, m2, m3, m4, m5, m6;
    uint64_t i0, i1, i2, i3, i4, i5, i6;

    c0 = tiny->keys[0];
    c1 = tiny->keys[1];
    c2 = tiny->keys[2];
    c3 = tiny->keys[3];
    c4 = tiny->keys[4];
    c5 = tiny->keys[5];
    c6 = tiny->keys[6];

    h0 = c0 >> 6;
    h1 = c1 >> 6;
    h2 = c2 >> 6;
    h3 = c3 >> 6;
    h4 = c4 >> 6;
    h5 = c5 >> 6;
    h6 = c6 >> 6;

    mask->sub[h0].map |= 1ull << c0;
    mask->sub[h1].map |= 1ull << c1;
    mask->sub[h2].map |= 1ull << c2;
    mask->sub[h3].map |= 1ull << c3;
    mask->sub[h4].map |= 1ull << c4;
    mask->sub[h5].map |= 1ull << c5;
    mask->sub[h6].map |= 1ull << c6;

    if (mask->sub[0].map) mask->sub[0].vec = calloc(8, 8);
    if (mask->sub[1].map) mask->sub[1].vec = calloc(8, 8);
    if (mask->sub[2].map) mask->sub[2].vec = calloc(8, 8);
    if (mask->sub[3].map) mask->sub[3].vec = calloc(8, 8);

    m0 = mask->sub[h0].map;
    m1 = mask->sub[h1].map;
    m2 = mask->sub[h2].map;
    m3 = mask->sub[h3].map;
    m4 = mask->sub[h4].map;
    m5 = mask->sub[h5].map;
    m6 = mask->sub[h6].map;

    i0 = __builtin_popcountll(_bzhi_u64(m0, c0 & 63));
    i1 = __builtin_popcountll(_bzhi_u64(m1, c1 & 63));
    i2 = __builtin_popcountll(_bzhi_u64(m2, c2 & 63));
    i3 = __builtin_popcountll(_bzhi_u64(m3, c3 & 63));
    i4 = __builtin_popcountll(_bzhi_u64(m4, c4 & 63));
    i5 = __builtin_popcountll(_bzhi_u64(m5, c5 & 63));
    i6 = __builtin_popcountll(_bzhi_u64(m6, c6 & 63));

    mask->sub[h0].vec[i0] = tiny->nodes[0];
    mask->sub[h1].vec[i1] = tiny->nodes[1];
    mask->sub[h2].vec[i2] = tiny->nodes[2];
    mask->sub[h3].vec[i3] = tiny->nodes[3];
    mask->sub[h4].vec[i4] = tiny->nodes[4];
    mask->sub[h5].vec[i5] = tiny->nodes[5];
    mask->sub[h6].vec[i6] = tiny->nodes[6];

    return mask;
}

void judy_insert(judy_t judy, const uchar *key, void *val)
{
    judyptr_t *nodeptr = &judy->root;

    while (1)
    {
        uchar cc = *key;

        switch (TAG(*nodeptr))
        {
        case LEAF:
        {
            if (PTR(*nodeptr))
                assert(0);

            goto L1;
        }
        case TRIE:
        {
            trie_t *trie = (trie_t *)PTR(*nodeptr);

            nodeptr = &trie->nodes[cc];

            break;
        }
        case TINY:
        {
            tiny_t *tiny = (tiny_t *)PTR(*nodeptr);

            __m64 v0 = _m_from_int64(*(uint64_t *)&tiny->keys);
            __m64 v1 = _mm_set1_pi8(cc);
            __m64 v2 = _mm_cmpeq_pi8(v0, v1);

            uint64_t res = _mm_movemask_pi8(v2) & tiny->mask;

            if (res)
            {
                nodeptr = &tiny->nodes[__builtin_ctz(res)];
                break;
            }

            if (tiny->mask != 0x7f)
            {
                uint64_t idx = __builtin_ctz(~tiny->mask);

                tiny->mask |= 0x01 << idx;
                tiny->keys[idx] = cc;

                nodeptr = &tiny->nodes[idx];

                break;
            }

            // grow to trie node

            trie_t *trie = _judy_alloc_node(judy, TRIE);

            trie->nodes[tiny->keys[0]] = tiny->nodes[0];
            trie->nodes[tiny->keys[1]] = tiny->nodes[1];
            trie->nodes[tiny->keys[2]] = tiny->nodes[2];
            trie->nodes[tiny->keys[3]] = tiny->nodes[3];
            trie->nodes[tiny->keys[4]] = tiny->nodes[4];
            trie->nodes[tiny->keys[5]] = tiny->nodes[5];
            trie->nodes[tiny->keys[6]] = tiny->nodes[6];

            mask_t *mask;
            
            __attribute__((noinline)) mask = _tiny_grow(judy, &tiny);

            // we can assume cc to not be in mask
            // and we won't need to allocate bc there are only 7
            // elements in mask

            uint64_t hi, lo, pc;
            
            hi = cc >> 6;

            uint64_t map = mask->sub[hi].map;

            mask->sub[hi].map |= 1ull << cc;

            pc = __builtin_popcountll(map);
            lo = __builtin_popcountll(_bzhi_u64(map, cc & 63));

            judyptr_t *ptr = &mask->sub[hi].vec[lo];

            memmove(ptr + 1, ptr, 8 * (pc - lo));

            *nodeptr = MIX(mask, MASK);

            nodeptr = ptr;

            goto L1;
        }
        case MASK:
        {
            mask_t *mask = (mask_t *)PTR(*nodeptr);

            uint64_t hi, lo, cc = *key;

            hi = cc >> 6;
            cc = cc & 63;

            uint64_t bitmap = mask->sub[hi].map;

            if (bitmap & (1ull << cc))
            {
                lo = __builtin_popcountll(_bzhi_u64(bitmap, cc));
                nodeptr = &mask->sub[hi].vec[lo];
                break;
            }
            else
            {
                judyptr_t **vec = &mask->sub[hi].vec;

                int cnt = __builtin_popcountll(bitmap);

                switch (cnt)
                {
                case 0:
                    *vec = calloc(8, 8);
                    break;
                case 8:
                    *vec = realloc(*vec, 16 * 8);
                    break;
                case 16:
                    *vec = realloc(*vec, 32 * 8);
                    break;
                case 32:
                    *vec = realloc(*vec, 64 * 8);
                    break;
                }

                lo = __builtin_popcountll(_bzhi_u64(bitmap, cc));

                judyptr_t *ptr = &mask->sub[hi].vec[lo];

                memmove(ptr + 1, ptr, 8 * (cnt - lo));

                nodeptr = &mask->sub[hi].vec[lo];
                mask->sub[hi].map |= 1ull << cc;
                break;
            }
        }
        }

        if (!cc)
            break;

        ++key;
    }

    goto L2;

L1:
    (void)0;

    while (1)
    {
        uchar cc = *key;

        // trie_t *trie = _judy_alloc_node(judy, TRIE);
        // *nodeptr = MIX(trie, TRIE);
        // nodeptr = &trie->nodes[cc];

        tiny_t *tiny = _judy_alloc_node(judy, TINY);

        *nodeptr = MIX(tiny, TINY);

        tiny->keys[0] = cc;
        tiny->mask = 0x01;

        nodeptr = &tiny->nodes[0];

        if (!cc)
            break;

        ++key;
    }

L2:
    (void)0;

    *nodeptr = (uintptr_t)val;
}

// int main()
// {
//     judy_t judy = judy_create();

//     judy_insert(judy, (const uchar *)"0", &judy);
//     judy_insert(judy, (const uchar *)"1", &judy);
//     judy_insert(judy, (const uchar *)"2", &judy);
//     judy_insert(judy, (const uchar *)"3", &judy);
//     judy_insert(judy, (const uchar *)"4", &judy);
//     judy_insert(judy, (const uchar *)"5", &judy);
//     judy_insert(judy, (const uchar *)"6", &judy);
//     judy_insert(judy, (const uchar *)"7", &judy);
//     judy_insert(judy, (const uchar *)"8", &judy);

//     void *res;

//     res = judy_lookup(judy, (const uchar *)"8");

//     printf("%p\n", res);

//     judy_delete(judy);

//     return 0;
// }