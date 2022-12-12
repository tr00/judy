#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#include "../src/judy.h"

#define N 1000
#define L 32

int main()
{
    uchar **strings = malloc(N * sizeof(uchar *));

    // create strings
    for (int i = 0; i < N; ++i)
    {
        int strlen = 32; // rand() % 48 + 16;

        strings[i] = malloc(strlen);

        for (int j = 0; j < strlen - 1; ++j)
            strings[i][j] = (uchar)(rand() % 26 + 'a');
        
        strings[i][strlen - 1] = '\0';
    }

    judy_t judy;

    judy_create(&judy);

    clock_t t0 = clock();

    // insert strings
    for (int i = 0; i < N; ++i)
    {
        judy_insert(&judy, strings[i], &judy);
    }

    clock_t t1 = clock();

    printf("insertion time: %.fms\n", (double)(t1 - t0)/CLOCKS_PER_SEC * 1000);

    judy_delete(&judy);

    for (int i = 0; i < N; ++i)
        free(strings[i]);
    
    free(strings);

    return 0;
}