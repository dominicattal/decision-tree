#ifndef TRIE_H
#define TRIE_H

typedef struct Trie Trie;

Trie*   trie_create(void);
void    trie_destroy(Trie* trie);
int     trie_contains(Trie* trie, const char* key);
void    trie_insert(Trie* trie, const char* key);

int     trie_num_unique_keys(Trie* trie);

// gets a unique identifier for key based on what has been inserted already
// returns -1 if key is not in the trie
int     trie_key_id(Trie* trie, const char* key);

// reverse of trie_key_id
char*   trie_id_key(Trie* trie, int id);

#endif
