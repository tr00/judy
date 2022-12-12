#include "judy.h"
#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nodes/trie.h"
#include "nodes/tiny.h"
#include "nodes/leaf.h"


void *judy_lookup(judy_t *judy, const uchar *key)
{
    JP node = judy->root;

    while (1)
    {
        uchar cc = *key;

        bool res;
        switch (typeof(node))
        {
        case LEAF:
            res = _leaf_lookup(&node, cc);
            break;
        case TINY:
            res = _tiny_lookup(&node, cc);
            break;
        case TRIE:
            res = _trie_lookup(&node, cc);
            break;
        default:
            assert(0);
        }

        if (res == false)
            return NULL;

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
    JP *nodeptr = &judy->root;

    // traverse the judy array by decoding char by char until
    // a node is reached which no longer contains the remaining key
    while (1)
    {
        uchar cc = *key;

        bool res;
        switch (typeof(*nodeptr))
        {
        case LEAF:
            res = _leaf_insert(&nodeptr, cc);
            break;
        case TINY:
            res = _tiny_insert(&nodeptr, cc);
            break;
        case TRIE:
            res = _trie_insert(&nodeptr, cc);
            break;
        }

        if (res == false)
            break;

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

        // // old:
        // struct TRIE *trie = claim(sizeof(struct TRIE));
        // *nodeptr = encode(trie, TRIE);
        // nodeptr = &trie->nodes[cc];

        // new:
        struct TINY *tiny = claim(sizeof(struct TINY));
        *nodeptr = encode(tiny, TINY);
        _tiny_insert(&nodeptr, cc);

        if (!cc)
            break;

        ++key;
    }

// we can assume that `key` is pointing to '\0' so
// all that is left to be done is write the value to a leaf node.
    *nodeptr = encode(val, LEAF);
}

void judy_create(judy_t *judy)
{
    judy->root = (JP)0;
}

void judy_delete(judy_t *judy)
{
}

// internal allocator

enum
{
    N2048,
    N64,
};

int numallocs[2] = {};

void *claim(size_t size)
{
    switch (size)
    {
    case 64:
        ++numallocs[N64];
        break;
    case 2048:
        ++numallocs[N2048];
        break;
    default:
        assert(0);
    }

    return calloc(1, size);
}

void stash(void *ptr, size_t size)
{
    free(ptr);
}
