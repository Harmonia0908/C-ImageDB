CC = gcc
BASE_CFLAGS = -std=c11 -g -O2
DEBUG_CFLAGS = -std=c11 -g3 -O0
WARNING_CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 \
	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS = $(BASE_CFLAGS) $(WARNING_CFLAGS)
LDLIBS = -lm
CPPFLAGS = -Iinclude
DEPFLAGS = -MMD -MP

ifeq ($(SANITIZE),1)
CFLAGS = -std=c11 -g -O1 $(WARNING_CFLAGS) -fno-omit-frame-pointer \
	-fsanitize=address,undefined
export SANITIZE
endif

# Shared implementation linked into both CLI and server binaries.
ENGINE_SRC = \
	src/image.c src/ppm.c src/bmp.c \
	src/database.c src/storage/store_file.c \
	src/process.c src/feature.c src/search.c src/similarity.c \
	src/report.c src/verify.c src/visualize.c

# Front-end-specific entry points and adapters.
CLI_SRC = \
	src/main.c src/cli.c src/app.c src/cli_parse.c src/cli_output.c
SERVER_SRC = \
	src/server_main.c src/net_server.c src/net_io.c

ENGINE_OBJ = $(ENGINE_SRC:.c=.o)
CLI_OBJ    = $(CLI_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

ALL_OBJ = $(ENGINE_OBJ) $(CLI_OBJ) $(SERVER_OBJ)
ALL_DEP = $(ALL_OBJ:.o=.d)
PROJECT_HEADERS = $(wildcard include/*.h src/*.h src/storage/*.h)
CORE_TEST_BIN = tests/test_core
CORE_TEST_SRC = tests/test_core.c src/image.c src/ppm.c src/process.c src/feature.c
NET_IO_TEST_BIN = tests/test_net_io
NET_IO_TEST_SRC = tests/test_net_io.c src/net_io.c
STORE_TEST_BIN = tests/test_store
STORE_TEST_SRC = tests/test_store.c src/database.c src/storage/store_file.c
CLI_PARSE_TEST_BIN = tests/test_cli_parse
CLI_PARSE_TEST_SRC = tests/test_cli_parse.c src/cli_parse.c
APP_TEST_BIN = tests/test_app
APP_TEST_SRC = tests/test_app.c src/app.c $(ENGINE_SRC)

.PHONY: all server debug release help clean strict test test-unit \
	test-integration benchmark-test sanitizer-test

all: imagedb cimagedb

server: imagedb-server

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS) $(WARNING_CFLAGS)" all server

release:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(BASE_CFLAGS) $(WARNING_CFLAGS)" all server

help:
	@echo "Build targets:"
	@echo "  all               Build imagedb and cimagedb (default)"
	@echo "  server            Build imagedb-server"
	@echo "  debug             Clean Debug build of CLI and server"
	@echo "  release           Clean Release build of CLI and server"
	@echo "  strict            Clean build with -Wconversion -Werror"
	@echo "Test targets:"
	@echo "  test              Run unit and integration tests"
	@echo "  test-unit         Run C unit tests"
	@echo "  test-integration  Run CLI and TCP integration tests"
	@echo "  benchmark-test    Run benchmark smoke test"
	@echo "  sanitizer-test    Run all tests with ASan and UBSan"

imagedb: $(ENGINE_OBJ) $(CLI_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cimagedb: $(ENGINE_OBJ) $(CLI_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

imagedb-server: $(ENGINE_OBJ) $(SERVER_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(CORE_TEST_BIN): $(CORE_TEST_SRC) $(PROJECT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(CORE_TEST_SRC) $(LDLIBS)

$(NET_IO_TEST_BIN): $(NET_IO_TEST_SRC) $(PROJECT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(NET_IO_TEST_SRC)

$(STORE_TEST_BIN): $(STORE_TEST_SRC) $(PROJECT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(STORE_TEST_SRC)

$(CLI_PARSE_TEST_BIN): $(CLI_PARSE_TEST_SRC) $(PROJECT_HEADERS)
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $(LDFLAGS) -o $@ $(CLI_PARSE_TEST_SRC) $(LDLIBS)

$(APP_TEST_BIN): $(APP_TEST_SRC) $(PROJECT_HEADERS)
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $(LDFLAGS) -o $@ $(APP_TEST_SRC) $(LDLIBS)

test-unit: $(CORE_TEST_BIN) $(NET_IO_TEST_BIN) $(STORE_TEST_BIN) \
	$(CLI_PARSE_TEST_BIN) $(APP_TEST_BIN)
	./$(CORE_TEST_BIN)
	./$(NET_IO_TEST_BIN)
	./$(STORE_TEST_BIN)
	./$(CLI_PARSE_TEST_BIN)
	./$(APP_TEST_BIN)

test-integration: all server
	bash tests/run_basic_tests.sh
	bash tests/run_db_tests.sh
	bash tests/run_image_ops_tests.sh
	bash tests/run_visual_tests.sh
	bash tests/run_storage_tests.sh
	bash tests/run_cli_compat_tests.sh
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
		all server $(CORE_TEST_BIN) $(NET_IO_TEST_BIN) $(STORE_TEST_BIN) \
		$(CLI_PARSE_TEST_BIN) $(APP_TEST_BIN)

sanitizer-test:
	$(MAKE) clean
	ASAN_OPTIONS=halt_on_error=1:detect_leaks=0 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		$(MAKE) SANITIZE=1 test

clean:
	rm -f $(ALL_OBJ) $(ALL_DEP) imagedb cimagedb imagedb-server \
		$(CORE_TEST_BIN) $(NET_IO_TEST_BIN) $(STORE_TEST_BIN) \
		$(CLI_PARSE_TEST_BIN) $(APP_TEST_BIN)

-include $(ALL_DEP)
