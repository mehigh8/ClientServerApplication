# Rosu Mihai Cosmin 323CA

CC=gcc
CFLAGS=-Wall -Wextra

build: server subscriber

server: server.o list.o
	$(CC) $^ -o $@

subscriber: subscriber.o
	$(CC) $^ -o $@
	
list.o: list.c list.h
	$(CC) $(CFLAGS) $^ -c

server.o: server.c
	$(CC) $(CFLAGS) $^ -c

subscriber.o: subscriber.c
	$(CC) $(CFLAGS) $^ -c

clean:
	rm -f *.o server subscriber *.h.gch

.PHONY: clean
