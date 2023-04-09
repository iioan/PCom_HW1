#include "trie.h"
#include "lib.h"

// function to create a new trie node
struct trie_node * trie_create() {
    struct trie_node * node = (struct trie_node * ) malloc(sizeof(struct trie_node));
    node -> children[0] = NULL;
    node -> children[1] = NULL;
    node -> entry = NULL;
    return node;
}

int mask_length(uint32_t mask) {
    uint32_t length = 0;
    for (int i = MAX_PACKET_LEN - 1; i >= 0; i--) {
        if (mask & (1 << i)) {
            length++;
        } else {
            break;
        }
    }
    return length;
}

void print_binary(uint32_t ip) {
    for (int i = 0; i < 32; i++) {
        if (ip & (1 << (31 - i)))
            printf("1");
        else
            printf("0");
        if (i % 8 == 7 && i != 31)
            printf(".");
    }
    printf("\n");
}

// function to insert a route table entry into the trie
void trie_insert(struct trie_node * root, struct route_table_entry * entry) {
    uint32_t prefix = ntohl(entry -> prefix);
    printf("\n");
    print_binary(prefix);
    printf("\n");
    uint32_t length = mask_length(ntohl(entry -> mask));
    struct trie_node * node = root;
    for (int i = 0; i < length; i++) {
        uint32_t bit = (prefix & (1 << (31 - i))) ? 1 : 0;
        printf("%d", bit);
        if (node -> children[bit] == NULL) {
            node -> children[bit] = trie_create();
        }
        node = node -> children[bit];
    }
    node -> entry = entry;
}

// function to find the longest prefix match for a given IP destination
struct route_table_entry * trie_lookup(struct trie_node * root, uint32_t destination) {
    struct trie_node * node = root;
    destination = ntohl(destination);
    struct route_table_entry * result = NULL;
    for (int i = MAX_PACKET_LEN - 1; i >= 0; i--) {
        int bit = (destination & (1 << i)) ? 1 : 0;
        if (node -> children[bit] == NULL) {
            break;
        }
        if (node -> children[bit] -> entry != NULL) {
            result = node -> children[bit] -> entry;
        }
        node = node -> children[bit];
    }
    return result;
}

// function to read entries and add them into the trie
int read_trie(const char * path, struct trie_node * root) {
    FILE * fp = fopen(path, "r");
    int j = 0, i;
    char * p, line[64];

    while (fgets(line, sizeof(line), fp) != NULL) {
        struct route_table_entry * entry = malloc(sizeof(struct route_table_entry));
        p = strtok(line, " .");
        i = 0;
        while (p != NULL) {
            if (i < 4)
                *
                (((unsigned char * ) & entry -> prefix) + i % 4) = (unsigned char) atoi(p);
            if (i >= 4 && i < 8)
                *
                (((unsigned char * ) & entry -> next_hop) + i % 4) = atoi(p);
            if (i >= 8 && i < 12)
                *
                (((unsigned char * ) & entry -> mask) + i % 4) = atoi(p);
            if (i == 12)
                entry -> interface = atoi(p);
            p = strtok(NULL, " .");
            i++;
        }
        trie_insert(root, entry);
        j++;
    }
    return j;
}