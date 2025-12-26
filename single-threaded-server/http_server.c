#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5

int keep_going = 1;

void handle_sigint(int signo) {
    keep_going = 0;
}

int main(int argc, char **argv) {
    // First argument is directory to serve, second is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    // Uncomment the lines below to use these definitions:
    const char *serve_dir = argv[1];
    const char *port = argv[2];

    // creating signal handler
    struct sigaction sighandle;
    sighandle.sa_handler = handle_sigint;
    sighandle.sa_flags = 0;    // Dont use restart
    sigemptyset(&sighandle.sa_mask);
    sigaction(SIGINT, &sighandle, NULL);

    // TODO Complete the rest of this function
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));    // initializes hints as 0

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *server;

    int ret_val = getaddrinfo(NULL, port, &hints, &server);

    if (ret_val == -1) {
        // Error checking
        fprintf(stderr, "getaddr info failed:%s\n", gai_strerror(ret_val));
        return -1;
    }

    int sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    // create new socket file descriptor
    if (sockfd == -1) {
        // Error checking
        perror("socket");
        freeaddrinfo(server);
        return -1;
    }

    // reserve a specific port
    if (bind(sockfd, server->ai_addr, server->ai_addrlen) == -1) {
        // Error checking
        perror("bind()");
        freeaddrinfo(server);
        return -1;
    }

    // designate socket file descriptor as a server socket
    if (listen(sockfd, LISTEN_QUEUE_LEN) == -1) {
        // Error checking
        perror("listen()");
        freeaddrinfo(server);
        return -1;
    }

    // used to store read request
    char buf[BUFSIZE];

    while (keep_going != 0) {
        // generate a new connection socket for each client
        int new_client = accept(sockfd, NULL, NULL);

        if (new_client == -1) {
            if (errno != EINTR) {
                perror("accept");
                close(sockfd);
                return -1;
            }
            return -1;
        }

        // reading the clients http request
        if (read_http_request(new_client, buf) == -1) {
            // saves request form client to buf
            if (close(new_client) == -1) {
                // Error checking
                perror("close");
                return -1;
            }
            return -1;
        }
        // Appending serve dir to file name in buf
        char path[BUFSIZE];
        if (snprintf(path, BUFSIZE, "%s%s", serve_dir, buf) == -1) {
            perror("snprintf");
            return -1;
        }

        if (write_http_response(new_client, path) == -1) {
            // writes response to client using file from path
            if (close(new_client) == -1) {
                // Error checking
                perror("close");
                return -1;
            }
            return -1;
        }
    }

    if (close(sockfd) == -1) {
        // Error checking
        perror("close");
        return -1;
    }
    return 0;
}
