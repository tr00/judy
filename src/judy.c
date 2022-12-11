

#include "internal.h"

#include <string.h>
#include <stdlib.h>

#include <simde/x86/mmx.h>
#include <simde/x86/sse.h>

#include "nodes/leaf.h"
#include "nodes/tiny.h"
#include "nodes/trie.h"

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
            res = _leaf_insert(nodeptr, cc);
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

        judy_trie_t *trie = (judy_trie_t *)calloc(1, sizeof(judy_trie_t));
        *nodeptr = encode(trie, TRIE);
        nodeptr = &trie->nodes[cc];

        if (!cc)
            break;

        ++key;
    }

// we can assume that `key` is pointing to '\0' so
// all that is left to be done is write the value to a leaf node.
INSERT:

    *nodeptr = encode(val, LEAF);
}

// int main()
// {
//     judy_t judy = {};

//     judy_insert(&judy, (uchar *)"abc", &judy);

//     const void *res;

//     res = judy_lookup(&judy, (uchar *)"abc");

//     assert(res == &judy);

//     judy_remove(&judy, (uchar *)"abc");

//     res = judy_lookup(&judy, (uchar *)"abc");

//     assert(res == NULL);

//     return 0;
// }