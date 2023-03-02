<b>Rosu Mihai Cosmin 323CA</b>

<b>Subscriber:</b>
- Subscriber-ul incepe prin a crea un socket pe care sa comunice cu serverul.
- Dupa ce se conecteaza ii trimite serverului id-ul sau.
- Dupa aceea, folosind doua intrari multiplexate, poate primi mesaje de la
server si de la STDIN.
- De la STDIN poate primi 3 tipuri de comenzi: exit, subscribe si unsubscribe.
  - exit: Subscriber-ul inchide socket-ul pe care comunica cu serverul si apoi
 programul se opreste.
  - subscribe/unsubscribe: Subscriber-ul trimite mesajul preluat de la tastatura,
 neschimbat, catre server.
- De pe socket-ul pe care comunica cu serverul poate primi 2 tipuri de mesaje:
exit si mesaj udp.
  - exit: Subscriber-ul inchide socket-ul pe care comunica cu serverul si apoi
 programul se opreste.
  - mesaj udp: Subscriber-ul descifreaza mesajul primit incat sa fie in format-ul
 cerut, iar apoi il afiseaza.

<b>Server:</b>
- Server-ul incepe prin crearea a 2 socketi (unul pe care receptioneaza mesajele
de la clientii udp si unul pe care vin noile conexiuni de la clientii tcp).
- Dupa crearea socketilor serverul bind-uieste socketii la adresa lui si face
socket-ul tcp sa fie pasiv (sa asculte pentru noi conexiuni tcp).
- Inainte sa inceapa primirea mesajelor, serverul creeaza cate o lista pentru
clienti tcp, topicuri si mesaje udp netrimise tuturor abonatilor.
- Dupa aceea, folosind intrari multiplexate, serverul poate primi mesaje de la
STDIN, clienti udp si clienti tcp.
- De la STDIN poate primi doar mesajul exit.
  - exit: Serverul trimite tuturor clientilor tcp mesajul exit, iar apoi
 elibereaza memoria alocata pentru elementele listelor si liste, si inchide
 toti socketii, dupa care programul se opreste.
- De la clientii udp primeste mesaje pe socket-ul udp.
  - mesaj udp: Serverul primeste mesajul folosind recvfrom, cu care obtine si
 adresa clientului udp care a trimis mesajul. Dupa aceea ataseaza la inceputul
 mesajului adresa si portul clientului udp care a trimis mesajul si il trimite
 tuturor clientilor tcp abonati la topicul mesajului primit. Daca exista
 clienti tcp abonati cu SF la acest topic, dar sunt deconectati, mesajul va fi
 salvat in lista de mesaje.
- De la clientii tcp poate primii 3 tipuri de mesaje: id, subscribe si
unsubscribe.
  - id: Serverul verifica daca in lista clientilor tcp exista vreo intrare care
 sa aiba id-ul primit. De aici rezulta 3 cazuri:  
  1.Nu exista: In acest caz se copiaza id-ul in campul clientului tcp care are
  socket-ul egal cu cel pe care a venit mesajul.  
  2.Exista, dar este deconectat: In acest caz se copiaza detaliile din clientul
  tcp curent(cel cu socket-ul pe care a venit mesajul) in clientul tcp cu id-ul
  cautat, iar clientul de pe care s-a copiat este sters din lista. (Acum
  intrarea din lista care reprezinta clientul este cea in care s-a copiat.)  
  3.Exista, dar este conectat: In acest caz noul client nu se poate conecta,
  deci intrarea sa din lista clientilor va fi stearsa, si serverul ii trimite
  mesajul exit pentru ca acesta sa se opreasca.
  - subscribe: Serverul cauta in lista topicurilor daca exista topicul la care
 clientul doreste sa se conecteze. Daca exista topicul, se va adauga id-ul
 clientului in lista id-urilor abonate la acest topic, cu SF-ul egal cu 1 sau 0
 in functie de comanda trimisa de client. Altfel, se creaza o noua intrare in
 mod analog ce va fi adaugata in lista topicurilor.
  - unsubscribe: Serverul cauta in topicul in lista si, cand este gasit, scoate
 id-ul clientului din lista id-urilor abonate.
- Pe socket-ul tcp pasiv serverul poate primi requesturi de la clienti tcp
pentru a se conecta. In acest caz, serverul ii accepta si creaza o noua intrare
in lista clientilor tcp, care are id-ul nesetat. (Va fi setat, daca este cazul,
cand serverul primeste mesajul de tip id din partea clientului.)
- Serverul, la fiecare iteratie, incearca sa trimita mesajele din lista, prin
verificarea daca intre timp s-au reconectat clientii carora trebuie trimis
mesajul. Cand un mesaj este trimis tuturor clientilor care trebuiau sa il
primeasca, este scos din lista mesajelor.

<b>Protocol la nivel aplicatie:</b>
- Toate mesajele ce sunt trimise intre server si clientii tcp respecta regula de
a se trimite mai intai lungimea mesajului (pe doi octeti), iar apoi se trimite
cintinutul mesajului, cu lungimea exacta (pentru a nu fi trimisi octeti nuli
fara vreun rost).
- Prin urmare, atat subscriberii, cat si serverul are grija sa receptioneze mai
intai dimensiunea mesajului si apoi continutul mesajului.
- Atunci cand serverul primeste un mesaj de la clientii udp, lungimea mesajului
este reprezentata de return-ul functiei recvfrom (cati octeti au fost primiti).
- Pentru ca un client sa transmita serverului id-ul lui, dupa ce se conecteaza
ii trimite serverului un mesaj de tip id. La randul lui, serverul are grija sa
nu poata fi doi clienti tcp conectati cu acelasi id simultan.
- Serverul retine ce clienti sunt abonati la fiecare topic, si daca sunt abonati
cu SF sau fara. Pentru fiecare mesaj udp primit serverul are grija sa nu piarda
mesajul pana nu este trimis tuturor clientilor tcp care erau abonati cu SF la
topicul respectiv la acel moment.
