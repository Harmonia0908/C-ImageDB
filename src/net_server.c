#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "database.h"
#include "net_server.h"
#include "search.h"

#define DATA_DIR                "data"
#define RECV_BUF_SIZE           4096
#define BACKLOG                 5
#define CLIENT_TIMEOUT_SECONDS  10
#define MAX_SEARCH_RESULTS      1000

static int send_all(int fd, const char *data, size_t length) {
    size_t sent = 0;

    while (sent < length) {
        ssize_t n;
#ifdef MSG_NOSIGNAL
        n = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
#else
        n = send(fd, data + sent, length - sent, 0);
#endif
        if (n > 0) {
            sent += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        return -1;
    }
    return 0;
}

static int respond(int fd, const char *message) {
    if (!message)
        return -1;
    return send_all(fd, message, strlen(message));
}

static void safe_text(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;

    if (!dst || dst_size == 0)
        return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < dst_size) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = (c >= 32 && c != 127) ? (char)c : '?';
        i++;
    }
    dst[i] = '\0';
}

static void handle_list(int fd) {
    image_record_t *records = NULL;
    int count = 0;
    int i;
    char buffer[256];

    if (db_load_records(DATA_DIR, &records, &count) != 0) {
        respond(fd, "ERROR: Failed to load records\n");
        return;
    }

    if (count == 0) {
        respond(fd, "No images.\n");
        free(records);
        return;
    }

    if (respond(fd, "ID  Name                 Size      \n") != 0 ||
        respond(fd, "--- -------------------- ----------\n") != 0) {
        free(records);
        return;
    }

    for (i = 0; i < count; i++) {
        char name[MAX_NAME_LEN];

        if (records[i].deleted)
            continue;
        safe_text(name, sizeof(name), records[i].name);
        snprintf(buffer, sizeof(buffer), "%-4d %-20s %4dx%-4d\n",
                 records[i].id, name, records[i].width, records[i].height);
        if (respond(fd, buffer) != 0)
            break;
    }

    free(records);
}

static void handle_info(int fd, int id) {
    image_record_t record;
    char buffer[1024];
    char time_buffer[64] = "unknown";
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    time_t timestamp;
    struct tm *time_info;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        snprintf(buffer, sizeof(buffer), "ERROR: Record not found: ID %d\n", id);
        respond(fd, buffer);
        return;
    }

    timestamp = (time_t)record.import_time;
    time_info = localtime(&timestamp);
    if (time_info)
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S",
                 time_info);
    safe_text(name, sizeof(name), record.name);
    safe_text(path, sizeof(path), record.path);

    snprintf(buffer, sizeof(buffer),
             "ID: %d\n"
             "Name: %s\n"
             "Width: %d x Height: %d\n"
             "Channels: %d\n"
             "File size: %ld bytes\n"
             "Import time: %s\n"
             "Path: %s\n",
             record.id, name, record.width, record.height, record.channels,
             record.file_size, time_buffer, path);
    respond(fd, buffer);
}

static void handle_search(int fd, int query_id, int top_k) {
    search_result_t *results = NULL;
    int count = 0;
    int i;
    char buffer[256];

    if (search_similar(DATA_DIR, query_id, top_k, METRIC_INTERSECTION,
                       &results, &count) != 0) {
        snprintf(buffer, sizeof(buffer),
                 "ERROR: Search failed for image %d\n", query_id);
        respond(fd, buffer);
        return;
    }

    snprintf(buffer, sizeof(buffer),
             "Query image: %d\nMetric: intersection\n", query_id);
    if (respond(fd, buffer) != 0)
        goto cleanup;

    if (count == 0) {
        respond(fd, "No similar images found.\n");
    } else {
        snprintf(buffer, sizeof(buffer), "Top %d similar images:\n", count);
        if (respond(fd, buffer) != 0)
            goto cleanup;
        for (i = 0; i < count; i++) {
            char name[MAX_NAME_LEN];

            safe_text(name, sizeof(name), results[i].name);
            snprintf(buffer, sizeof(buffer),
                     "%d. id=%-4d name=%-20s score=%.4f\n",
                     i + 1, results[i].image_id, name, results[i].value);
            if (respond(fd, buffer) != 0)
                break;
        }
    }

cleanup:
    free(results);
}

static int next_token(const char **cursor, char *token, size_t token_size) {
    const char *p;
    size_t length = 0;
    int too_long = 0;

    if (!cursor || !*cursor || !token || token_size == 0)
        return -1;
    p = *cursor;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (!*p) {
        *cursor = p;
        return 0;
    }
    while (*p && !isspace((unsigned char)*p)) {
        if (length + 1 < token_size)
            token[length++] = *p;
        else
            too_long = 1;
        p++;
    }
    token[length] = '\0';
    *cursor = p;
    return too_long ? -1 : 1;
}

static int parse_int_token(const char *token, int *value) {
    char *end;
    long parsed;

    if (!token || !*token || !value)
        return 0;
    errno = 0;
    parsed = strtol(token, &end, 10);
    if (errno == ERANGE || end == token || *end != '\0' ||
        parsed < (long)INT_MIN || parsed > (long)INT_MAX)
        return 0;
    *value = (int)parsed;
    return 1;
}

/* Process a complete line without its CRLF terminator. Returns 0 for QUIT. */
static int process_line(int fd, const char *line) {
    char tokens[4][64];
    const char *cursor = line;
    int token_count = 0;
    int token_status;
    int id;
    int top_k;

    do {
        if (token_count == 4) {
            respond(fd, "ERROR: Too many arguments\n");
            return 1;
        }
        token_status = next_token(&cursor, tokens[token_count],
                                  sizeof(tokens[token_count]));
        if (token_status < 0) {
            respond(fd, "ERROR: Token too long\n");
            return 1;
        }
        if (token_status > 0)
            token_count++;
    } while (token_status > 0);

    if (token_count == 0)
        return 1;

    if (strcmp(tokens[0], "QUIT") == 0) {
        if (token_count != 1) {
            respond(fd, "ERROR: Usage: QUIT\n");
            return 1;
        }
        respond(fd, "BYE\n");
        return 0;
    }

    if (strcmp(tokens[0], "LIST") == 0) {
        if (token_count != 1) {
            respond(fd, "ERROR: Usage: LIST\n");
            return 1;
        }
        handle_list(fd);
        return 1;
    }

    if (strcmp(tokens[0], "INFO") == 0) {
        if (token_count != 2 || !parse_int_token(tokens[1], &id) || id <= 0) {
            respond(fd, "ERROR: Usage: INFO <id>\n");
            return 1;
        }
        handle_info(fd, id);
        return 1;
    }

    if (strcmp(tokens[0], "SEARCH") == 0) {
        if (token_count != 3 || !parse_int_token(tokens[1], &id) || id <= 0 ||
            !parse_int_token(tokens[2], &top_k)) {
            respond(fd, "ERROR: Usage: SEARCH <id> <k>\n");
            return 1;
        }
        if (top_k <= 0) {
            respond(fd, "ERROR: top_k must be positive\n");
            return 1;
        }
        if (top_k > MAX_SEARCH_RESULTS) {
            respond(fd, "ERROR: top_k too large (max 1000)\n");
            return 1;
        }
        handle_search(fd, id, top_k);
        return 1;
    }

    respond(fd, "ERROR: Unknown command. Available: LIST, INFO, SEARCH, QUIT\n");
    return 1;
}

static int process_bytes(int fd, const char *data, size_t data_size,
                         char *line, size_t *line_length, int *discarding) {
    size_t i;

    for (i = 0; i < data_size; i++) {
        unsigned char byte = (unsigned char)data[i];

        if (*discarding) {
            if (byte == '\n')
                *discarding = 0;
            continue;
        }
        if (byte == '\0') {
            respond(fd, "ERROR: NUL byte is not allowed\n");
            *line_length = 0;
            *discarding = 1;
            continue;
        }
        if (byte == '\n') {
            if (*line_length > 0 && line[*line_length - 1] == '\r')
                (*line_length)--;
            line[*line_length] = '\0';
            if (!process_line(fd, line))
                return 0;
            *line_length = 0;
            continue;
        }
        if (*line_length >= MAX_CMDLINE - 1) {
            respond(fd, "ERROR: Line too long\n");
            *line_length = 0;
            *discarding = 1;
            continue;
        }
        line[(*line_length)++] = (char)byte;
    }
    return 1;
}

int net_server_run(int port) {
    int server_fd;
    struct sockaddr_in address;

    if (port <= 0 || port > 65535)
        return 1;
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "[ERROR] Failed to configure SIGPIPE handling\n");
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        fprintf(stderr, "[ERROR] Failed to create socket\n");
        return 1;
    }

    {
        int option = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option,
                       sizeof(option)) != 0) {
            fprintf(stderr, "[ERROR] Failed to configure listening socket\n");
            close(server_fd);
            return 1;
        }
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        fprintf(stderr, "[ERROR] Failed to bind to port %d\n", port);
        close(server_fd);
        return 1;
    }
    if (listen(server_fd, BACKLOG) < 0) {
        fprintf(stderr, "[ERROR] Failed to listen\n");
        close(server_fd);
        return 1;
    }

    printf("C-ImageDB server listening on port %d\n", port);
    printf("Commands: LIST, INFO <id>, SEARCH <id> <k>, QUIT\n");

    for (;;) {
        struct sockaddr_in client_address;
        socklen_t client_length = sizeof(client_address);
        int client_fd;
        char recv_buffer[RECV_BUF_SIZE];
        char line[MAX_CMDLINE];
        size_t line_length = 0;
        int discarding = 0;
        struct timeval timeout;

        client_fd = accept(server_fd, (struct sockaddr *)&client_address,
                           &client_length);
        if (client_fd < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "[ERROR] Failed to accept connection\n");
            continue;
        }

        timeout.tv_sec = CLIENT_TIMEOUT_SECONDS;
        timeout.tv_usec = 0;
        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                       sizeof(timeout)) != 0 ||
            setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                       sizeof(timeout)) != 0) {
            close(client_fd);
            continue;
        }

        respond(client_fd, "C-ImageDB Server\n");
        respond(client_fd, "Commands: LIST, INFO <id>, SEARCH <id> <k>, QUIT\n");

        for (;;) {
            ssize_t received = recv(client_fd, recv_buffer,
                                    sizeof(recv_buffer), 0);
            if (received > 0) {
                if (!process_bytes(client_fd, recv_buffer, (size_t)received,
                                   line, &line_length, &discarding))
                    break;
                continue;
            }
            if (received < 0 && errno == EINTR)
                continue;
            break;
        }

        close(client_fd);
    }
}
