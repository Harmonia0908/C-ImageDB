CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDLIBS = -lm
INCLUDES = -Iinclude

COMMON_SRC = src/image.c src/ppm.c src/bmp.c src/database.c src/process.c src/feature.c src/search.c src/similarity.c src/report.c src/verify.c src/visualize.c
CLI_SRC    = src/main.c src/cli.c
SERVER_SRC = src/server_main.c src/net_server.c

COMMON_OBJ = $(COMMON_SRC:.c=.o)
CLI_OBJ    = $(CLI_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

ALL_OBJ = $(COMMON_OBJ) $(CLI_OBJ) $(SERVER_OBJ)

.PHONY: all server clean

all: imagedb cimagedb

server: imagedb-server

imagedb: $(COMMON_OBJ) $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

cimagedb: $(COMMON_OBJ) $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

imagedb-server: $(COMMON_OBJ) $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(ALL_OBJ) imagedb cimagedb imagedb-server
