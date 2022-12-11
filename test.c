#include <stdlib.h>
#include <assert.h>

#include "src/judy.h"

int main()
{
    judy_t judy = {};

    judy_insert(&judy, (uchar *)"abc", &judy);

    const void *res;

    res = judy_lookup(&judy, (uchar *)"abc");

    assert(res == &judy);

    return 0;
}