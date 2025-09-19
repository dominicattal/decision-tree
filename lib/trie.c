#include <stdlib.h>
#include <string.h>

typedef struct TrieNode TrieNode;

typedef struct TrieNode {
    TrieNode* children[16];
    long long data;
} TrieNode;

typedef struct Trie {
    TrieNode* root;
    char** keys;
    int unique_keys;
} Trie;

static void tnode_set_terminal(TrieNode* node, int val)
{
    node->data = (node->data & ~1) | val;
}

static void tnode_set_id(TrieNode* node, int val)
{
    node->data = (node->data & ~(0xFFFFFFFFFF << 1)) | (val << 1);
}

static int tnode_get_terminal(TrieNode* node)
{
    return node->data & 1;
}

static int tnode_get_id(TrieNode* node)
{
    return node->data >> 1;
}

static TrieNode* tnode_create(void)
{
    return calloc(1, sizeof(TrieNode));
}

static void tnode_destroy(TrieNode* node)
{
    if (node == NULL) 
        return;
    for (int i = 0; i < 16; i++)
        tnode_destroy(node->children[i]);
    free(node);
}

Trie* trie_create(void)
{
    return calloc(1, sizeof(Trie));
}

void trie_destroy(Trie* trie)
{
    tnode_destroy(trie->root);
    for (int i = 0; i < trie->unique_keys; i++)
        free(trie->keys[i]);
    free(trie->keys);
    free(trie);
}

int trie_contains(Trie* trie, const char* key)
{
    TrieNode* node;
    int n, i, c;
    node = trie->root;
    if (node == NULL)
        return 0;
    n = strlen(key);
    for (i = 0, c = key[i]; i < n; i++, c = key[i])
        if ((node = node->children[c/16]) == NULL || (node = node->children[c%16]) == NULL)
            return 0;
    return tnode_get_terminal(node);
}

void trie_insert(Trie* trie, const char* key)
{
    TrieNode* node;
    int n, i, c, d1, d2;
    char* str_copy;
    if (trie->root == NULL)
        trie->root = tnode_create();
    node = trie->root;
    n = strlen(key);
    for (i = 0, c = key[0]; i < n; i++, c = key[i]) {
        d1 = c / 16, d2 = c % 16;
        if (node->children[d1] == NULL)
            node->children[d1] = tnode_create();
        node = node->children[d1];
        if (node->children[d2] == NULL)
            node->children[d2] = tnode_create();
        node = node->children[d2];
    }
    tnode_set_terminal(node, 1);
    tnode_set_id(node, trie->unique_keys);
    if (trie->keys == NULL)
        trie->keys = malloc(sizeof(char*));
    else
        trie->keys = realloc(trie->keys, (trie->unique_keys+1) * sizeof(char*));
    str_copy = malloc((n+1) * sizeof(char));
    strncpy(str_copy, key, n+1);
    trie->keys[trie->unique_keys++] = str_copy;
}

int trie_num_unique_keys(Trie* trie)
{
    return trie->unique_keys;
}

int trie_key_id(Trie* trie, const char* key)
{
    TrieNode* node;
    int n, i, c;
    node = trie->root;
    if (node == NULL)
        return -1;
    n = strlen(key);
    for (i = 0, c = key[i]; i < n; i++, c = key[i])
        if ((node = node->children[c/16]) == NULL || (node = node->children[c%16]) == NULL)
            return -1;
    return tnode_get_id(node);
}

char* trie_id_key(Trie* trie, int id)
{
    return trie->keys[id];
}
