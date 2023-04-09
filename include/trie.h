#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

struct trie_node {
    struct trie_node * children[2];
    struct route_table_entry * entry;
};

#define MAX_PREFIX_LEN 32;

struct trie_node * trie_create();

void trie_insert(struct trie_node * root, struct route_table_entry * entry);

int mask_length(uint32_t mask);

void print_binary(uint32_t ip);

struct route_table_entry * trie_lookup(struct trie_node * root, uint32_t destination) ;

int read_trie(const char * path, struct trie_node * root) ;