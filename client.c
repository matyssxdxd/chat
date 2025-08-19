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

static void die(const char* msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1337);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect()");
    }

    struct pollfd pfds[2];

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;

    pfds[1].fd = fd;
    pfds[1].events = POLLIN;

    while (1) {
        int poll_count = poll(pfds, 2, -1);
        if (poll_count == -1) { die("Error poll()"); }

        if (pfds[0].revents & POLLIN) {
            char input[256];
            if (fgets(input, sizeof(input), stdin)) {
                send(fd, input, strlen(input), 0);
            }
        }

        if (pfds[1].revents & POLLIN) {
            char rbuf[256];
            int nbytes = recv(fd, rbuf, sizeof(rbuf) - 1, 0);
            if (nbytes <= 0) { die("Error recv()"); }

            rbuf[nbytes] = '\0';
            printf("Received: %s", rbuf);
        }
    }
    return 0;
}
