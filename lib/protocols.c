#include "protocols.h"
#include "lib.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define ETHERTYPE_IP 0x0800 /* IP */


char *buffer_arp(uint32_t daddr, uint32_t saddr, struct ether_header *eth_hdr, uint16_t op) {
    char *buffer = (char *) malloc(sizeof(struct arp_header) + sizeof(struct ether_header));
    struct arp_header* arp_hdr = (struct arp_header *) malloc(sizeof(struct arp_header));
    arp_hdr -> htype = htons(1);
    arp_hdr -> ptype = htons(ETHERTYPE_IP);
    arp_hdr -> hlen = 6;
    arp_hdr -> plen = 4;
    arp_hdr -> op = op;
    memcpy(arp_hdr -> sha, eth_hdr -> ether_shost, 6 * sizeof(u_int8_t));
    memcpy(arp_hdr -> tha, eth_hdr -> ether_dhost, 6 * sizeof(uint8_t));

    arp_hdr -> spa = saddr;
    arp_hdr -> tpa = daddr;

    memcpy(buffer, eth_hdr, sizeof(struct ether_header));
    memcpy(buffer + sizeof(struct ether_header), arp_hdr, sizeof(struct arp_header));
    return buffer;
}


char *buffer_icmp(struct iphdr *ip_hdr, struct ether_header *eth_hdr, struct icmphdr *icmp_hdr, uint8_t type, uint8_t code) {
    char *buffer = (char *) malloc(sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr));
    struct ether_header *eth_hdr_new = (struct ether_header *) malloc(sizeof(struct ether_header));
    struct icmphdr *icmp_hdr_new = (struct icmphdr*) malloc(sizeof(struct icmphdr));
    struct iphdr *ip_hdr_new = (struct iphdr *) malloc(sizeof(struct iphdr));

    memcpy(eth_hdr_new -> ether_dhost, eth_hdr -> ether_shost, 6 * sizeof(uint8_t));
    memcpy(eth_hdr_new -> ether_shost, eth_hdr -> ether_dhost, 6 * sizeof(uint8_t));
    eth_hdr_new -> ether_type = eth_hdr -> ether_type;

    icmp_hdr_new -> type = type;
    icmp_hdr_new -> code = code;
    if (icmp_hdr -> type == 8 && icmp_hdr -> code == 0) {
        icmp_hdr_new -> un.echo.id = icmp_hdr -> un.echo.id;
        icmp_hdr_new -> un.echo.sequence = icmp_hdr -> un.echo.sequence;
    }
    icmp_hdr_new -> checksum = 0;
    icmp_hdr_new -> checksum = checksum((uint16_t*) icmp_hdr_new, sizeof(struct icmphdr));

    ip_hdr_new -> tos = 0;
    ip_hdr_new -> tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
    ip_hdr_new -> id = htons(1);
    ip_hdr_new -> frag_off = 0;
    ip_hdr_new -> ttl = 64;
    ip_hdr_new -> protocol = IPPROTO_ICMP;
    ip_hdr_new -> saddr = ip_hdr -> daddr;
    ip_hdr_new -> daddr = ip_hdr -> saddr;
    ip_hdr_new -> check = 0;
    ip_hdr_new -> check = checksum((uint16_t *) ip_hdr_new, sizeof(struct iphdr));

    memcpy(buffer, eth_hdr_new, sizeof(struct ether_header));
    memcpy(buffer + sizeof(struct ether_header), ip_hdr_new, sizeof(struct iphdr)); 
    memcpy(buffer + sizeof(struct ether_header) + sizeof(struct iphdr), icmp_hdr_new, sizeof(struct icmphdr));

    return buffer;
}