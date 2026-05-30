#include <stdio.h>
#include <stdlib.h>
#include "net_server.h"

int main(int argc, char **argv) {
    int port;

    if (argc != 2) {
        fprintf(stderr, "Usage: ./imagedb-server <port>\n");
        return 1;
    }

    port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "[ERROR] Invalid port: %s\n", argv[1]);
        return 1;
    }

    return net_server_run(port);
}
