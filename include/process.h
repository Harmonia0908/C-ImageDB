#ifndef PROCESS_H
#define PROCESS_H

#include "image.h"

image_t *process_gray(const image_t *src);
image_t *process_binary(const image_t *src, int threshold);
image_t *process_blur3x3(const image_t *src);
image_t *process_sobel_edge(const image_t *src);
image_t *process_resize_nearest(const image_t *src, int new_w, int new_h);
image_t *process_rotate(const image_t *src, int degrees);
image_t *process_equalize(const image_t *src);
image_t *process_median(const image_t *src, int kernel_size);
image_t *process_gaussian(const image_t *src);
image_t *process_adjust(const image_t *src, int brightness, double contrast);
image_t *process_resize_bilinear(const image_t *src, int new_w, int new_h);

#endif
