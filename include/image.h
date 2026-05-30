#ifndef IMAGE_H
#define IMAGE_H

typedef struct image {
    int width;
    int height;
    int channels;
    unsigned char *data;
} image_t;

image_t *image_create(int width, int height, int channels);
void image_destroy(image_t *img);
image_t *image_clone(const image_t *src);
int image_valid(const image_t *img);

#endif
