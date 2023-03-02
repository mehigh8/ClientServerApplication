// Rosu Mihai Cosmin 323CA
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "list.h"
#define BUFFLEN 2000
#define MAX_CLIENTS 100
#define ID_SIZE 10
#define TOPIC_SIZE 50

// Structura care retine detalii despre un client tcp.
struct tcp_client {
	struct sockaddr_in address; // Adresa clientului
	char id[ID_SIZE]; // Id-ul clientului
	uint8_t connected; // Variabila care retine daca clientul este conectat sau nu
	int socket; // Socket-ul clientului
};

// Structura care retine detalii despre un mesaj udp.
struct message {
	struct sockaddr_in address; // Adresa de la care a venit mesajul
	int store_count; // Numarul de clienti abonati cu SF la topicul mesajului,
					 // care inca nu au primit mesajul
	short len; // Lungimea mesajului
	char topic[TOPIC_SIZE]; // Topicul mesajului
	char payload[BUFFLEN]; // Continutul mesajului
	struct list* ids; // Lista cu id-urile clientilor la care mai trebuie trimis mesajul
};

// Structura care retine detalii despre un topic.
struct topic {
	char topic[TOPIC_SIZE]; // Topicul
	int store_count; // Numarul de clienti abonati la topic cu SF
	struct list* ids; // Lista cu id-urile clientilor abonati la topic (cu sau fara SF)
};

// Structura care retine detalii despre un id, folosita in cadrul listelor
// din message si topic.
struct id {
	char id[ID_SIZE]; // Id-ul
	int sf; // Variabila care retine daca id-ul este abonat cu SF sau nu la topicul in cauza.
};

/*
 * Functie care receptioneaza un mesaj udp si il retine atasandu-i
 * adresa ip si portul pe care a fost primit la inceput.
 * @msg - adresa unde va fi salvat mesajul
 * @sockfd - socket-ul pe care este primit mesajul udp
 */
void recv_udp(struct message *msg, int sockfd)
{
	char buffer[BUFFLEN];
	memset(buffer, 0, sizeof(buffer));

	struct sockaddr_in client_address;
	socklen_t addrlen = sizeof(client_address);
	// Preiau mesajul intr-un buffer.
	short buflen = recvfrom(sockfd, buffer, sizeof(buffer), 0,
				   (struct sockaddr *) &client_address, &addrlen);
	if (buflen < 0) {
		fprintf(stderr, "recv from udp\n");
		exit(0);
	}

	// Resetez memoria unde va fi salvat mesajul.
	memset(msg, 0, sizeof(struct message));
	// Copiez topicul, continutul si adresa mesajului.
	memcpy(msg->topic, buffer, TOPIC_SIZE);
	memcpy(msg->payload, buffer, buflen);
	memcpy(&(msg->address), &client_address, sizeof(client_address));
	// Creez lista de id-uri.
	msg->ids = list_create(sizeof(struct id));
	// Resetez buffer-ul.
	memset(buffer, 0, sizeof(buffer));
	// Copiez in buffer adresa de pe care a fost primit mesajul.
	strcpy(buffer, inet_ntoa(client_address.sin_addr));
	buffer[strlen(buffer)] = ':';
	// Transform portul din network byte order in host byte order si apoi
	// il transform in char-uri.
	char port[20];
	memset(port, 0, sizeof(port));
	snprintf(port, sizeof(port), "%hu", ntohs(client_address.sin_port));
	// Concatenez portul la buffer.
	strcat(buffer, port);
	buffer[strlen(buffer)] = ' ';
	// Preiau lungimea buffer-ului cu ip si port.
	int len = strlen(buffer);

	// Copiez la sfarsitul bufferului continutul mesajului primit.
	memcpy(buffer + len, msg->payload, buflen);
	// Actualizez lungimea si continutul mesajului.
	buflen += len;
	msg->len = buflen;
	memcpy(msg->payload, buffer, buflen);
}

/*
 * Functie care trimite mesajul udp clientilor abonati la topic (care sunt conectati).
 * @msg - adresa mesajului udp
 * @topics - lista de topicuri
 * @tcps - lista clientilor tcp
 */
void send_udp(struct message *msg, struct list *topics, struct list *tcps)
{
	// Caut topicul mesajului in lista de topicuri.
	struct node* current_topic = topics->head;
	while (current_topic != NULL) {
		// Verific daca am gasit topicul cautat.
		if (strncmp(((struct topic *)current_topic->data)->topic, msg->topic, TOPIC_SIZE) == 0) {
			// Setez store count-ul mesajului la numarul de clienti abonati la topic cu SF.
			msg->store_count = ((struct topic *)current_topic->data)->store_count;
			// Parcurg lista de id-uri abonate la topic.
			struct node* current_id = ((struct topic *)current_topic->data)->ids->head;
			while (current_id != NULL) {
				// Caut clientul tcp care are asociat id-ul curent.
				struct node* current_tcp = tcps->head;
				while (current_tcp != NULL) {
					// Verific daca am gasit clientul tcp.
					if (strncmp(((struct tcp_client *)current_tcp->data)->id,
								((struct id *)current_id->data)->id, ID_SIZE) == 0) {
						// Verific daca clientul este conectat.
						if (((struct tcp_client *)current_tcp->data)->connected == 1) {
							// In acest caz ii trimit mesajul (mai intai lungimea
							// si apoi continutul).
							if (send(((struct tcp_client *)current_tcp->data)->socket, &msg->len,
								sizeof(uint16_t), 0) < 0) {
								fprintf(stderr, "send tcp from udp\n");
								exit(0);
							}
							if (send(((struct tcp_client *)current_tcp->data)->socket,
								msg->payload, msg->len, 0) < 0) {
								fprintf(stderr, "send tcp from udp\n");
								exit(0);
							}
							// Daca clientul era abonat cu SF decrementez store count-ul mesajului.
							if (((struct id *)current_id->data)->sf == 1) {
								msg->store_count--;
							}
						// Daca clientul nu era conectat dar este abonat cu SF, ii pun id-ul in
						// lista id-urilor mesajului.
						} else if (((struct id *)current_id->data)->sf == 1) {
							list_add_nth_node(msg->ids, 0, current_id->data);
						}
						break;
					}
					current_tcp = current_tcp->next;
				}
				current_id = current_id->next;
			}
			break;
		}
		current_topic = current_topic->next;
	}
}

/*
 * Functie care aboneaza/dezaboneaza clientii tcp de la topicuri in functie de mesajul primit.
 * @buffer - buffer-ul care contine mesajul
 * @tcp - clientul tcp care a trimit mesajul
 * @topics - lista de topicuri
 */
void manage_subscription(char *buffer, struct tcp_client *tcp, struct list* topics)
{
	// Verific daca mesajul este de tip subscribe.
	if (strncmp(buffer, "subscribe", 9) == 0) {
		// Sparg mesajul in mai multe parti si preiau topicul la care vrea clientul sa se aboneze,
		// pe care il retin in variabila topic.
		char *word = strtok(buffer, " ");
		word = strtok(NULL, " ");
		char topic[TOPIC_SIZE];
		strncpy(topic, word, TOPIC_SIZE);
		// Trec la urmatoarea parte din mesaj (SF-ul).
		word = strtok(NULL, " ");

		// Caut in lista de topicuri daca exista deja topicul.
		struct node *current_topic = topics->head;
		int found = 0;
		while (current_topic != NULL) {
			// Daca l-am gasit, creez un nou struct id cu id-ul clientului si SF-ul, pe care il
			// adaug in lista id-urilor topicului.
			if (strncmp(((struct topic *) current_topic->data)->topic, topic, TOPIC_SIZE) == 0) {
				found = 1;
				struct id newid;
				strncpy(newid.id, tcp->id, ID_SIZE);
				newid.sf = 0;
				// Veriric daca SF-ul este 1, caz in care actualizez store count-ul topicului.
				if (word[0] == '1') {
					((struct topic *) current_topic->data)->store_count++;
					newid.sf = 1;
				}
				list_add_nth_node(((struct topic *) current_topic->data)->ids, 0, &newid);
				break;
			}
			current_topic = current_topic->next;
		}
		// Daca nu am gasit topicul in lista, inseamna ca trebuie sa il creez.
		if (found == 0) {
			// Creez topicul si il populez.
			struct topic newtopic;
			memset(&newtopic, 0, sizeof(struct topic));
			strncpy(newtopic.topic, topic, TOPIC_SIZE);
			// Creez lista id-urilor.
			newtopic.ids = list_create(sizeof(struct id));
			// Creez noul struct id.
			struct id newid;
			strncpy(newid.id, tcp->id, ID_SIZE);
			newid.sf = 0;
			// Verific daca SF-ul este 1, caz in care actualizez store count-ul topicului.
			if (word[0] == '1') {
				newtopic.store_count++;
				newid.sf = 1;
			}
			// Adaug noul id in lista id-urilor.
			list_add_nth_node(newtopic.ids, 0, &newid);
			// Adaug noul topic in lista topicurilor.
			list_add_nth_node(topics, 0, &newtopic);
		}
	// Verific daca mesajul este de tip unsubscribe.
	} else if (strncmp(buffer, "unsubscribe", 11) == 0) {
		// Preiau topicul de la care vrea clientul sa se dezaboneze.
		char *word = strtok(buffer, " ");
		word = strtok(NULL, " ");
		char topic[TOPIC_SIZE];
		strncpy(topic, word, TOPIC_SIZE);

		// Caut topicul in lista de topicuri.
		struct node *current_topic = topics->head;
		while (current_topic != NULL) {
			// Verific daca am gasit topicul.
			if (strncmp(((struct topic *) current_topic->data)->topic, topic, TOPIC_SIZE) == 0) {
				// In acest caz, parcurg lista id-urilor din topic.
				struct node* current_id = ((struct topic *) current_topic->data)->ids->head;
				for (int j = 0; j < ((struct topic *) current_topic->data)->ids->size; j++) {
					// Daca am gasit id-ul clientului, il scot din lista.
					if (strncmp(((struct id *)current_id->data)->id, tcp->id, ID_SIZE) == 0) {
						// Daca clientul era abonat cu SF decrementez store countul topicului.
						if (((struct id *)current_id->data)->sf == 1)
							((struct topic *) current_topic->data)->store_count--;
						struct node* aux =
							list_remove_nth_node(((struct topic *) current_topic->data)->ids, j);
						// Eliberez memoria nodului.
						free(aux->data);
						free(aux);
					}
					current_id = current_id->next;
				}
				break;
			}
			current_topic = current_topic->next;
		}
	}
}

/*
 * Functie care trimite mesajele udp ramase in lista (daca s-au conectat clientii).
 * @messages - lista mesajelor udp
 * @tcps - lista clientilor tcp
 */
void send_udp_remaining(struct list* messages, struct list* tcps)
{
	// Parcurg lista mesajelor udp.
	struct node* current_msg = messages->head;
	while (current_msg != NULL) {
		// Parcurg lista id-urilot din mesajul curent.
		struct node* msg_id = ((struct message *)current_msg->data)->ids->head;
		while (msg_id != NULL) {
			int sent = 0;
			// Caut clientul tcp cu id-ul curent.
			struct node* current_tcp = tcps->head;
			while (current_tcp != NULL) {
				// Verific daca id-ul clientului tcp curent este egal cu id-ul cautat.
				if (strncmp(((struct tcp_client *)current_tcp->data)->id,
							((struct id *)msg_id->data)->id, ID_SIZE) == 0) {
					// Verific daca clientul este conectat.
					if (((struct tcp_client *)current_tcp->data)->connected == 1) {
						// In acest caz trimit mesajul (lungime si continut).
						if (send(((struct tcp_client *)current_tcp->data)->socket,
							&(((struct message *)current_msg->data)->len),
							sizeof(uint16_t), 0) < 0) {

							fprintf(stderr, "send tcp msg afterwards len\n");
							exit(0);
						}
						if (send(((struct tcp_client *)current_tcp->data)->socket,
							((struct message *)current_msg->data)->payload,
							((struct message *)current_msg->data)->len, 0) < 0) {

							fprintf(stderr, "send tcp msg afterwards\n");
							exit(0);
						}
						// Decrementez store count-ul mesajului deoarece doar clientii abonati
						// cu SF pot ajunge in lista id-urilor.
						((struct message *)current_msg->data)->store_count--;
						sent = 1;

						// Parcurg lista id-urilor pentru a scoate idd-ul clientului la care
						// tocmai am trimis.
						struct node* aux = ((struct message *)current_msg->data)->ids->head;
						for (int j = 0;
							j < ((struct message *)current_msg->data)->ids->size; j++) {
							if (aux == msg_id) {
								// Inainte sa elimin id-ul, trec la urmatorul id cu msg_id.
								msg_id = msg_id->next;
								aux = list_remove_nth_node(
									  ((struct message *)current_msg->data)->ids, j);
								// Eliberez memoria nodului.
								free(aux->data);
								free(aux);
								break;
							}
						}
					}
					break;
				}
				current_tcp = current_tcp->next;
			}
			// Daca am trimis mesajul iteratia asta nu mai actualizez msg_id-ul deoarece
			// o fac cand elimin din lista.
			if (sent == 0)
				msg_id = msg_id->next;
		}
		// Verific daca store count-ul mesajului a ajuns la 0, caz in care il elimin din lista.
		if (((struct message *)current_msg->data)->store_count == 0) {
			struct node* aux = messages->head;
			for (int j = 0; j < messages->size; j++) {
				if (aux == current_msg) {
					current_msg = current_msg->next;
					aux = list_remove_nth_node(messages, j);
					list_free(&((struct message *)aux->data)->ids);
					// Eliberez memoria nodului.
					free(aux->data);
					free(aux);
					break;
				}
				aux = aux->next;
			}
		} else {
			current_msg = current_msg->next;
		}
	}
}

int main(int argc, char *argv[])
{
	// Dezactivez buffering-ul la afisare.
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc < 2) {
		fprintf(stderr, "Usage: ./server <PORT_SERVER>\n");
		return 0;
	}

	// Creez doua multimi de file descriptori pe care le resetez.
	fd_set read_fds, tmp_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	// Creez socket-ul pentru clientii udp.
	int sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd_udp < 0) {
		fprintf(stderr, "socket udp\n");
		return 0;
	}
	// Creez socket-ul pentru clientii tcp.
	int sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd_tcp < 0) {
		fprintf(stderr, "socket tcp\n");
		return 0;
	}
	// Dezactivez algoritmul lui Nagle.
	int flag = 1;
	if (setsockopt(sockfd_tcp, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0) {
		fprintf(stderr, "tcp no delay\n");
		return 0;
	}
	// Pregatesc adresa serverului.
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(atoi(argv[1]));
	// Setez adresa pe INADDR_ANY pentru a se putea conecta clienti de la orice adresa.
	server_address.sin_addr.s_addr = INADDR_ANY;

	// Fac bind celor doua socket-uri la adresa.
	if (bind(sockfd_udp, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
		fprintf(stderr, "bind udp\n");
		return 0;
	}
	if (bind(sockfd_tcp, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
		fprintf(stderr, "bind tcp\n");
		return 0;
	}
	// Setez socket-ul de tcp sa fie pasiv (sa asculte pentru conexiuni noi).
	if (listen(sockfd_tcp, MAX_CLIENTS) < 0) {
		fprintf(stderr, "listen tcp\n");
		return 0;
	}

	// Setez cele doua socket-uri si socket-ul 0 (stdin).
	FD_SET(sockfd_udp, &read_fds);
	FD_SET(sockfd_tcp, &read_fds);
	FD_SET(0, &read_fds);
	int fdmax = (sockfd_udp > sockfd_tcp ? sockfd_udp : sockfd_tcp);

	char buffer[BUFFLEN];
	// Creez lista de clienti tcp.
	struct list* tcps = list_create(sizeof(struct tcp_client));
	// Creez lista de topicuri.
	struct list* topics = list_create(sizeof(struct topic));
	// Creez lista de mesaje udp.
	struct list* messages = list_create(sizeof(struct message));

	short buflen;

	while (1) {
		tmp_fds = read_fds;
		// Selectez file descriptorii pe care am primit mesaje.
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) < 0) {
			fprintf(stderr, "select\n");
			return 0;
		}

		for (int i = 0; i <= fdmax; i++) {
			// Verific daca pe socket-ul i este setat (a primit mesaj).
			if (FD_ISSET(i, &tmp_fds)) {
				// Verific daca mesajul a venit de la stdin.
				if (i == 0) {
					memset(buffer, 0, sizeof(buffer));
					// Preiau mesajul.
					if (read(0, buffer, sizeof(buffer) - 1) < 0) {
						fprintf(stderr, "read\n");
						return 0;
					}
					// Verific daca mesajul este de tip exit.
					if (strncmp(buffer, "exit", 4) == 0) {
						// In acest caz, parcurg lista clientilor tcp.
						struct node* node = tcps->head;
						while (node != NULL) {
							memset(buffer, 0, sizeof(buffer));
							strcpy(buffer, "exit");
							buflen = strlen(buffer);
							// Daca clientul este deconectat pot trece la urmatorul.
							if (((struct tcp_client *)node->data)->connected == 0) {
								node = node->next;
								continue;
							}
							// Altfel, ii trimit clientului mesajul exit.
							if (send(((struct tcp_client *) node->data)->socket, &buflen,
								sizeof(uint16_t), 0) < 0) {

								fprintf(stderr, "send tcp exit len\n");
								return 0;
							}
							if (send(((struct tcp_client *) node->data)->socket, buffer,
								buflen, 0) < 0) {

								fprintf(stderr, "send tcp exit\n");
								return 0;
							}
							// Inchid socket-ul clientului.
							close(((struct tcp_client *) node->data)->socket);
							node = node->next;
						}
						// Eliberez memoria alocata dinamic pentru toate listele folosite.
						struct node* topic = topics->head;
						while (topic != NULL) {
							list_free(&((struct topic *) topic->data)->ids);
							topic = topic->next;
						}

						struct node* msg = messages->head;
						while (msg != NULL) {
							list_free(&((struct message *) msg->data)->ids);
							msg = msg->next;
						}

						list_free(&topics);
						list_free(&messages);
						list_free(&tcps);
						// Inchid socket-ul pentru udp si cel pasiv pentru tcp.
						close(sockfd_tcp);
						close(sockfd_udp);
						return 0;
					}
				// Verific daca se conecteaza un nou client tcp.
				} else if (i == sockfd_tcp) {
					// In acest caz, il accept.
					struct sockaddr_in client_address;
					socklen_t addrlen = sizeof(client_address);

					int newsockfd = accept(sockfd_tcp, (struct sockaddr *) &client_address,
						&addrlen);
					if (newsockfd < 0) {
						fprintf(stderr, "accept tcp\n");
						return 0;
					}
					// Dezactivez algoritmul lui Nagle.
					if (setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY,
						(char *)&flag, sizeof(int)) < 0) {

						fprintf(stderr, "tcp no delay\n");
						return 0;
					}
					// Adaug noul socket in multimea file descriptorilor.
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax)
						fdmax = newsockfd;

					// Creez o noua intrare pentru lista clientilor tcp,
					// care inca nu are id-ul setat.
					struct tcp_client newtcp;
					memset(&newtcp, 0, sizeof(struct tcp_client));
					memcpy(&newtcp.address, &client_address, sizeof(client_address));
					newtcp.connected = 1;
					newtcp.socket = newsockfd;
					// Adaug noul client tcp in lista.
					list_add_nth_node(tcps, 0, &newtcp);
				// Verific daca mesajul este venit de la un client udp.
				} else if (i == sockfd_udp) {
					// In acest caz receptionez mesajul in variabila msg.
					struct message msg;
					recv_udp(&msg, i);
					// Trimit mesajul clientilor tcp (care sunt conectati).
					send_udp(&msg, topics, tcps);
					// Daca mesajul nu a fost trimis tuturor clientilor abonati cu SF,
					// il adaug in lista de mesaje.
					if (msg.store_count > 0)
						list_add_nth_node(messages, 0, &msg);
					// Altfel eliberez memoria listei de id-uri.
					else
						list_free(&msg.ids);
				// Altfel, inseamna ca mesajul primit este de la un client tcp.
				} else {
					memset(buffer, 0, sizeof(buffer));
					buflen = 0;
					// Preiau lungimea mesajului.
					int n = recv(i, &buflen, sizeof(uint16_t), 0);
					if (n < 0) {
						fprintf(stderr, "recv tcp len\n");
						return 0;
					}
					// Retin in tcp clientul care a trimis mesajul.
					struct tcp_client* tcp;
					struct node* current_tcp = tcps->head;
					while (current_tcp != NULL) {
						if (((struct tcp_client *) current_tcp->data)->socket == i) {
							tcp = (struct tcp_client *) current_tcp->data;
							break;
						}
						current_tcp = current_tcp->next;
					}
					// Daca numarul de octeti receptionati cand am preluat lungimea
					// mesajului este 0, inseamna ca clientul tcp s-a deconectat.
					if (n == 0) {
						tcp->connected = 0;
						printf("Client %s disconnected.\n", tcp->id);
						// Scot socket-ul din multime si il inchid.
						FD_CLR(i, &read_fds);
						close(i);
						continue;
					}
					// Altfel, preiau si mesajul.
					if (recv(i, buffer, buflen, 0) < 0) {
						fprintf(stderr, "recv tcp\n");
						return 0;
					}

					// Verific daca measjul este de tip ID.
					if (strncmp(buffer, "ID: ", 4) == 0) {
						current_tcp = tcps->head;
						int found = 0;
						// In acest caz, caut in lista de clienti tcp (care sunt conectati sau
						// deconectati) daca gasesc id-ul.
						while (current_tcp != NULL) {
							if (strncmp(((struct tcp_client *) current_tcp->data)->id, buffer + 4,
								ID_SIZE) == 0) {
								// Daca clientul cu acelasi id este deja conectat, inseamna ca noul
								// client nu se poate conecta.
								if (((struct tcp_client *) current_tcp->data)->connected == 1) {
									// In acest caz, afisez mesajul corespunzator.
									printf("Client %s already connected.\n",
										   ((struct tcp_client *) current_tcp->data)->id);
									// Retin ca l-am gasit.
									found = 1;
									// Parcurg lista clientilor tcp pentru a scoate intrarea noului
									// client tcp.
									current_tcp = tcps->head;
									for (int j = 0; j < tcps->size; j++) {
										if (((struct tcp_client*)current_tcp->data)->socket == i) {
											struct node* aux = list_remove_nth_node(tcps, j);
											free(aux->data);
											free(aux);

											memset(buffer, 0, sizeof(buffer));
											// Ii trimit noului client mesajul exit.
											strcpy(buffer, "exit");
											buflen = strlen(buffer);
											if (send(i, &buflen, sizeof(uint16_t), 0) < 0) {
												fprintf(stderr, "send exit tcp len\n");
												return 0;
											}
											if (send(i, buffer, buflen, 0) < 0) {
												fprintf(stderr, "send exit tcp\n");
												return 0;
											}

											// Scot socket-ul din multime si il inchid.
											FD_CLR(i, &read_fds);
											close(i);
											break;
										}
										current_tcp = current_tcp->next;
									}
								// Altfel, daca este deconectat clientul cu acelasi id, copiez
								// detaliile noului client in cel vechi.
								} else {
									struct node* new_node = tcps->head;
									for (int j = 0; j < tcps->size; j++) {
										if (((struct tcp_client *) new_node->data)->socket == i) {
											// Scot din lista noul client.
											struct node* aux = list_remove_nth_node(tcps, j);
											// Copiez datele.
											memcpy(
											&((struct tcp_client *)(current_tcp->data))->address,
											&((struct tcp_client *)aux->data)->address,
											sizeof(struct sockaddr));
											((struct tcp_client*)current_tcp->data)->socket = i;
											((struct tcp_client*)current_tcp->data)->connected = 1;
											// Schimb clientul spre care pointeaza tcp.
											tcp = (struct tcp_client *)current_tcp->data;

											// Eliberez memoria.
											free(aux->data);
											free(aux);
											break;
										}
										new_node = new_node->next;
									}
								}
								break;
							}
							current_tcp = current_tcp->next;
						}
						if (found == 1)
							continue;
						// Copiez id-ul in structura clientului tcp.
						memcpy(tcp->id, buffer + 4, ID_SIZE);
						// Afisez un mesaj corespunzator.
						printf("New client %s connected from %s:%d.\n",
								tcp->id, inet_ntoa(tcp->address.sin_addr),
								ntohs(tcp->address.sin_port));
					} else {
						// Daca mesajul nu este de tip ID, inseamna ca este de tip
						// subscribe/unsubscribe.
						manage_subscription(buffer, tcp, topics);
					}
				}
			}
		}
		// Incerc sa trimit mesajele ramase (daca exista).
		send_udp_remaining(messages, tcps);
	}
	return 0;
}