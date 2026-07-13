CC = gcc
BASE_CFLAGS = -std=c11 -g -O2
WARNING_CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 \
	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS = $(BASE_CFLAGS) $(WARNING_CFLAGS)
LDLIBS = -lm
CPPFLAGS = -Iinclude

ifeq ($(SANITIZE),1)
CFLAGS = -std=c11 -g -O1 $(WARNING_CFLAGS) -fno-omit-frame-pointer \
	-fsanitize=address,undefined
export SANITIZE
endif

COMMON_SRC = src/image.c src/ppm.c src/bmp.c src/database.c src/process.c src/feature.c src/search.c src/similarity.c src/report.c src/verify.c src/visualize.c
CLI_SRC    = src/main.c src/cli.c
SERVER_SRC = src/server_main.c src/net_server.c src/net_io.c

COMMON_OBJ = $(COMMON_SRC:.c=.o)
CLI_OBJ    = $(CLI_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

ALL_OBJ = $(COMMON_OBJ) $(CLI_OBJ) $(SERVER_OBJ)
CORE_TEST_BIN = tests/test_core
CORE_TEST_SRC = tests/test_core.c src/image.c src/ppm.c src/process.c src/feature.c
NET_IO_TEST_BIN = tests/test_net_io
NET_IO_TEST_SRC = tests/test_net_io.c src/net_io.c

.PHONY: all server clean strict test test-unit test-integration \
	benchmark-test sanitizer-test

all: imagedb cimagedb

server: imagedb-server

imagedb: $(COMMON_OBJ) $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

cimagedb: $(COMMON_OBJ) $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

imagedb-server: $(COMMON_OBJ) $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(CORE_TEST_BIN): $(CORE_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE_TEST_SRC) $(LDLIBS)

$(NET_IO_TEST_BIN): $(NET_IO_TEST_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(NET_IO_TEST_SRC)

test-unit: $(CORE_TEST_BIN) $(NET_IO_TEST_BIN)
	./$(CORE_TEST_BIN)
	./$(NET_IO_TEST_BIN)

test-integration: all server
	bash tests/run_basic_tests.sh
	bash tests/run_db_tests.sh
	bash tests/run_image_ops_tests.sh
	bash tests/run_visual_tests.sh
	bash tests/run_storage_tests.sh
	bash tests/search_similar_test.sh
	bash tests/report_test.sh
	bash tests/verify_repair_test.sh
	bash tests/run_net_tests.sh

test:
	$(MAKE) test-unit
	$(MAKE) test-integration

benchmark-test: all
	bash tests/benchmark_test.sh

strict:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(WARNING_CFLAGS) -Wconversion -Werror" \
		all server $(CORE_TEST_BIN) $(NET_IO_TEST_BIN)

sanitizer-test:
	$(MAKE) clean
	ASAN_OPTIONS=halt_on_error=1:detect_leaks=0 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		$(MAKE) SANITIZE=1 test

clean:
	rm -f $(ALL_OBJ) imagedb cimagedb imagedb-server \
		$(CORE_TEST_BIN) $(NET_IO_TEST_BIN)
