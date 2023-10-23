Topic 1 PCom - Dataplane Router
Ioan Teodorescu - 323CB

The project consists of implementing a dataplane for a router, which involves understanding and applying routing and packet analysis concepts. A router can have multiple interfaces and can receive packets on any of them, process them, and forward them to other directly connected devices. The project has been implemented in C.
Requirements

    Routing Process

    Involves receiving a data packet, analyzing it, and selecting the best HOP to transmit the packet to its destination. I will discuss the routing process when we receive an IPv4 packet.

    To begin, once we receive the packet, the variable buf is converted into a pointer to the ether_header structure so that the header information can be analyzed. We check whether the upper-layer protocol encapsulated in the Ethernet header is of type IP. If so, the IP and ICMP headers are extracted from the packet (used later).

    We start by checking the correctness of the checksum. We store the checksum value and recalculate it using the checksum function provided [calling ntohs to convert the value from network format (big-endian) to host format (little-endian)]. If the checksum in the header is different from the newly calculated checksum, it means that the packet has been altered and should be discarded.

    Next, we look for the next hop. We call trie_lookup, which aims to traverse the trie search tree, returning a pointer to a route_table_entry structure containing information about the next hop. If nhop is NULL, then no hop has been found, and we send back an ICMP message to the packet source with "Destination unreachable" (type = 3, code = 0).

    The next step is to check and update the TTL (Time To Live). If it is less than or equal to 1, the packet has no more life left and should be discarded. Additionally, an ICMP message of "Time Exceeded" (type = 11, code = 0) is sent back to the source.

    At this point, we can modify the TTL and checksum (due to TTL).

    The router needs to send the packet, but it does not know the MAC address of the next hop. Here, the ARP (Address Resolution Protocol) comes to the rescue. We call get_arp_entry and search the ARP cache for an entry with the IP address of the next hop. If there is no entry, and the returned variable is NULL, an ARP Request is generated.

    If all conditions are met (correct checksum, TTL > 1, a next hop, and its MAC address), we need to update the Ethernet header (put the MAC address of the router's interface as the source, and the MAC address of the next hop as the destination) and send the packet.

    Efficient Longest Prefix Match

    This is implemented in trie.h and trie.c. In main(), a root node is initialized, and entries from a file are added to the trie.

    Structure:

    c

    struct trie_node {
        struct trie_node * children[2];
        struct route_table_entry * entry;
    };

        trie_create()
            Allocates memory for a new node and returns it.
        mask_length()
            Calculates the number of consecutive set bits in a mask, starting with the most significant bit.
        read_trie(const char * path, struct trie_node * root)
            Opens the file and reads each router entry (the function is based on read_rtable from lib.c).
            Adds the entry's data into a structure, which is then added to the trie.
            Returns the number of nodes.
        void trie_insert(struct trie_node * root, struct route_table_entry * entry)
            Adds nodes to the trie.
            Takes the prefix and calculates the mask length (changing from network order to host order).
            Traverses based on the mask length (what comes after the consecutive set bits in the mask doesn't matter).
            Starts from the most significant bit (right to left in little-endian).
            Traverses the trie and adds a new node if there is no node corresponding to the current bit in the prefix.
            The entry is added to the last node.
        struct route_table_entry * trie_lookup(struct trie_node * root, uint32_t destination)
            Traversal starts from the most significant bit (the same case as above).
            If the node corresponding to the bit is null, it means that the tree has been traversed based on the destination address bits (we may or may not have a result).
            If it's not null, the entry from that node is added to the result.
            Move on to the next node.

    ARP Protocol

    When analyzing an IPv4 packet, we look in the ARP cache, and if we don't find an ARP entry, we need to generate an ARP Request. First, the packet is stored in a queue and will be sent later. The hardware address (MAC) of the source is set to the router's interface address for the next hop, and the destination address is set to the broadcast address. The buffer_arp function (in protocols.c) generates the packet that will be sent. Two headers are added: one for Ethernet and one for ARP.

        ARP Request
            The router receives a packet and checks if ether_type is an ARP type. If so, it checks the opcode in the ARP header (1 - Request; 2 - Reply). Since we received a request, the source address in the Ethernet header is set as the destination address because the packet needs to be sent through the same network interface. The MAC address of the interface used is obtained and copied to the source address.
            The recipient's IP address (arp_hdr → tpa) is set to the source IP address (arp_hdr → spa). And spa will have the IP address of the used interface. A new buffer for the ARP reply is created, which will be sent.

        ARP Reply
            Since we received a reply, the source's IP and MAC addresses are saved in the ARP cache for later use. Then, the queue of pending packets is traversed because no arp_entry was found for them. The top of the queue is taken, and the get_arp_entry function is used. If there is still no entry, it is added back to the queue. Otherwise, the Ethernet header is modified with the current interface's address for shost and the MAC address of the entry sought from the beginning for dhost. The packet is sent back for analysis.

    ICMP Protocol

    When a packet is discarded, a message will be sent to the sender. Within this project, two cases are handled.

        Destination Unreachable (type 3, code 0)
            Sent when there is no route to the destination, specifically after executing the trie_lookup function and not finding any route. The buffer_icmp function in protocols.c creates a specific ICMP protocol packet. New headers are initialized for each protocol, carrying the data. For Ethernet, the source and destination addresses are swapped (the destination must send an error message to the source). For ICMP, the error type and code are added, and the checksum is calculated. For IP, parameters are set (swapping saddr, daddr; adding the encapsulated protocol type to the IP header), and the checksum is calculated. All these headers are then placed in a buffer and returned to be sent.

        Time Exceeded (type 11, code 0)
            Sent when the TTL's lifetime has expired. It is treated similarly to the situation above, with type and code set to 11 and 0, respectively.

        ICMP Echo Request (type 8, code 0)
            When the router receives this message, it needs to send an Echo Reply with type 0, code 0. However, new information needs to be added to the packet (in buffer_icmp, lines 42-45). The router needs to ensure that these values are present in the "Echo reply" packet.

All requirements are implemented, with a local checker score of 100 points.
