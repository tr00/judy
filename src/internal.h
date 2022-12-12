#ifndef __INTERNAL_H_
#define __INTERNAL_H_

#define SIMDE_ENABLE_NATIVE_ALIASES

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <assert.h>

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

enum
{
    LEAF,
    TINY,
    TRIE,
};


/**
 * Node Interface:
 * 
 * static bool _{name}_lookup(JP *node, uchar cc);
 * 
 * static bool _{name}_insert(JP **nodeptr, uchar cc);
 * 
 */


void *claim(size_t size);

void stash(void *ptr, size_t size);

#endif
