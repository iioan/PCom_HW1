#include "queue.h"
#include "lib.h"
#include "protocols.h"
#include "trie.h"
#include <string.h>
#include <arpa/inet.h>

#define ETHERTYPE_IP 0x0800 /* IP */
#define ETHERTYPE_ARP 0x0806 /* ARP */
#define ARPOP_REQUEST 1
#define ARPOP_REPLY 2

struct arp_entry * arp_table;
int arp_table_len;

struct trie_node * trie_root;
int trie_length;


struct arp_entry * get_arp_entry(uint32_t ip) {
    for (int i = 0; i < arp_table_len; i++) {
        if (ip == arp_table[i].ip) {
            return & arp_table[i];
        }
    }
    return NULL;
}


int main(int argc, char * argv[]) {
    char buf[MAX_PACKET_LEN];

    // Do not modify this line
    init(argc - 2, argv + 2);

    trie_root = trie_create();
    trie_length = read_trie(argv[1], trie_root);

    arp_table = (struct arp_entry * ) malloc(sizeof(struct arp_entry) * 100);
    queue unsent_packets = queue_create();

    while (1) {
        int interface;
        size_t len;

        uint32_t length_arp = sizeof(struct arp_header) + sizeof(struct ether_header);
        uint32_t length_icmp = sizeof(struct icmphdr) + sizeof(struct iphdr) + sizeof(struct ether_header);

        interface = recv_from_any_link(buf, & len);
        DIE(interface < 0, "recv_from_any_links");

        printf("We have received a packet\n");
        struct ether_header * eth_hdr = (struct ether_header * ) buf;
        u_int16_t ether_type = ntohs(eth_hdr -> ether_type);

        if (ether_type == ETHERTYPE_IP) {
            printf("It's an IP packet\n");
            struct iphdr * ip_hdr = (struct iphdr * )(buf + sizeof(struct ether_header));
            struct icmphdr * icmp_hdr = (struct icmphdr * )(buf + sizeof(struct ether_header) + sizeof(struct iphdr));

            if (icmp_hdr -> type == 8 && icmp_hdr -> code == 0) {
                u_int32_t interface_ip = inet_addr(get_interface_ip(interface));

                if (ip_hdr -> daddr == interface_ip) {
                    printf("It's for us\n");
                    // send icmp echo reply
                    char * buffer = buffer_icmp(ip_hdr, eth_hdr, icmp_hdr, 0, 0);
                    send_to_link(interface, buffer, length_icmp);
                    continue;
                }
            }

            u_int16_t ip_header_checksum = ip_hdr -> check;
            ip_hdr -> check = 0;
            uint16_t check_sum = ntohs(checksum((uint16_t * ) ip_hdr, sizeof(struct iphdr)));

            ip_hdr -> check = ip_header_checksum;
            if (check_sum != ip_hdr -> check) {
                printf("Checksum is not correct\n");
                continue;
            }
            printf("Checksum is correct\n");

            struct route_table_entry * nhop = trie_lookup(trie_root, ip_hdr -> daddr);
            if (nhop == NULL) {
                printf("No route found\n");
                char * buffer = buffer_icmp(ip_hdr, eth_hdr, icmp_hdr, 3, 0);
                send_to_link(interface, buffer, length_icmp);
                continue;
            }
            printf("Route found\n");

            if (ip_hdr -> ttl <= 1) {
                printf("It's time to die\n");
                char * buffer = buffer_icmp(ip_hdr, eth_hdr, icmp_hdr, 11, 0);
                send_to_link(interface, buffer, length_icmp);
                continue;
            }
            printf("TTL still valid\n");

            uint8_t old_ttl = ip_hdr -> ttl;
            (ip_hdr -> ttl) --;
            int new_checksum = ~(~ip_hdr -> check + ~((uint16_t) old_ttl) + (uint16_t) ip_hdr -> ttl) - 1;
            ip_hdr -> check = new_checksum;

            struct arp_entry * new_arp_entry = get_arp_entry(ip_hdr -> daddr);
            if (!new_arp_entry) {
                printf("Didn't get the entry\n");

                char * saved = (char * ) malloc(MAX_PACKET_LEN);
                memcpy(saved, buf, MAX_PACKET_LEN);
                queue_enq(unsent_packets, saved);

                printf("Generating ARP request\n");
                uint8_t next_hop_mac[6] = {0};
                get_interface_mac(nhop -> interface, next_hop_mac);
                uint8_t brc_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

                memcpy(eth_hdr -> ether_shost, next_hop_mac, 6 * sizeof(uint8_t));
                memcpy(eth_hdr -> ether_dhost, brc_mac, 6 * sizeof(uint8_t));

                eth_hdr -> ether_type = htons(ETHERTYPE_ARP);

                uint32_t next_hop_interface_ip = inet_addr(get_interface_ip(nhop -> interface));

                char * buffer = buffer_arp(nhop -> next_hop, next_hop_interface_ip, eth_hdr, htons(ARPOP_REQUEST));
                send_to_link(nhop -> interface, buffer, length_arp);
                continue;
            }
            printf("Got the entry\n");
            get_interface_mac(interface, eth_hdr -> ether_shost);
            memcpy( & eth_hdr -> ether_dhost, new_arp_entry -> mac, 6 * sizeof(uint8_t));
            send_to_link(nhop -> interface, buf, len);
        }
        if (ether_type == ETHERTYPE_ARP) {
            printf("It's an ARP packet\n");
            struct arp_header * arp_hdr = (struct arp_header * )(buf + sizeof(struct ether_header));

            if (ntohs(arp_hdr -> op) == ARPOP_REQUEST) {
                printf("It's an ARP request\n");

                memcpy(eth_hdr -> ether_dhost, eth_hdr -> ether_shost, 6 * sizeof(uint8_t));

                uint8_t interface_mac[6] = {0};
                get_interface_mac(interface, interface_mac);
                memcpy(eth_hdr -> ether_shost, interface_mac, 6 * sizeof(uint8_t));

                arp_hdr -> tpa = arp_hdr -> spa;
                arp_hdr -> spa = inet_addr(get_interface_ip(interface));

                char * buffer = buffer_arp(arp_hdr -> tpa, arp_hdr -> spa, eth_hdr, htons(ARPOP_REPLY));
                send_to_link(interface, buffer, length_arp);
            }

            if (ntohs(arp_hdr -> op) == ARPOP_REPLY) {
                printf("It's an ARP reply\n");

                if (get_arp_entry(arp_hdr -> spa) == NULL) {
                    printf("da\n");
                    arp_table[arp_table_len].ip = arp_hdr -> spa;
                    memcpy(arp_table[arp_table_len].mac, arp_hdr -> sha, 6 * sizeof(uint8_t));
                    arp_table_len++;
                }

                while (!queue_empty(unsent_packets)) {
                    printf("Checking the packets\n");
                    char * buffer = queue_deq(unsent_packets);
                    struct ether_header * eth_hdr_1 = (struct ether_header * ) buffer;

                    struct arp_entry * new_arp_entry = get_arp_entry(arp_hdr -> spa);
                    if (!new_arp_entry) {
                        printf("It's null\n");
                        queue_enq(unsent_packets, buffer);
                        continue;
                    }

                    printf("Sending the packets\n");
                    u_int8_t interface_mac[6] = {0};
                    get_interface_mac(interface, interface_mac); // get interface ip

                    memcpy(eth_hdr_1 -> ether_shost, interface_mac, 6 * sizeof(uint8_t));
                    memcpy(eth_hdr_1 -> ether_dhost, new_arp_entry -> mac, 6 * sizeof(uint8_t));
                    send_to_link(interface, buffer, length_arp);
                }
            }
        }
    }
}