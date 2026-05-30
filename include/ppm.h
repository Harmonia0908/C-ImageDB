#ifndef PPM_H
#define PPM_H

#include "image.h"

image_t *ppm_read(const char *path);
int ppm_write(const char *path, const image_t *img);

#endif
