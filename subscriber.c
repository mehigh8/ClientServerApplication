// Rosu Mihai Cosmin 323CA
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#define BUFFLEN 2000
#define TOPIC_SIZE 50

/*
 * Functie care trimite un mesaj pe un socket.
 * @sockfd - socket-ul pe care va fi trimis mesajul
 * @buffer - mesajul
 */
void send_message(int sockfd, char *buffer)
{
	short buflen = strlen(buffer);
	// Trimit mai intai lungimea mesajului.
	if (send(sockfd, &buflen, sizeof(short), 0) < 0) {
		fprintf(stderr, "send len\n");
		exit(0);
	}
	// Apoi trimit mesajul.
	if (send(sockfd, buffer, buflen, 0) < 0) {
		fprintf(stderr, "send\n");
		exit(0);
	}
}

/*
 * Functie care primeste un mesaj de pe un socket.
 * @sockfd - socket-ul pe care este primit mesajul
 * @buffer - buffer-ul in care va fi retinut mesajul
 */
void recv_message(int sockfd, char *buffer)
{
	short buflen = 0;
	// Mai intai preiau lungimea mesajului.
	if (recv(sockfd, &buflen, sizeof(short), 0) < 0) {
		fprintf(stderr, "recv len\n");
		exit(0);
	}
	// Apoi, preiau mesajul in buffer.
	if (recv(sockfd, buffer, buflen, 0) < 0) {
		fprintf(stderr, "recv\n");
		exit(0);
	}
}

/*
 * Functie care descifreaza si afiseaza un mesaj.
 * @buffer - buffer-ul in care este retinut mesajul
 */
void print_message(char *buffer)
{
	// Creez un nou buffer in care voi construi mesajul ce trebuie afisat.
	char new_buffer[BUFFLEN];
	// Setez octetii noului buffer pe 0.
	memset(new_buffer, 0, sizeof(new_buffer));

	// Incep sa sparg buffer-ul primit ca parametru, pentru a prelua diferite
	// parti, incapand cu ip-ul si portul.
	char *part = strtok(buffer, " ");
	// Pun ip-ul si portul in noul buffer.
	strcpy(new_buffer, part);
	strcat(new_buffer, " - ");

	int len = strlen(part) + 3;
	// Preiau topicul mesajului.
	part = buffer + strlen(part) + 1;
	// Il pun in noul buffer.
	memcpy(new_buffer + len, part, TOPIC_SIZE);
	strcat(new_buffer, " - ");
	part = part + TOPIC_SIZE;

	// Preiau tipul mesajului.
	char type = part[0];
	part = part + 1;

	// Buffer folosit pentru a transforma din numar in char.
	char number_char[20];
	memset(number_char, 0, sizeof(number_char));
	// In functie de type, aleg ce trebuie facut pentru descifrarea mesajului.
	switch (type) {
		case 0:
			// Cazul INT:
			strcat(new_buffer, "INT - ");

			// Preiau octetul de semn.
			char sign = part[0];
			part = part + 1;

			// Preiau numarul si il schimb din network byte order
			// in host byte order.
			uint32_t number;
			memcpy(&number, part, sizeof(uint32_t));
			number = ntohl(number);

			// Daca este cazul aplic semnul.
			int fin_num;
			if (sign == 1)
				fin_num = -1 * number;
			else
				fin_num = number;

			// Transform numarul in char-uri pentru a-l concatena in buffer.
			snprintf(number_char, sizeof(number_char), "%d", fin_num);
			strcat(new_buffer, number_char);
			break;
		case 1:
			// Cazul SHORT_REAL:
			strcat(new_buffer, "SHORT_REAL - ");

			// Preiau numarul si schimb byte order-ul.
			uint16_t abs_num;
			memcpy(&abs_num, part, sizeof(uint16_t));
			abs_num = ntohs(abs_num);

			// Impart numarul la 100 si il transform in char-uri pentru a-l
			// concatena in buffer.
			float fl_num = (float)abs_num / 100;
			snprintf(number_char, sizeof(number_char), "%.2f", fl_num);
			strcat(new_buffer, number_char);
			break;
		case 2:
			// Cazul FLOAT:
			strcat(new_buffer, "FLOAT - ");

			// Preiau octetul de semn.
			sign = part[0];
			part = part + 1;

			// Preiau valoarea numarului si schimb byte order-ul.
			uint32_t abs_val;
			memcpy(&abs_val, part, sizeof(uint32_t));
			abs_val = ntohl(abs_val);
			part = part + sizeof(uint32_t);

			// Preiau puterea lui 10 la care trebuie impartit numarul.
			uint8_t ten_pow;
			memcpy(&ten_pow, part, sizeof(uint8_t));

			// Impart numarul si ii aplic semnul, daca este cazul.
			double real_num = (double)abs_val;
			for (int j = 0; j < ten_pow; j++)
				real_num /= 10;

			if (sign == 1)
				real_num *= -1;

			// Transform numarul in char-uri si il concatenez la buffer.
			snprintf(number_char, sizeof(number_char), "%f", real_num);
			strcat(new_buffer, number_char);
			break;
		case 3:
			// Cazul STRING:
			strcat(new_buffer, "STRING - ");

			// Concatenez string-ul la buffer.
			strcat(new_buffer, part);
			break;
	}
	// Afisez noul buffer.
	printf("%s\n", new_buffer);
}

int main(int argc, char *argv[])
{
	// Dezactivez buffering-ul la afisare.
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc < 4) {
		fprintf(stderr, "Usage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n");
		return 0;
	}
	// Creez doua multimi de file descriptori, pe care le resetez.
	fd_set read_fds, tmp_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	// Creez un socket de tip SOCK_STREAM.
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "socket\n");
		return 0;
	}
	// Dezactivez algoritmul lui Nagle.
	int flag = 1;
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0) {
		fprintf(stderr, "tcp no delay\n");
		return 0;
	}
	// Setez socket-ul tocmai creat si socket-ul 0 (stdin) in multime.
	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);
	int fdmax = sockfd;
	// Pregatesc adresa serverului.
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(atoi(argv[3]));
	server_address.sin_addr.s_addr = inet_addr(argv[2]);
	// Conectez socket-ul la server.
	if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
		fprintf(stderr, "connect\n");
		return 0;
	}

	char buffer[BUFFLEN];
	// Construiesc un mesaj cu id-ul clientului pentru a-l trimite catre server.
	memset(buffer, 0, sizeof(buffer));
	strcat(buffer, "ID: ");
	strcat(buffer, argv[1]);
	// Trimit id-ul catre server.
	send_message(sockfd, buffer);

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
					// Daca mesajul este exit, inchid socket-ul si opresc programul.
					if (strncmp(buffer, "exit", 4) == 0) {
						close(sockfd);
						return 0;
					// Daca mesajul este subscribe/unsubscribe il trimit mai departe
					// catre server si afisez un mesaj corespunzator.
					} else if (strncmp(buffer, "subscribe", 9) == 0) {
						send_message(sockfd, buffer);

						printf("Subscribed to topic.\n");
					} else if (strncmp(buffer, "unsubscribe", 11) == 0) {
						send_message(sockfd, buffer);

						printf("Unsubscribed from topic.\n");
					}
				// Verific daca mesajul a venit de la server.
				} else if (i == sockfd) {
					memset(buffer, 0, sizeof(buffer));

					recv_message(i, buffer);
					// Daca mesajul primit este exit, inchidd socket-ul
					// si opresc programul.
					if (strncmp(buffer, "exit", 4) == 0) {
						close(sockfd);
						return 0;
					}
					// Altfel afisez mesajul descifrat.
					print_message(buffer);
				}
			}
		}
	}

	return 0;
}