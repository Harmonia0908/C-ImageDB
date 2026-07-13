#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "net_io.h"

static int failures;
static int send_calls;
static unsigned char captured[64];
static size_t captured_size;

#define CHECK(condition) do {                                                   \
    if (!(condition)) {                                                        \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++;                                                            \
    }                                                                          \
} while (0)

static ssize_t short_send(int fd, const void *buffer, size_t length, int flags) {
    size_t amount;

    CHECK(fd == 42);
    CHECK(flags == 7);
    send_calls++;
    if (send_calls == 2) {
        errno = EINTR;
        return -1;
    }

    amount = (send_calls == 1 && length > 2) ? 2 : length;
    memcpy(captured + captured_size, buffer, amount);
    captured_size += amount;
    return (ssize_t)amount;
}

static ssize_t zero_send(int fd, const void *buffer, size_t length, int flags) {
    (void)fd;
    (void)buffer;
    (void)length;
    (void)flags;
    return 0;
}

int main(void) {
    static const unsigned char payload[] = "partial-write";

    CHECK(net_send_all_with(42, payload, sizeof(payload) - 1, 7,
                            short_send) == 0);
    CHECK(send_calls == 3);
    CHECK(captured_size == sizeof(payload) - 1);
    CHECK(memcmp(captured, payload, sizeof(payload) - 1) == 0);
    CHECK(net_send_all_with(42, payload, sizeof(payload) - 1, 0,
                            zero_send) == -1);

    if (failures != 0) {
        fprintf(stderr, "%d network I/O test(s) failed\n", failures);
        return 1;
    }
    puts("network I/O unit tests: PASS");
    return 0;
}
