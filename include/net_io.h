#ifndef NET_IO_H
#define NET_IO_H

#include <stddef.h>
#include <sys/types.h>

typedef ssize_t (*net_send_fn)(int fd, const void *buffer,
                               size_t length, int flags);

/* Send exactly length bytes, retrying EINTR and short writes. The callback
 * variant keeps the stream-I/O loop deterministic and directly testable. */
int net_send_all_with(int fd, const void *data, size_t length, int flags,
                      net_send_fn send_fn);
int net_send_all(int fd, const void *data, size_t length);

#endif
