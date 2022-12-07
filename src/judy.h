#ifndef __JUDY_H_
#define __JUDY_H_

typedef unsigned char uchar;

/**
 * finds the value associated with the '\0'-terminated string key.
 * returns NULL if it can't be found.
 */
void *judy_lookup(void *judy, const uchar *key);

/**
 * inserts the value into the judy array. 
 * the value must be a valid pointer and can't be NULL.
 */
void judy_insert(void *judy, const uchar *key, void *val);

/**
 * removes a previously insert value from judy.
 * if the key can't be found nothing happens.
 */
void judy_remove(void *judy, const uchar *key);

#endif // __JUDY_H_
