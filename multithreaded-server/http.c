#include "http.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    } else if (strcmp(".mp3", file_extension) == 0) {
        return "audio/mpeg";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) {
    // stdio FILE * gives us 'fgets()' to easily read line by line
    int sock_fd_copy = dup(fd);
    if (sock_fd_copy == -1) {
        perror("dup");
        return -1;
    }
    FILE *socket_stream = fdopen(sock_fd_copy, "r");
    if (socket_stream == NULL) {
        perror("fdopen");
        close(sock_fd_copy);
        return -1;
    }
    // Disable the usual stdio buffering
    if (setvbuf(socket_stream, NULL, _IONBF, 0) != 0) {
        perror("setvbuf");
        fclose(socket_stream);
        return -1;
    }

    // Keep consuming lines until we find an empty line
    // This marks the end of the response headers and beginning of actual content
    char buf[BUFSIZE];
    // reads the first line of the reuqest to get file name
    if (fgets(buf, BUFSIZE, socket_stream) == NULL) {
        perror("fgets");
        fclose(socket_stream);
        return -1;
    }
    // getting file name from first line and storing in resource name
    char *file = strtok(buf, " ");
    file = strtok(NULL, " ");
    strcpy(resource_name, file);
    // reading in the rest of the request
    while (fgets(buf, BUFSIZE, socket_stream) != NULL) {
        if (strcmp(buf, "\r\n") == 0) {
            break;
        }
    }
    // error checking and clean up
    if (ferror(socket_stream)) {
        perror("fgets");
        fclose(socket_stream);
        return -1;
    }

    if (fclose(socket_stream) != 0) {
        perror("fclose");
        return -1;
    }

    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    // writes the reuqest from the designated resource_path to fd
    char buf[BUFSIZE];
    int len;
    struct stat content;
    // STAT struct
    if (stat(resource_path, &content) == -1) {
        // file does not exist
        len = snprintf(buf, BUFSIZE,
                       "HTTP/1.0 404 Not Found\r\n"
                       "Content-Length: 0\r\n"
                       "\r\n");
        if (len == -1) {
            perror("snprintf");
            return -1;
        }
        // dont want to do anything else if file doesn't exists
        return -1;
    }
    int cont_len = content.st_size;
    //  access variable called content.st_size for length
    char *end = strstr(resource_path, ".");
    //  get content type
    const char *cont_type = get_mime_type(end);
    // bhbdk.txt extract '.txt'

    // if the file exists is it does put 200 OK
    FILE *f = fopen(resource_path, "r");
    len = snprintf(buf, BUFSIZE,
                   "HTTP/1.0 200 OK\r\n"
                   "Content Type: %s\r\n"
                   "Content-Legnth: %d\r\n"
                   "\r\n",
                   cont_type, cont_len);

    if (len < 0) {
        perror("snprintf");
        return -1;
    }

    if (len >= BUFSIZE) {
        perror("snprintf");
        return -1;
    }

    int written = write(fd, buf, len);
    // writing from buffer to client
    if (written != len) {
        perror("write()");
        return -1;
    }

    int num_bytes;
    memset(buf, 0, BUFSIZE);

    // writing  rest of response to buf
    while ((num_bytes = fread(buf, 1, BUFSIZE, f)) > 0) {
        int writ = write(fd, buf, num_bytes);
        if (writ != num_bytes) {
            // error check
            perror("Error occurred.");
            fclose(f);
            close(fd);
            return -1;
        }
    }

    fclose(f);
    return 0;
}
