#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "process.h"

image_t *process_gray(const image_t *src) {
    image_t *dst;
    size_t i;
    size_t total;

    if (!image_valid(src))
        return NULL;

    total = (size_t)src->width * (size_t)src->height;

    dst = image_create(src->width, src->height, 3);
    if (!dst)
        return NULL;

    for (i = 0; i < total; i++) {
        unsigned char r = src->data[i * 3 + 0];
        unsigned char g = src->data[i * 3 + 1];
        unsigned char b = src->data[i * 3 + 2];
        unsigned char gray = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
        dst->data[i * 3 + 0] = gray;
        dst->data[i * 3 + 1] = gray;
        dst->data[i * 3 + 2] = gray;
    }

    return dst;
}

image_t *process_binary(const image_t *src, int threshold) {
    image_t *gray_img, *dst;
    size_t i;
    size_t total;

    if (!image_valid(src) || threshold < 0 || threshold > 255)
        return NULL;

    gray_img = process_gray(src);
    if (!gray_img)
        return NULL;

    total = (size_t)src->width * (size_t)src->height;
    dst = image_create(src->width, src->height, 3);
    if (!dst) {
        image_destroy(gray_img);
        return NULL;
    }

    for (i = 0; i < total; i++) {
        unsigned char g = gray_img->data[i * 3];
        unsigned char bw = (g >= (unsigned char)threshold) ? 255 : 0;
        dst->data[i * 3 + 0] = bw;
        dst->data[i * 3 + 1] = bw;
        dst->data[i * 3 + 2] = bw;
    }

    image_destroy(gray_img);
    return dst;
}

image_t *process_blur3x3(const image_t *src) {
    image_t *dst;
    int w, h, x, y, c;

    if (!image_valid(src))
        return NULL;

    w = src->width;
    h = src->height;
    dst = image_create(w, h, 3);
    if (!dst)
        return NULL;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < 3; c++) {
                int sum = 0, count = 0, nx, ny;
                for (ny = y - 1; ny <= y + 1; ny++) {
                    for (nx = x - 1; nx <= x + 1; nx++) {
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                            sum += src->data[(ny * w + nx) * 3 + c];
                            count++;
                        }
                    }
                }
                dst->data[(y * w + x) * 3 + c] = (unsigned char)(sum / count);
            }
        }
    }

    return dst;
}

image_t *process_sobel_edge(const image_t *src) {
    image_t *gray_img, *dst;
    int w, h, x, y;

    if (!image_valid(src))
        return NULL;

    gray_img = process_gray(src);
    if (!gray_img)
        return NULL;

    w = src->width;
    h = src->height;
    dst = image_create(w, h, 3);
    if (!dst) {
        image_destroy(gray_img);
        return NULL;
    }

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int gx, gy, mag;
            if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                dst->data[(y * w + x) * 3 + 0] = 0;
                dst->data[(y * w + x) * 3 + 1] = 0;
                dst->data[(y * w + x) * 3 + 2] = 0;
                continue;
            }

            gx = (-1 * gray_img->data[((y-1)*w + (x-1)) * 3])
               + ( 0 * gray_img->data[((y-1)*w +  x   ) * 3])
               + ( 1 * gray_img->data[((y-1)*w + (x+1)) * 3])
               + (-2 * gray_img->data[(( y )*w + (x-1)) * 3])
               + ( 0 * gray_img->data[(( y )*w +  x   ) * 3])
               + ( 2 * gray_img->data[(( y )*w + (x+1)) * 3])
               + (-1 * gray_img->data[((y+1)*w + (x-1)) * 3])
               + ( 0 * gray_img->data[((y+1)*w +  x   ) * 3])
               + ( 1 * gray_img->data[((y+1)*w + (x+1)) * 3]);

            gy = (-1 * gray_img->data[((y-1)*w + (x-1)) * 3])
               + (-2 * gray_img->data[((y-1)*w +  x   ) * 3])
               + (-1 * gray_img->data[((y-1)*w + (x+1)) * 3])
               + ( 0 * gray_img->data[(( y )*w + (x-1)) * 3])
               + ( 0 * gray_img->data[(( y )*w +  x   ) * 3])
               + ( 0 * gray_img->data[(( y )*w + (x+1)) * 3])
               + ( 1 * gray_img->data[((y+1)*w + (x-1)) * 3])
               + ( 2 * gray_img->data[((y+1)*w +  x   ) * 3])
               + ( 1 * gray_img->data[((y+1)*w + (x+1)) * 3]);

            mag = abs(gx) + abs(gy);
            if (mag > 255) mag = 255;

            dst->data[(y * w + x) * 3 + 0] = (unsigned char)mag;
            dst->data[(y * w + x) * 3 + 1] = (unsigned char)mag;
            dst->data[(y * w + x) * 3 + 2] = (unsigned char)mag;
        }
    }

    image_destroy(gray_img);
    return dst;
}

image_t *process_resize_nearest(const image_t *src, int new_w, int new_h) {
    image_t *dst;
    int y, x, c;
    int src_y, src_x;

    if (!image_valid(src))
        return NULL;
    if (new_w <= 0 || new_h <= 0)
        return NULL;

    {
        size_t w = (size_t)new_w;
        size_t h = (size_t)new_h;
        size_t ch = (size_t)src->channels;
        if (w > SIZE_MAX / h || w * h > SIZE_MAX / ch)
            return NULL;
    }

    dst = image_create(new_w, new_h, src->channels);
    if (!dst)
        return NULL;

    for (y = 0; y < new_h; y++) {
        src_y = (int)((double)y * src->height / new_h);
        if (src_y >= src->height) src_y = src->height - 1;

        for (x = 0; x < new_w; x++) {
            src_x = (int)((double)x * src->width / new_w);
            if (src_x >= src->width) src_x = src->width - 1;

            for (c = 0; c < src->channels; c++) {
                dst->data[(y * new_w + x) * src->channels + c] =
                    src->data[(src_y * src->width + src_x) * src->channels + c];
            }
        }
    }

    return dst;
}

image_t *process_rotate(const image_t *src, int degrees) {
    image_t *dst;
    int w, h, new_w, new_h;
    int y, x, c;

    if (!image_valid(src))
        return NULL;
    if (degrees != 90 && degrees != 180 && degrees != 270)
        return NULL;

    w = src->width;
    h = src->height;

    if (degrees == 90 || degrees == 270) {
        new_w = h;
        new_h = w;
    } else {
        new_w = w;
        new_h = h;
    }

    dst = image_create(new_w, new_h, src->channels);
    if (!dst)
        return NULL;

    for (y = 0; y < new_h; y++) {
        for (x = 0; x < new_w; x++) {
            int sx, sy;

            switch (degrees) {
                case 90:
                    sx = y;
                    sy = new_w - 1 - x;
                    break;
                case 180:
                    sx = w - 1 - x;
                    sy = h - 1 - y;
                    break;
                case 270:
                    sx = new_h - 1 - y;
                    sy = x;
                    break;
                default:
                    image_destroy(dst);
                    return NULL;
            }

            for (c = 0; c < src->channels; c++) {
                dst->data[(y * new_w + x) * src->channels + c] =
                    src->data[(sy * w + sx) * src->channels + c];
            }
        }
    }

    return dst;
}

/* -- Phase 6 algorithms -- */

static int cmp_uchar(const void *a, const void *b) {
    return (int)(*(const unsigned char *)a) - (int)(*(const unsigned char *)b);
}

image_t *process_equalize(const image_t *src) {
    image_t *gray_img, *dst;
    int hist[256], cdf[256], lut[256];
    int cdf_min;
    size_t i;
    size_t total;

    if (!image_valid(src))
        return NULL;

    gray_img = process_gray(src);
    if (!gray_img)
        return NULL;

    total = (size_t)gray_img->width * (size_t)gray_img->height;
    memset(hist, 0, sizeof(hist));

    for (i = 0; i < total; i++)
        hist[gray_img->data[i * 3]]++;

    cdf[0] = hist[0];
    for (i = 1; i < 256; i++)
        cdf[i] = cdf[i - 1] + hist[i];

    cdf_min = 0;
    for (i = 0; i < 256; i++) {
        if (cdf[i] > 0) { cdf_min = cdf[i]; break; }
    }

    for (i = 0; i < 256; i++) {
        double denom = (double)((int)total - cdf_min);
        int v = (denom > 0.0)
              ? (int)(((double)(cdf[i] - cdf_min) / denom) * 255.0 + 0.5)
              : 0;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        lut[i] = v;
    }

    dst = image_create(gray_img->width, gray_img->height, 3);
    if (!dst) {
        image_destroy(gray_img);
        return NULL;
    }

    for (i = 0; i < total; i++) {
        unsigned char v = (unsigned char)lut[gray_img->data[i * 3]];
        dst->data[i * 3 + 0] = v;
        dst->data[i * 3 + 1] = v;
        dst->data[i * 3 + 2] = v;
    }

    image_destroy(gray_img);
    return dst;
}

image_t *process_median(const image_t *src, int kernel_size) {
    image_t *dst;
    int w, h, x, y, c, half, xn, yn, cnt;
    unsigned char *window;

    if (!image_valid(src))
        return NULL;
    if (kernel_size != 3 && kernel_size != 5)
        return NULL;

    w = src->width;
    h = src->height;
    dst = image_create(w, h, src->channels);
    if (!dst)
        return NULL;

    half = kernel_size / 2;
    window = malloc((size_t)(kernel_size * kernel_size));
    if (!window) {
        image_destroy(dst);
        return NULL;
    }

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < src->channels; c++) {
                cnt = 0;
                for (yn = y - half; yn <= y + half; yn++) {
                    if (yn < 0 || yn >= h) continue;
                    for (xn = x - half; xn <= x + half; xn++) {
                        if (xn < 0 || xn >= w) continue;
                        window[cnt++] = src->data[(yn * w + xn) * src->channels + c];
                    }
                }
                qsort(window, (size_t)cnt, 1, cmp_uchar);
                dst->data[(y * w + x) * src->channels + c] = window[cnt / 2];
            }
        }
    }

    free(window);
    return dst;
}

image_t *process_gaussian(const image_t *src) {
    image_t *dst;
    int w, h, x, y, c;
    static const int kernel[3][3] = {{1,2,1},{2,4,2},{1,2,1}};

    if (!image_valid(src))
        return NULL;

    w = src->width;
    h = src->height;
    dst = image_create(w, h, src->channels);
    if (!dst)
        return NULL;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < src->channels; c++) {
                int sum = 0, asum = 0, nx, ny;
                for (ny = y - 1; ny <= y + 1; ny++) {
                    for (nx = x - 1; nx <= x + 1; nx++) {
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                            int kw = kernel[ny - y + 1][nx - x + 1];
                            sum += src->data[(ny * w + nx) * src->channels + c] * kw;
                            asum += kw;
                        }
                    }
                }
                dst->data[(y * w + x) * src->channels + c] =
                    (unsigned char)((sum + asum / 2) / asum);
            }
        }
    }

    return dst;
}

image_t *process_adjust(const image_t *src, int brightness, double contrast) {
    image_t *dst;
    size_t total;
    size_t i;

    if (!image_valid(src) || !isfinite(contrast) || contrast <= 0.0)
        return NULL;

    dst = image_create(src->width, src->height, src->channels);
    if (!dst)
        return NULL;

    total = (size_t)src->width * (size_t)src->height * (size_t)src->channels;

    for (i = 0; i < total; i++) {
        double v = ((double)src->data[i] - 128.0) * contrast + 128.0 + (double)brightness;
        if (v < 0.0) v = 0.0;
        if (v > 255.0) v = 255.0;
        dst->data[i] = (unsigned char)(v + 0.5);
    }

    return dst;
}

image_t *process_resize_bilinear(const image_t *src, int new_w, int new_h) {
    image_t *dst;
    int y, x, c;

    if (!image_valid(src))
        return NULL;
    if (new_w <= 0 || new_h <= 0)
        return NULL;

    {
        size_t w = (size_t)new_w;
        size_t h = (size_t)new_h;
        size_t ch = (size_t)src->channels;
        if (w > SIZE_MAX / h || w * h > SIZE_MAX / ch)
            return NULL;
    }

    dst = image_create(new_w, new_h, src->channels);
    if (!dst)
        return NULL;

    for (y = 0; y < new_h; y++) {
        double sry = (new_h > 1) ? (double)y * (src->height - 1) / (new_h - 1) : 0.0;
        int sy0 = (int)sry;
        int sy1 = (sy0 + 1 < src->height) ? sy0 + 1 : sy0;
        double wy = sry - (double)sy0;
        if (sy0 < 0) sy0 = 0;
        if (sy1 >= src->height) sy1 = src->height - 1;

        for (x = 0; x < new_w; x++) {
            double srx = (new_w > 1) ? (double)x * (src->width - 1) / (new_w - 1) : 0.0;
            int sx0 = (int)srx;
            int sx1 = (sx0 + 1 < src->width) ? sx0 + 1 : sx0;
            double wx = srx - (double)sx0;
            if (sx0 < 0) sx0 = 0;
            if (sx1 >= src->width) sx1 = src->width - 1;

            for (c = 0; c < src->channels; c++) {
                double p00 = (double)src->data[(sy0 * src->width + sx0) * src->channels + c];
                double p10 = (double)src->data[(sy0 * src->width + sx1) * src->channels + c];
                double p01 = (double)src->data[(sy1 * src->width + sx0) * src->channels + c];
                double p11 = (double)src->data[(sy1 * src->width + sx1) * src->channels + c];

                double v = (1.0 - wx) * (1.0 - wy) * p00
                         +        wx  * (1.0 - wy) * p10
                         + (1.0 - wx) *        wy  * p01
                         +        wx  *        wy  * p11;
                if (v < 0.0) v = 0.0;
                if (v > 255.0) v = 255.0;
                dst->data[(y * new_w + x) * src->channels + c] = (unsigned char)(v + 0.5);
            }
        }
    }

    return dst;
}
