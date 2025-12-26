#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) {
    keep_going = 0;
}

// Information passed to consumer threads
typedef struct queue queue_t;

// Consumer thread start function
void *consumer_thread_func(void *arg) {
    connection_queue_t *queue = (connection_queue_t *) arg;
    char buf[BUFSIZE];
    int fd;
    while (1) {
        fd = connection_queue_dequeue(queue);
        if (fd == -1) {
            perror("connect_queue_dequeue\n");
        }

        if (read_http_request(fd, buf) == -1) {
            // saved in buf
            if (close(fd) == -1) {
                perror("close\n");
            }
            perror("read_http_request\n");
        }

        char path[BUFSIZE];
        snprintf(path, BUFSIZE, "%s%s", serve_dir, buf);

        if (write_http_response(fd, path) == -1) {
            if (close(fd) == -1) {
                perror("close\n");
            }
            perror("write_http_response\n");
        }
        if (close(fd) == -1) {
            perror("close");
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    pthread_t consumer_threads[N_THREADS];
    connection_queue_t queue;

    if (connection_queue_init(&queue) != 0) {
        fprintf(stderr, "Failed to initialize queue\n");
        return 1;
    }

    int result;

    // First argument is directory to serve, second is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }

    serve_dir = argv[1];
    const char *port = argv[2];

    // creating signal handler
    struct sigaction sighandle;
    sighandle.sa_handler = handle_sigint;
    sighandle.sa_flags = 0;    // Dont use restart
    sigemptyset(&sighandle.sa_mask);
    sigaction(SIGINT, &sighandle, NULL);

    if (sigprocmask(SIG_BLOCK, &sighandle.sa_mask, NULL) == -1) {
        perror("block sig\n");
        return -1;
    }

    // Create multiple producer threads
    for (int i = 0; i < N_THREADS; i++) {
        if ((result = pthread_create(&consumer_threads[i], NULL, consumer_thread_func, &queue)) !=
            0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(result));
            return 1;
        }
    }

    if (sigprocmask(SIG_UNBLOCK, &sighandle.sa_mask, NULL) == -1) {
        perror("unblock sig\n");
        return -1;
    }

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

        if (connection_queue_enqueue(&queue, new_client) == -1) {
            perror("dsfghjkl");
            return -1;
        }
    }

    if (close(sockfd) == -1) {
        // Error checking
        perror("close");
        return -1;
    }

    if (connection_queue_shutdown(&queue) != 0) {
        fprintf(stderr, "shutdown");
    }

    for (int i = 0; i < N_THREADS; i++) {
        if ((pthread_join(consumer_threads[i], NULL)) != 0) {
            fprintf(stderr, "pthread_join: %d\n", i);
        }
    }

    if (connection_queue_free(&queue) != 0) {
        fprintf(stderr, "free");
    }

    return 0;
}
