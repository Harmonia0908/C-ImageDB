#ifndef BMP_H
#define BMP_H

#include "image.h"

image_t *bmp_read(const char *path);
int bmp_write(const char *path, const image_t *img);

#endif
