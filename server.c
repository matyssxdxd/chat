#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>

#define PORT 1337

typedef struct server {
    int socket;
    struct sockaddr_in addr;
} server;

static void die(const char* msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

server* new_server(char* address, int port) {
    server* s = malloc(sizeof(server));
    s->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (s->socket < 0) { die("Error creating socket."); }

    int val = 1;
    setsockopt(s->socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    memset(&s->addr, 0, sizeof(s->addr));
    s->addr.sin_family = AF_INET;
    s->addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &(s->addr.sin_addr)) == 0) { die("Invalid IPv4 address."); }
    
    if (bind(s->socket, (struct sockaddr*)&s->addr, sizeof(s->addr))) {
        die("Error binding socket.");
    }

    if (listen(s->socket, SOMAXCONN)) { die ("Error listening."); }

    return s;
}

void launch_server(server* s) {
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(s->socket, (struct sockaddr*)&client_addr, &addrlen);
        if (connfd < 0) { continue; }

        char rbuf[1024];
        ssize_t n = read(s->socket, rbuf, 1024);
        if (n < 0) { die("Error reading."); }

        printf("client says: %s\n", rbuf);

        char* wbuf = "Hello, world!";
        write(connfd, wbuf, strlen(wbuf));

        close(connfd);
    }
}

void close_server(server* s) {
    close(s->socket);
    free(s);
}

int main (void) {
    server* s = new_server("127.0.0.1", PORT);
    launch_server(s);
    close_server(s);

    return 0;
}
