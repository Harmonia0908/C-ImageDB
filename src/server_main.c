#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "net_server.h"

int main(int argc, char **argv) {
    int port;
    long parsed;
    char *end;

    if (argc != 2) {
        fprintf(stderr, "Usage: ./imagedb-server <port>\n");
        return 1;
    }

    errno = 0;
    parsed = strtol(argv[1], &end, 10);
    if (errno == ERANGE || end == argv[1] || *end != '\0' ||
        parsed <= 0 || parsed > 65535 || parsed > (long)INT_MAX) {
        fprintf(stderr, "[ERROR] Invalid port: %s\n", argv[1]);
        return 1;
    }
    port = (int)parsed;

    return net_server_run(port);
}
