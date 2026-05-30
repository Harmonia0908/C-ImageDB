#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "common.h"
#include "bmp.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t signature;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;
} bmp_file_header_t;

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pixels_per_meter;
    int32_t  y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} bmp_info_header_t;
#pragma pack(pop)

#define BMP_SIGNATURE    0x4D42
#define BI_RGB           0
#define BMP_HEADER_SIZE  40

image_t *bmp_read(const char *path) {
    FILE *fp;
    bmp_file_header_t fh;
    bmp_info_header_t ih;
    image_t *img;
    int width, height, y;
    size_t row_raw, row_padded;
    unsigned char *row_buf;

    fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    if (fread(&fh, sizeof(fh), 1, fp) != 1 || fh.signature != BMP_SIGNATURE) {
        fclose(fp);
        return NULL;
    }

    if (fread(&ih, sizeof(ih), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    if (ih.header_size != BMP_HEADER_SIZE ||
        ih.bit_count != 24 ||
        ih.compression != BI_RGB ||
        ih.planes != 1) {
        fclose(fp);
        return NULL;
    }

    width = ih.width;
    height = ih.height;

    if (width <= 0 || height <= 0 ||
        width > MAX_IMAGE_WIDTH || height > MAX_IMAGE_HEIGHT) {
        fclose(fp);
        return NULL;
    }

    /* Check size_t overflow and pixel limit */
    {
        size_t w = (size_t)width;
        size_t h = (size_t)height;
        if (w > SIZE_MAX / 3 || h > SIZE_MAX / (w * 3))
            { fclose(fp); return NULL; }
        if (w * h > MAX_IMAGE_PIXELS)
            { fclose(fp); return NULL; }
    }

    img = image_create(width, height, 3);
    if (!img) {
        fclose(fp);
        return NULL;
    }

    /* Row sizes using size_t to avoid int overflow */
    row_raw = (size_t)width * 3;
    row_padded = (row_raw + 3) & ~((size_t)3);

    row_buf = malloc(row_padded);
    if (!row_buf) {
        image_destroy(img);
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, fh.data_offset, SEEK_SET) != 0) {
        free(row_buf);
        image_destroy(img);
        fclose(fp);
        return NULL;
    }

    for (y = 0; y < height; y++) {
        int dst_y = height - 1 - y;
        int x;

        if (fread(row_buf, 1, row_padded, fp) != row_padded) {
            free(row_buf);
            image_destroy(img);
            fclose(fp);
            return NULL;
        }

        for (x = 0; x < width; x++) {
            unsigned char *dst = &img->data[(dst_y * width + x) * 3];
            dst[0] = row_buf[x * 3 + 2];
            dst[1] = row_buf[x * 3 + 1];
            dst[2] = row_buf[x * 3 + 0];
        }
    }

    free(row_buf);
    fclose(fp);
    return img;
}

int bmp_write(const char *path, const image_t *img) {
    FILE *fp;
    bmp_file_header_t fh;
    bmp_info_header_t ih;
    size_t row_raw, row_padded, data_offset, file_size;
    unsigned char *row_buf;
    int y;

    if (!image_valid(img) || img->channels != 3)
        return -1;

    row_raw = (size_t)img->width * 3;
    row_padded = (row_raw + 3) & ~((size_t)3);
    data_offset = sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t);

    /* Check file_size fits in uint32_t */
    if (row_padded > SIZE_MAX / (size_t)img->height)
        return -1;
    file_size = data_offset + row_padded * (size_t)img->height;
    if (file_size > (size_t)UINT32_MAX)
        return -1;

    fp = fopen(path, "wb");
    if (!fp)
        return -1;

    memset(&fh, 0, sizeof(fh));
    fh.signature = BMP_SIGNATURE;
    fh.file_size = (uint32_t)file_size;
    fh.data_offset = (uint32_t)data_offset;
    if (fwrite(&fh, sizeof(fh), 1, fp) != 1) { fclose(fp); return -1; }

    memset(&ih, 0, sizeof(ih));
    ih.header_size = BMP_HEADER_SIZE;
    ih.width = img->width;
    ih.height = img->height;
    ih.planes = 1;
    ih.bit_count = 24;
    ih.compression = BI_RGB;
    ih.image_size = (uint32_t)(row_padded * (size_t)img->height);
    if (fwrite(&ih, sizeof(ih), 1, fp) != 1) { fclose(fp); return -1; }

    row_buf = calloc(1, row_padded);
    if (!row_buf) { fclose(fp); return -1; }

    for (y = 0; y < img->height; y++) {
        int src_y = img->height - 1 - y;
        int x;

        for (x = 0; x < img->width; x++) {
            const unsigned char *src = &img->data[(src_y * img->width + x) * 3];
            row_buf[x * 3 + 0] = src[2];
            row_buf[x * 3 + 1] = src[1];
            row_buf[x * 3 + 2] = src[0];
        }

        if (fwrite(row_buf, 1, row_padded, fp) != row_padded) {
            free(row_buf);
            fclose(fp);
            return -1;
        }
    }

    free(row_buf);
    if (fclose(fp) != 0) return -1;
    return 0;
}
