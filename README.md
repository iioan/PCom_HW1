# Communication Protocols Homework NO. 1 - Dataplane Router

The project consists of implementing a **dataplane for a router**, which involves understanding and applying routing and packet analysis concepts. A router can have multiple interfaces and can receive packets on any of them, process them, and forward them to other **directly connected devices**. The project has been implemented in C.

## Requirements

1. **Routing Process**

   Involves receiving a data packet, analyzing it, and selecting the best HOP to transmit the packet to its destination. I will discuss the routing process when we receive an **IPv4** packet.

   To begin, once we receive the packet, the variable `buf` is converted into a pointer to the *ether_header* structure so that the header information can be analyzed. We check whether the upper-layer protocol encapsulated in the Ethernet header is of type IP. If so, the IP and ICMP headers are extracted from the packet (used later).

   We start by checking the correctness of the checksum. We store the checksum value and recalculate it using the checksum function provided [calling ntohs to convert the value from network format (big-endian) to host format (little-endian)]. If the checksum in the header is different from the newly calculated checksum, it means that the packet has been altered and should be discarded.

   Next, we look for the next hop. We call `trie_lookup`, which aims to traverse the trie search tree, returning a pointer to a `route_table_entry` structure containing information about the next hop. If `nhop` is NULL, then no hop has been found, and we send back an ICMP message to the packet source with "Destination unreachable" (type = 3, code = 0).

   The next step is to check and update the TTL (Time To Live). If it is less than or equal to 1, the packet has no more life left and should be discarded. Additionally, an ICMP message of "Time Exceeded" (type = 11, code = 0) is sent back to the source.

   At this point, we can modify the TTL and checksum (due to TTL).

   The router needs to send the packet, but it does not know the MAC address of the next hop. Here, the ARP (Address Resolution Protocol) comes to the rescue. We call `get_arp_entry` and search the ARP cache for an entry with the IP address of the next hop. If there is no entry, and the returned variable is NULL, an ARP Request is generated.

   If all conditions are met (correct checksum, TTL > 1, a next hop, and its MAC address), we need to update the Ethernet header (put the MAC address of the router's interface as the source, and the MAC address of the next hop as the destination) and send the packet.

1. **Efficient Longest Prefix Match**

   This is implemented in `trie.h` and `trie.c`. In `main()`, a root node is initialized, and entries from a file are added to the trie.

   Structure:

   ```c
   struct trie_node {
       struct trie_node * children[2];
       struct route_table_entry * entry;
   };
   ```

1. ```trie_create()```
    - aloca memorie pentru un nou nod si il returneaza.
2. ```mask_length()```
    - calculeaza numărul de biți setați consecutiv a unei masti, incepand cu cel mai semnificativ bit.
3. ```read_trie(const char * path, struct trie_node * root)```
    - deschide fisierul si citeste fiecare intrare de router (functia are la baza read_rtable din lib.c)
    - se adauga datele intrarii intr-un entry, care mai apoi este adaugat in trie
    - se returneaza numarul de noduri.
4. ```void trie_insert(struct trie_node * root, struct route_table_entry * entry)```
    - adauga nodurile in trie
    - se ia prefixul si se calculeaza lungimea mastii (schimbam de la network order la host order)
    - se parcurge in functie de lungimea mastii (ceea ce este dupa sirul de biti setati consecutivi, nu mai conteaza..)
    - se incepe de la cel mai semnificativ (dreapta → stanga in little endian)
    - parcurge arborele trie și adaugă un nou nod în cazul în care nu există un nod corespunzător bitului curent din prefix.
    - la final, in ultimul nod este adaugat entry-ul.
5. ```struct route_table_entry * trie_lookup(struct trie_node * root, uint32_t destination)```
    - parcurgerea se incepe de la cel mai semnificativ bit (acelasi caz ca si mai sus)
    - daca nodul corespunzator bitului e null, inseamna ca am terminat de parcurs arborele in functie de bitii adresei destinatie (posibil sa avem un rezultat sau nu).
    - daca nu e null, entry-ul din nodul respectiv se adauga in rezultat
    - se trece la urmatorul nod

3. **Protocolul ARP**

Atunci cand analizam pachetul de tip IPv4, se cauta in cache-ul ARP-ului si daca nu gasim un arp entry, trebuie sa generam un ARP Request. Mai intai, pachetul este memorat intr-o coada si va fi trimis mai tarziu. Adresa hardware a sursei (MAC) va fi adresa interfeței routerului către next hop, iar pentru destinatie, adresa va fi cea de Broadcast. Functia ```buffer_arp``` (din ```protocols.c```) genereaza pachetul care va fi trimis. Vor fi adaugate 2 headere: unul de Ethernet si cel de ARP.

1. ARP Request
- Routerul primeste un pachet si verifica ```ether_type``` e cel de ARP. Daca da, verifica ```opcode-ul``` header-ului de ARP (1 - Request; 2 - Reply). Din moment ce am primit un request, in header-ul de Ethernet, pun adresa sursa in adresa destinatie, deoarece pachetul trebuie trimis prin aceeasi interfata de retea. Se afla adresa MAC a interfetei utilizate si se copiaza in adresa sursa.
- Adresa IP destinatar (```arp_hdr → tpa```) este setata la adresa ip sursa (```arp_hdr → spa```). Iar spa o sa aiba adresa IP a interfetei utilizate. Se va crea un nou buffer pentru ARP reply, care va fi trimis

b. ARP Reply

- Din moment ce am primit un reply, adresele IP si MAC ale sursei vor fi salvate in cache-ul ARP, pentru a fi folosite ulterior. Apoi, se va parcurge coada cu pachetele aflate in asteptare, deoarece nu a fost gasit un ```arp_entry``` pentru ele. Se ia varful cozii si se foloseste functia ```get_arp_entry```. Daca nici acum nu avem un ```entry```, se adauga inapoi in coada. Altfel, headerul Ethernet este modificat astfel: la ```shost```, este pusa adresa interfetei curente, si la ```dhost```, adresa MAC a entry-ului care era cautat de la bun inceput. Se trimite inapoi pachetul pentru analizare.
1. **Protocolul ICMP**

Atunci cand se va arunca un pachet, vom trimite un mesaj expeditorului. In cadrul temei, tratam 2 cazuri. 

1. **Destination unreachable (type 3, code 0)** 
    - Se trimite atunci cand nu exista ruta catre destinatie, mai exact, dupa executarea functiei trie_lookup, nu avem nicio ruta. Functia ```buffer_icmp``` din ```protocols.c``` va crea un pachet specific protocolului. Se vor initializa headere noi pentru fiecare protocol, unde vor fi transportate datele. Pentru Ethernet, se face swap intre adresele sursa si destinatie (destinatia trebuie sa-i trimita sursei mesajul de eroare). Pentru ICMP, se adauga tipul si codul erorii si se va calcula suma de control. Pentru IP, setam parametrii (swap intre ```saddr```, ```daddr```; adaugam tipul de protocol encapsulat de headerul IP) si calculam checksum. Toate aceste headere sunt puse mai apoi in buffer si odata returnat, va fi trimis.
    
    b. **Time exceeded (type 11, code 0)**
    
    - Se trimite atunci cand durata de viata a TTL-ului a expirat. Se trateaza asemenator ca situatia de sus, doar la type si code sunt puse numerele 11, respectiv 0
    
    c. **ICMP Echo Request (type 8, code 0)**
    
    - Atunci cand router-ul primeste acest mesaj, el trebuie sa trimita un Echo Reply cu ```type 0, code 0```. Insa, in noul pachet, trebuie adaugate noi informatii (in ```buffer_icmp```, liniile 42-45); routerul trebuie să se asigure că  avem aceste valori în pachetul de tip "Echo reply”
    

### Toate cerintele sunt imprementare, punctajul pe checker-ul local fiind 100 de puncte.
