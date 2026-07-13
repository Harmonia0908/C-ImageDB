#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "image.h"
#include "common.h"

image_t *image_create(int width, int height, int channels) {
    image_t *img;
    size_t w, h, c, pixels, bytes;

    if (width <= 0 || height <= 0 || channels != IMAGE_CHANNELS)
        return NULL;
    if (width > MAX_IMAGE_WIDTH || height > MAX_IMAGE_HEIGHT)
        return NULL;

    w = (size_t)width;
    h = (size_t)height;
    c = (size_t)channels;

    /* Check pixel count limit and overflow */
    if (w > SIZE_MAX / h) return NULL;
    pixels = w * h;
    if (pixels > MAX_IMAGE_PIXELS) return NULL;
    if (pixels > SIZE_MAX / c) return NULL;
    bytes = pixels * c;

    img = malloc(sizeof(image_t));
    if (!img)
        return NULL;

    img->data = malloc(bytes);
    if (!img->data) {
        free(img);
        return NULL;
    }

    img->width = width;
    img->height = height;
    img->channels = channels;
    memset(img->data, 0, bytes);

    return img;
}

void image_destroy(image_t *img) {
    if (!img)
        return;
    free(img->data);
    free(img);
}

image_t *image_clone(const image_t *src) {
    image_t *dst;
    size_t w, h, c, bytes;

    if (!image_valid(src))
        return NULL;

    dst = image_create(src->width, src->height, src->channels);
    if (!dst)
        return NULL;

    w = (size_t)src->width;
    h = (size_t)src->height;
    c = (size_t)src->channels;
    bytes = w * h * c;
    memcpy(dst->data, src->data, bytes);

    return dst;
}

int image_valid(const image_t *img) {
    size_t width, height;

    if (!img)
        return 0;
    if (img->width <= 0 || img->height <= 0 ||
        img->width > MAX_IMAGE_WIDTH || img->height > MAX_IMAGE_HEIGHT ||
        img->channels != IMAGE_CHANNELS)
        return 0;
    if (!img->data)
        return 0;

    width = (size_t)img->width;
    height = (size_t)img->height;
    if (width > SIZE_MAX / height || width * height > MAX_IMAGE_PIXELS)
        return 0;
    return 1;
}
