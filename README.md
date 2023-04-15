# Tema 1 PCom - Dataplane Router

### Ioan Teodorescu - 323CB

Tema consta in implementarea **dataplane-ului unui router**, ce presupune înțelegerea și aplicarea conceptelor de rutare și de analiză a pachetelor. Un router poate avea mai multe interfețe și poate recepționa pachete pe oricare dintre acestea, să le prelucreze și să le trimită mai departe catre alte dispozitive **direct conectate**. Tema a fost implementată in C.

## Cerinte

1. **Procesul de dirijare** 

Presupune primirea pachetului de date, analizarea acestuia si selectarea celui mai bun HOP pentru a transmite acheful către destinație. Voi discuta procesul de dirijare atunci când vom primi un pachet de tip **IPv4**.

Pentru inceput, odata ce am primit pachetul, variabila ```buf``` este convertită într-un pointer la structura *ether_header,* astfel încât informațiile din antet să pot fie analizate. Se verifica daca protocolul superior care este incapsulat in header-ul de Ethernet este unul de tip IP. În caz afirmativ, se extrage din pachet header-ul pentru IP si ICMP (este folosit mai tarziu). 

Voi incepe prin verificarea corectitudinii sumei de control. Se memoreaza valoarea checksum-ului si se recalculeaza, folosind functia checksum din cadrul API-ului pus la dispoziție [apelez ntohs pentru a converti valoarea din formatul retelei (big-endian) in formatul gazda (little-endian)]. Daca suma din header e diferita fata de cea nou calculata, asta inseamna ca pachetul a suferit modificari si trebuie aruncat. 

Apoi, urmeaza cautarea urmatorului hop. Se apeleaza ```trie_lookup``` ce are ca scop parcurgerea arborelui de cautare de tip trie, returnand un pointer la o structura ```route_table_entry```, ce ar trebuii sa contina informatiile despre next hop. Daca nhop e NULL, atunci nu a fost gasit un hop, se va trimite inapoi catre sursa pachetului un mesaj ICMP de tip "Destination unreachable” (type = 3, code = 0).

Urmatorul pas consta in verificarea si actualizarea TTL-ului. Daca e mai mic sau egal decat 1, pachetul nu mai are timp de viata si trebuie aruncat. De asemenea, se va trimite catre sursa un mesaj ICMP de tip “Time Exceeded” (type = 11, code = 0).

In acest moment, putem modifica valoarea TTL-ului si checksum-ului (din cauza lui TTL)

Router-ul trebuie sa trimită pachetul, insă acesta nu stie adresa MAC a urmatorului hop. Aici vine in ajutor Protocolul ARP. Se apeleaza get_arp_entry si cauta in cache-ul ARP daca exista vreo intrare care are adresa IP a urmatorului hop. Daca nu exista o intrare si variabila returnata e NULL, se va genera un ARP Request. 

Daca toate conditiile sunt indeplinite (avem checksum corect, ttl > 1, un urmator hop si adresa MAC a acestuia), trebuie sa actualizam header-ul de Ethernet (la sursa, punem adresa MAC a interfetei router-ului; la destinatie, adresa MAC a urmatorului hop) si se trimite pachetul 

1. **Longest Prefix Match eficient**
- Este implementat in ```trie.h``` si ```trie.c```. In ```main()```, se initializeaza un nod root si se vor adauga intrarile din fisier in trie.

Structura: 

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
    

### Toate cerintele sunt implementate, punctajul pe checker-ul local fiind de 100 de puncte.
