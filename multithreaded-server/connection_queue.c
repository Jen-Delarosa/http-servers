#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

// typedef struct {
//     node_t *head;
//     unsigned size;
//     pthread_mutex_t lock;
//     pthread_cond_t queue_full;
//     pthread_cond_t queue_empty;
// } queue_t;

/*
 * Initialize a new connection queue.
 * The queue can store at most 'CAPACITY' elements.
 * queue: Pointer to connection_queue_t to be initialized
 * Returns 0 on success or -1 on error
 */
int connection_queue_init(connection_queue_t *queue) {
    // thread safe bounded queue

    queue->length = 0;
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->shutdown = 0;

    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        perror("pthread_mutex_init\n");
        return -1;
    }

    if (pthread_cond_init(&queue->queue_full, NULL) != 0) {
        perror("pthread_cond_init\n");
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }

    if (pthread_cond_init(&queue->queue_empty, NULL) != 0) {
        perror("pthread_cond_init\n");
        if (pthread_cond_destroy(&queue->queue_full) != 0) {
            perror("pthread_cond_destroy\n");
            return -1;
        }
        if (pthread_mutex_destroy(&queue->lock) != 0) {
            perror("pthread_mutex_destroy\n");
            return -1;
        }
        return -1;
    }

    return 0;
}

/*
 * Add a new file descriptor to a connection queue. If the queue is full, then
 * this function blocks until space becomes available. If the queue is shut
 * down, then no addition to the queue takes place and an error is returned.
 * queue: A pointer to the connection_queue_t to add to
 * connection_fd: The socket file descriptor to add to the queue
 * Returns 0 on success or -1 on error
 */
int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    if (pthread_mutex_lock(&queue->lock) != 0) {
        perror("pthread_mutex_lock\n");
        return -1;
    }

    while (queue->length == CAPACITY && queue->shutdown != 0) {
        // Wait for space to become available
        if (pthread_cond_wait(&queue->queue_full, &queue->lock) == -1) {
            perror("pthread_cond_wait\n");
            if (pthread_mutex_unlock(&queue->lock) == -1) {
                perror("pthread unlock\n");
                return -1;
            }
            return -1;
        }
    }

    if (queue->shutdown) {
        if (pthread_mutex_unlock(&queue->lock) != 0) {
            perror("pthread_mutex_unlock\n");
            return -1;
        }
        return -1;
    }

    queue->client_fds[queue->write_idx] = connection_fd;

    queue->write_idx = (queue->write_idx + 1) % CAPACITY;

    queue->length++;

    if (pthread_cond_signal(&queue->queue_empty) != 0) {
        perror("pthread_cond_signal\n");
        return -1;
    }

    if (pthread_mutex_unlock(&queue->lock) != 0) {
        perror("pthread_mutex_unlock\n");
        return -1;
    }

    return 0;
}

/*
 * Remove a file descriptor from the connection queue. If the queue is empty,
 * then this function blocks until an item becomes available. If the queue is
 * shut down, then no removal from the queue takes place and an error is
 * returned.
 * queue: A pointer to the connection_queue_t to remove from
 * Returns the removed socket file descriptor on success or -1 on error
 */
int connection_queue_dequeue(connection_queue_t *queue) {
    if (pthread_mutex_lock(&queue->lock) != 0) {
        perror("pthread_mutex_lock\n");
        return -1;
    }

    while (queue->length == 0 && !queue->shutdown) {
        // cannot remove if it is empty
        if (pthread_cond_wait(&queue->queue_empty, &queue->lock) == -1) {
            printf("pthread_cond_wait\n");
            if (pthread_mutex_unlock(&queue->lock) == -1) {
                printf("pthread unlock\n");
            }
        }
    }

    if (queue->shutdown) {
        if (pthread_mutex_unlock(&queue->lock) != 0) {
            perror("othread_mutex_unlock\n");
            return -1;
        }
        return -1;
    }

    int connection_fd = queue->client_fds[queue->read_idx];

    queue->read_idx = (queue->read_idx + 1) % CAPACITY;

    queue->length--;

    if (pthread_cond_signal(&queue->queue_full) != 0) {
        perror("pthread_cond_signal\n");
        return -1;
    }

    if (pthread_mutex_unlock(&queue->lock) != 0) {
        perror("pthread_mutex_unlock\n");
        return -1;
    }

    return connection_fd;
}

/*
 * Cleanly shuts down the connection queue. All threads currently blocked on an
 * enqueue or dequeue operation are unblocked and an error is returned to them.
 * queue: A pointer to the connection_queue_t to shut down
 * Returns 0 on success or -1 on error
 */
int connection_queue_shutdown(connection_queue_t *queue) {
    int ret_val = 0;

    if (pthread_mutex_lock(&queue->lock) != 0) {
        perror("pthread_mutex_lock\n");
        ret_val = -1;
    }

    queue->shutdown = 1;

    if (pthread_cond_broadcast(&queue->queue_empty) != 0) {
        perror("pthread_cond_broadcast\n");
        ret_val = -1;
    }

    if (pthread_cond_broadcast(&queue->queue_full) != 0) {
        perror("pthread_cond_broadcast\n");
        ret_val = -1;
    }

    if (pthread_mutex_unlock(&queue->lock) != 0) {
        perror("pthread_mutex_unlock\n");
        ret_val = -1;
    }

    return ret_val;
}

/*
 * Deallocates and cleans up any resources associated with a connection queue.
 * Returns 0 on success or -1 on error
 */
int connection_queue_free(connection_queue_t *queue) {
    int ret_val = 0;

    if (pthread_mutex_destroy(&queue->lock)) {
        perror("pthread_mutex_destroy\n");
        ret_val = -1;
    }

    if (pthread_cond_destroy(&queue->queue_empty)) {
        perror("pthread_cond_destroy\n");
        ret_val = -1;
    }

    if (pthread_cond_destroy(&queue->queue_full)) {
        perror("pthread_cond_destroy\n");
        ret_val = -1;
    }

    return ret_val;
}
