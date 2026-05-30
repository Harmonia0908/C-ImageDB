#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define MAX_NAME_LEN        128
#define MAX_PATH_LEN        256
#define MAX_CMDLINE         1024

/* Image size bounds to prevent runaway allocations */
#define MAX_IMAGE_WIDTH     16384
#define MAX_IMAGE_HEIGHT    16384
#define MAX_IMAGE_PIXELS    (256UL * 1024 * 1024)  /* 256 megapixels */

#endif
