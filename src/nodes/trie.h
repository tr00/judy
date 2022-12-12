#ifndef __TRIE_H_
#define __TRIE_H_

/**
 * This node stores all subexpanses in a single buffer.
 * Each subexpanse is indexed by the current char.
 *
 * A lookup takes requires only 1 cache line fill but
 * the memory footprint of this node is very large
 * for small population sizes.
 */
struct TRIE
{
    JP nodes[256];
};

static bool _trie_lookup(JP *node, uchar cc)
{
    struct TRIE *trie = (struct TRIE *)decode(*node);

    *node = trie->nodes[cc];

    return true;
}

static bool _trie_insert(JP **nodeptr, uchar cc)
{
    struct TRIE *trie = (struct TRIE *)decode(**nodeptr);

    *nodeptr = &trie->nodes[cc];

    return true;
}

#endif // __TRIE_H_
