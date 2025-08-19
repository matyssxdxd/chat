all: server client

server: server.c
	$(CC) server.c -o server -Wall -Wextra -pedantic -std=c99

client: client.c
	$(CC) client.c -o client -Wall -Wextra -pedantic -std=c99
