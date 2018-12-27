CC = gcc
CFLAGS = -Wall -g -lpthread

all: server_n_client

server_n_client: server.o client.o bank.o
	$(CC) $(CFLAGS)  -o server server.o bank.o
	$(CC) $(CFLAGS)  -o client client.o bank.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

bank.o: bank.c bank.h
	$(CC) $(CFLAGS) -c bank.c

clean:
	rm -rf *.o client server
