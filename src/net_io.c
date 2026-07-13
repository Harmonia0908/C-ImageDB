#include <errno.h>
#include <sys/socket.h>

#include "net_io.h"

int net_send_all_with(int fd, const void *data, size_t length, int flags,
                      net_send_fn send_fn) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t sent = 0;

    if ((!data && length > 0) || !send_fn)
        return -1;

    while (sent < length) {
        ssize_t result = send_fn(fd, bytes + sent, length - sent, flags);

        if (result > 0) {
            if ((size_t)result > length - sent)
                return -1;
            sent += (size_t)result;
            continue;
        }
        if (result < 0 && errno == EINTR)
            continue;
        return -1;
    }
    return 0;
}

int net_send_all(int fd, const void *data, size_t length) {
    int flags = 0;

#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#endif
    return net_send_all_with(fd, data, length, flags, send);
}
