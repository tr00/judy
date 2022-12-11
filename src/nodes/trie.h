#ifndef __TRIE_H_
#define __TRIE_H_

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
