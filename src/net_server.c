#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "net_server.h"
#include "database.h"
#include "feature.h"
#include "search.h"

#define DATA_DIR        "data"
#define RECV_BUF_SIZE   4096
#define BACKLOG         5

static int client_fd;

static void respond(const char *msg) {
    send(client_fd, msg, strlen(msg), 0);
}

static void handle_list(void) {
    image_record_t *records;
    int count, i;
    char buf[256];

    if (db_load_records(DATA_DIR, &records, &count) != 0) {
        respond("ERROR: Failed to load records\n");
        return;
    }

    if (count == 0) {
        respond("No images.\n");
        return;
    }

    respond("ID  Name                 Size      \n");
    respond("--- -------------------- ----------\n");

    for (i = 0; i < count; i++) {
        if (records[i].deleted)
            continue;
        snprintf(buf, sizeof(buf), "%-4d %-20s %4dx%-4d\n",
                 records[i].id,
                 records[i].name,
                 records[i].width,
                 records[i].height);
        respond(buf);
    }

    free(records);
}

static void handle_info(int id) {
    image_record_t record;
    char buf[1024];
    char time_buf[64];
    time_t tt;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        snprintf(buf, sizeof(buf), "ERROR: Record not found: ID %d\n", id);
        respond(buf);
        return;
    }

    tt = (time_t)record.import_time;
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&tt));

    snprintf(buf, sizeof(buf),
             "ID: %d\n"
             "Name: %s\n"
             "Width: %d x Height: %d\n"
             "Channels: %d\n"
             "File size: %ld bytes\n"
             "Import time: %s\n"
             "Path: %s\n",
             record.id, record.name,
             record.width, record.height,
             record.channels,
             record.file_size,
             time_buf,
             record.path);
    respond(buf);
}

static void handle_search(int query_id, int top_k) {
    search_result_t *results;
    int count, i;
    char buf[256];

    if (top_k <= 0) {
        respond("ERROR: top_k must be positive\n");
        return;
    }

    if (search_similar(DATA_DIR, query_id, top_k, METRIC_INTERSECTION,
                       &results, &count) != 0) {
        snprintf(buf, sizeof(buf),
                 "ERROR: Search failed for image %d\n", query_id);
        respond(buf);
        return;
    }

    snprintf(buf, sizeof(buf), "Query image: %d\nMetric: intersection\n", query_id);
    respond(buf);

    if (count == 0) {
        respond("No similar images found.\n");
    } else {
        snprintf(buf, sizeof(buf), "Top %d similar images:\n", count);
        respond(buf);
        for (i = 0; i < count; i++) {
            snprintf(buf, sizeof(buf), "%d. id=%-4d name=%-20s score=%.4f\n",
                     i + 1, results[i].image_id, results[i].name, results[i].value);
            respond(buf);
        }
    }

    free(results);
}

/* Process a single complete line (no \n or \r at end). Returns 0 if QUIT. */
static int process_line(const char *line) {
    char cmd[32];
    int id, k;
    char extra[32];

    if (line[0] == '\0')
        return 1;

    if (sscanf(line, "%31s", cmd) != 1)
        return 1;

    if (strcmp(cmd, "QUIT") == 0) {
        respond("BYE\n");
        return 0;
    }

    if (strcmp(cmd, "LIST") == 0) {
        handle_list();
        return 1;
    }

    if (strcmp(cmd, "INFO") == 0) {
        if (sscanf(line, "%*s %d %31s", &id, extra) != 1) {
            respond("ERROR: Usage: INFO <id>\n");
            return 1;
        }
        handle_info(id);
        return 1;
    }

    if (strcmp(cmd, "SEARCH") == 0) {
        if (sscanf(line, "%*s %d %d %31s", &id, &k, extra) != 2) {
            respond("ERROR: Usage: SEARCH <id> <k>\n");
            return 1;
        }
        if (k <= 0) {
            respond("ERROR: top_k must be positive\n");
            return 1;
        }
        handle_search(id, k);
        return 1;
    }

    respond("ERROR: Unknown command. Available: LIST, INFO, SEARCH, QUIT\n");
    return 1;
}

/* Process all complete lines in buf. Returns 0 if QUIT was received. */
static int process_buffer(char *buf, size_t len) {
    size_t start = 0;
    size_t i;

    for (i = 0; i < len; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            buf[i] = '\0';
            if (i > start) {
                if (!process_line(&buf[start]))
                    return 0;
            }
            /* Skip consecutive \r\n */
            if (buf[i] == '\r' && i + 1 < len && buf[i + 1] == '\n')
                i++;
            start = i + 1;
        }
    }
    return 1;
}

int net_server_run(int port) {
    int server_fd;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        fprintf(stderr, "[ERROR] Failed to create socket\n");
        return 1;
    }

    {
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
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
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        char line_buf[RECV_BUF_SIZE];
        size_t line_len = 0;
        ssize_t n;

        printf("Waiting for connection...\n");
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            fprintf(stderr, "[ERROR] Failed to accept connection\n");
            continue;
        }

        printf("Client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        respond("C-ImageDB Server\n");
        respond("Commands: LIST, INFO <id>, SEARCH <id> <k>, QUIT\n");

        for (;;) {
            n = recv(client_fd, line_buf + line_len,
                     RECV_BUF_SIZE - line_len - 1, 0);
            if (n <= 0)
                break;

            line_len += (size_t)n;
            line_buf[line_len] = '\0';

            /* Find last complete line */
            {
                char *last_nl = NULL;
                size_t processed;
                size_t i;

                for (i = 0; i < line_len; i++) {
                    if (line_buf[i] == '\n')
                        last_nl = &line_buf[i];
                }

                if (last_nl) {
                    processed = (size_t)(last_nl - line_buf) + 1;

                    if (!process_buffer(line_buf, processed))
                        goto client_done;

                    /* Move remaining partial line to front */
                    line_len -= processed;
                    if (line_len > 0)
                        memmove(line_buf, &line_buf[processed], line_len);
                }

                /* Buffer full with no newline - discard to avoid overflow */
                if (line_len >= RECV_BUF_SIZE - 1) {
                    respond("ERROR: Line too long\n");
                    line_len = 0;
                }
            }
        }

client_done:
        printf("Client disconnected.\n");
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
