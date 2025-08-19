#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdint.h>

static void die(const char* msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

void clear_current_line() {
    printf("\033[A");      // Move cursor up one line
    printf("\033[2K");     // Clear entire line
    printf("\r");          // Move cursor to beginning of line
}

int main(void) {
    char username[50];

    do {
        printf("Please enter your username: ");
    } while (!fgets(username, sizeof(username), stdin));

    size_t username_len = strlen(username);
    username[username_len - 1] = '\0';

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { die("socket()"); }

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
            char input[512];
            char message[256];
            if (fgets(message, sizeof(message), stdin)) {
                clear_current_line();
                size_t offset = 0;

                uint8_t ulen = (uint8_t)(username_len - 1);
                memcpy(input + offset, &ulen, sizeof(ulen));
                offset += sizeof(ulen);

                memcpy(input + offset, username, username_len - 1);
                offset += username_len - 1;

                uint16_t mlen = (uint16_t)(strlen(message) - 1);
                memcpy(input + offset, &mlen, sizeof(mlen));
                offset += sizeof(mlen);

                memcpy(input + offset, message, mlen);
                offset += mlen;

                send(fd, input, offset, 0);

                printf("%s: %.*s\n", username, (int)mlen, message);
            }
        }

        if (pfds[1].revents & POLLIN) {
            char rbuf[512];
            int nbytes = recv(fd, rbuf, sizeof(rbuf) - 1, 0);
            if (nbytes <= 0) { 
                printf("Server disconnected\n");
                break;
            }
            rbuf[nbytes] = '\0';
            printf("%s", rbuf);
        }
    }

    close(fd);
    return 0;
}
