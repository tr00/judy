
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include <simde/x86/mmx.h>
#include <simde/x86/sse.h>

typedef uint8_t uchar;

#define JUDY_MASK_PTR 0x0000fffffffffff8ull
#define JUDY_MASK_TAG 0x0000000000000007ull

#define typeof(ptr) ((ptr)&JUDY_MASK_TAG)
#define decode(ptr) ((ptr)&JUDY_MASK_PTR)

/**
 *
 */
typedef uintptr_t JP;

static inline __attribute__((always_inline)) JP encode(void *node, uintptr_t tag)
{
    uintptr_t ptr = (uintptr_t)node;

    assert((ptr & JUDY_MASK_TAG) == 0);
    assert((tag & JUDY_MASK_PTR) == 0);

    return (JP)(ptr | tag);
}

typedef struct JUDY
{
    JP root;
} judy_t;

enum
{
    LEAF,
    TINY,
    TRIE,
};

/**
 * This node stores all subexpanses in a single buffer.
 * Each subexpanse is indexed by the current char.
 *
 * A lookup takes requires only 1 cache line fill but
 * the memory footprint of this node is very large
 * for small population sizes.
 */
typedef struct JUDY_TRIE_NODE
{
    JP nodes[256];
} judy_trie_t;

/**
 * 
 */
typedef struct JUDY_TINY_NODE
{
    uchar keys[7];
    uint8_t mask;
    JP nodes[7];
} judy_tiny_t;

void *judy_lookup(judy_t *judy, const uchar *key)
{
    JP node = judy->root;

    while (1)
    {
        uchar cc = *key;

        switch (typeof(node))
        {
        case LEAF:
            return NULL;
        case TINY:
        {
            judy_tiny_t *tiny = (judy_tiny_t *)decode(node);

            __m64 vec = _m_from_int64(*(uint64_t *)&tiny->keys);
            __m64 key = _mm_set1_pi8(cc);
            __m64 cmp = _mm_cmpeq_pi8(vec, key);

            uint64_t res = _mm_movemask_pi8(cmp) & tiny->mask;

            if (!res)
                return NULL;

            node = tiny->nodes[__builtin_clz(res)];

            break;
        }
        case TRIE:
        {
            judy_trie_t *trie = (judy_trie_t *)decode(node);
            node = trie->nodes[cc];
            break;
        }
        }

        // If cc == '\0' we can assume that we deal with a leave node
        // because no string key continues after '\0'.
        // The associated value can be retrieved instead of a subexpanse
        if (!cc)
            return (void *)decode(node);

        ++key;
    }

    __builtin_unreachable();
}

void judy_insert(judy_t *judy, const uchar *key, void *val)
{
    JP *node = &judy->root;

// traverse the judy array by decoding char by char until
// a node is reached which no longer contains the remaining key
LOOKUP:

    while (1)
    {
        uchar cc = *key;

        switch (typeof(*node))
        {
        case LEAF:
            goto EXPAND;
        case TINY:
        {
            judy_tiny_t *tiny = (judy_tiny_t *)decode(*node);

            __m64 vec = _m_from_int64(*(uint64_t *)&tiny->keys);
            __m64 key = _mm_set1_pi8(cc);
            __m64 cmp = _mm_cmpeq_pi8(vec, key);

            uint64_t res = _mm_movemask_pi8(cmp) & tiny->mask;

            if (!res)
            {
                // key not found
            }

            node = &tiny->nodes[__builtin_clz(res)];

            break;
        }
        case TRIE:
        {
            judy_trie_t *trie = (judy_trie_t *)decode(*node);
            node = &trie->nodes[cc];
            break;
        }
        }

        if (!cc)
            break;

        ++key;
    }

// at this point `key` points to the remaining undecoded
// chars and `node` points to an empty leaf node.
// From here new nodes get allocated.
EXPAND:

    while (1)
    {
        uchar cc = *key;

        judy_trie_t *trie = (judy_trie_t *)calloc(1, sizeof(judy_trie_t));
        *node = encode(trie, TRIE);
        node = &trie->nodes[cc];

        if (!cc)
            break;

        ++key;
    }

// we can assume that `key` is pointing to '\0' so
// all that is left to be done is write the value to a leaf node.
INSERT:

    *node = encode(val, LEAF);
}


int main()
{
    judy_t judy = {};

    judy_insert(&judy, (uchar *)"abc", &judy);

    const void *res;
    
    res = judy_lookup(&judy, (uchar *)"abc");

    assert(res == &judy);

    judy_remove(&judy, (uchar *)"abc");

    res = judy_lookup(&judy, (uchar *)"abc");

    assert(res == NULL);

    return 0;
}