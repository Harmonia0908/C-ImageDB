#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "common.h"
#include "visualize.h"

#define HIST_IMG_W  768
#define HIST_IMG_H  256

image_t *visualize_hist_image(const image_feature_t *feature) {
    image_t *img;
    int y, bin;
    int max_r, max_g, max_b, overall_max;

    if (!feature) return NULL;

    img = image_create(HIST_IMG_W, HIST_IMG_H, 3);
    if (!img) return NULL;

    max_r = max_g = max_b = 1;
    for (bin = 0; bin < 256; bin++) {
        if (feature->r_hist[bin] > max_r) max_r = feature->r_hist[bin];
        if (feature->g_hist[bin] > max_g) max_g = feature->g_hist[bin];
        if (feature->b_hist[bin] > max_b) max_b = feature->b_hist[bin];
    }
    overall_max = max_r;
    if (max_g > overall_max) overall_max = max_g;
    if (max_b > overall_max) overall_max = max_b;
    if (overall_max < 1) overall_max = 1;

    memset(img->data, 0, (size_t)HIST_IMG_W * HIST_IMG_H * 3);

    /* R channel (cols 0..255) */
    for (bin = 0; bin < 256; bin++) {
        int bar_h = (int)((double)feature->r_hist[bin] / overall_max * (HIST_IMG_H - 1));
        for (y = 0; y < bar_h; y++) {
            int py = HIST_IMG_H - 1 - y;
            unsigned char *p = &img->data[(py * HIST_IMG_W + bin) * 3];
            p[0] = 255; p[1] = 0; p[2] = 0;
        }
    }

    /* G channel (cols 256..511) */
    for (bin = 0; bin < 256; bin++) {
        int bar_h = (int)((double)feature->g_hist[bin] / overall_max * (HIST_IMG_H - 1));
        for (y = 0; y < bar_h; y++) {
            int py = HIST_IMG_H - 1 - y;
            unsigned char *p = &img->data[(py * HIST_IMG_W + 256 + bin) * 3];
            p[0] = 0; p[1] = 255; p[2] = 0;
        }
    }

    /* B channel (cols 512..767) */
    for (bin = 0; bin < 256; bin++) {
        int bar_h = (int)((double)feature->b_hist[bin] / overall_max * (HIST_IMG_H - 1));
        for (y = 0; y < bar_h; y++) {
            int py = HIST_IMG_H - 1 - y;
            unsigned char *p = &img->data[(py * HIST_IMG_W + 512 + bin) * 3];
            p[0] = 0; p[1] = 0; p[2] = 255;
        }
    }

    return img;
}

image_t *visualize_contact_sheet(image_t **images, int count,
                                 int thumb_w, int thumb_h) {
    image_t *sheet;
    int total_w, i, y, x, c;

    if (!images || count <= 0 || thumb_w <= 0 || thumb_h <= 0)
        return NULL;

    /* Check count won't cause overflow before multiplication */
    if ((size_t)count > (size_t)INT_MAX / (size_t)thumb_w)
        return NULL;

    for (i = 0; i < count; i++) {
        if (!image_valid(images[i])) return NULL;
        if (images[i]->width != thumb_w || images[i]->height != thumb_h)
            return NULL;
    }

    {
        size_t tw = (size_t)thumb_w * (size_t)count;
        size_t th = (size_t)thumb_h;
        if (tw > (size_t)INT_MAX || tw > (size_t)MAX_IMAGE_WIDTH)
            return NULL;
        if (tw > SIZE_MAX / th || tw * th > SIZE_MAX / 3)
            return NULL;
        total_w = (int)tw;
    }

    sheet = image_create(total_w, thumb_h, images[0]->channels);
    if (!sheet) return NULL;

    for (i = 0; i < count; i++) {
        int off_x = i * thumb_w;
        for (y = 0; y < thumb_h; y++) {
            for (x = 0; x < thumb_w; x++) {
                for (c = 0; c < images[i]->channels; c++) {
                    sheet->data[(y * total_w + off_x + x) * images[i]->channels + c] =
                        images[i]->data[(y * thumb_w + x) * images[i]->channels + c];
                }
            }
        }
    }

    return sheet;
}
