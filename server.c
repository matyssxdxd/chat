#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>
#include <poll.h>

#define PORT 1337

typedef struct server {
    int socket;
    struct sockaddr_in addr;
    int fd_size;
    int fd_count;
    struct pollfd *pfds;
} server;

static void die(const char* msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

void add_to_pfds(struct pollfd **pfds, int newfd, int *fd_count,
                 int *fd_size)
{
    if (*fd_count == *fd_size) {
        *fd_size *= 2;
        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN;
    (*pfds)[*fd_count].revents = 0;

    (*fd_count)++;
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

    s->fd_size = 5;
    s->pfds = malloc(sizeof(struct pollfd) * s->fd_size);
    
    s->pfds[0].fd = s->socket;
    s->pfds[0].events = POLLIN;
    s->fd_count = 1;

    return s;
}

void del_from_pfds(struct pollfd pfds[], int i, int* fd_count)
{
    pfds[i] = pfds[*fd_count-1];
    (*fd_count)--;
}

void handle_new_connection(int listener, int* fd_count, int* fd_size, struct pollfd** pfds) {
    struct sockaddr_in remoteaddr;
    socklen_t addrlen;
    int newfd;
    char remoteIP[INET_ADDRSTRLEN];

    addrlen = sizeof(remoteaddr);
    newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen);

    if (newfd == -1) { 
        die("Error accept()"); 
    } else {
        add_to_pfds(pfds, newfd, fd_count, fd_size);

        inet_ntop(AF_INET, &remoteaddr.sin_addr, remoteIP, INET6_ADDRSTRLEN);

        printf("new connection from %s on socket %d\n", remoteIP, newfd);
    }
}
void handle_client_data(int listener, int* fd_count, struct pollfd* pfds, int* pfd_i) {
    char rbuf[512];
    int nbytes = recv(pfds[*pfd_i].fd, rbuf, sizeof(rbuf), 0);
    int sender_fd = pfds[*pfd_i].fd;
    
    if (nbytes <= 0) {
        if (nbytes == 0) {
            printf("socket %d hung up\n", sender_fd);
        } else {
            die("Error recv()");
        }
        close(sender_fd);
        del_from_pfds(pfds, *pfd_i, fd_count);
        (*pfd_i)--;
        return;
    }
    
    size_t offset = 0;
    uint8_t ulen = rbuf[offset];
    offset += sizeof(ulen);
    
    char username[50];
    memcpy(username, rbuf + offset, ulen);
    username[ulen] = '\0';
    offset += ulen;
    
    uint16_t mlen;
    memcpy(&mlen, rbuf + offset, sizeof(mlen));
    offset += sizeof(mlen);
    
    char message[256];
    memcpy(message, rbuf + offset, mlen);
    message[mlen] = '\0';
    
    char output[512];
    snprintf(output, sizeof(output), "%s: %s\n", username, message);
    
    printf("recv from %s: %s\n", username, message);
    
    for (int j = 0; j < *fd_count; j++) {
        int dest_fd = pfds[j].fd;
        if (dest_fd != listener && dest_fd != sender_fd) {
            if (send(dest_fd, output, strlen(output), 0) < 0) {
                die("Error send()");
            }
        }
    }
}

void process_connections(int listener, int* fd_count, int* fd_size, struct pollfd** pfds) {
    for (int i = 0; i < *fd_count; i++) {
        // Checks if any of the connections are ready
        if ((*pfds)[i].revents & (POLLIN | POLLHUP)) {
            // If listeners is ready, then there's a new connection
            if ((*pfds)[i].fd == listener) {
                handle_new_connection(listener, fd_count, fd_size, pfds);
            } else {
                handle_client_data(listener, fd_count, *pfds, &i);
            }
        }
    }
}

void launch_server(server* s) {
    while (1) {
        int poll_count = poll(s->pfds, s->fd_count, -1);
        if (poll_count == -1) { die("Error poll()"); }

        process_connections(s->socket, &s->fd_count, &s->fd_size, &s->pfds);
    }
}

void close_server(server* s) {
    close(s->socket);
    free(s->pfds);
    free(s);
}

int main (void) {
    server* s = new_server("127.0.0.1", PORT);
    launch_server(s);
    close_server(s);

    return 0;
}
