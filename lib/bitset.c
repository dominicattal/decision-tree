#include "bitset.h"
#include <stdio.h>
#include <stdlib.h>

#define SIZE sizeof(char)

typedef struct Bitset {
    int numset;
    int length;
    char* buf;
} Bitset;

Bitset* bitset_create(int length)
{
    Bitset* bs = malloc(sizeof(Bitset));
    bs->numset = 0;
    bs->length = length;
    bs->buf = calloc((length + SIZE - 1) / SIZE, SIZE);
    return bs;
}

void bitset_set(Bitset* bs, int i)
{
    bs->numset++;
    bs->buf[i/SIZE] |= 1 << (i % SIZE);
}

void bitset_setall(Bitset* bs)
{
    bs->numset = bs->length;
    for (int i = 0; i < (int)((bs->length + SIZE - 1) / SIZE); i++)
        bs->buf[i] = ~0;
}

void bitset_unset(Bitset* bs, int i)
{
    bs->numset--;
    bs->buf[i/SIZE] &= ~(1<<(i % SIZE));
}

void bitset_unsetall(Bitset* bs)
{
    bs->numset = 0;
    for (int i = 0; i < (int)((bs->length + SIZE - 1) / SIZE); i++)
        bs->buf[i] = 0;
}

int bitset_isset(Bitset* bs, int i)
{
    return (bs->buf[i / SIZE] >> (i % SIZE)) & 1;
}

int bitset_numset(Bitset* bs)
{
    return bs->numset;
}

void bitset_destroy(Bitset* bs)
{
    free(bs->buf);
    free(bs);
}

void bitset_print(Bitset* bs)
{
    for (int i = 0; i < bs->length; i++)
        printf("%d", (bs->buf[i / SIZE] >> (i%SIZE)) & 1);
    puts("");
}
