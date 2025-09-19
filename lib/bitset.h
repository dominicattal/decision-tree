#ifndef BITSET_H
#define BITSET_H

typedef struct Bitset Bitset;

Bitset* bitset_create(int length);
void    bitset_set(Bitset* bs, int i);
void    bitset_setall(Bitset* bs);
void    bitset_unset(Bitset* bs, int i);
void    bitset_unsetall(Bitset* bs);
int     bitset_isset(Bitset* bs, int i);
int     bitset_numset(Bitset* bs);
void    bitset_destroy(Bitset* bs);
void    bitset_print(Bitset* bs);

#endif
