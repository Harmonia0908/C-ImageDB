#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "feature.h"

int feature_extract_rgb_hist(const image_t *img, int image_id, image_feature_t *out) {
    size_t i;
    size_t total;
    double sum_r, sum_g, sum_b;

    if (!image_valid(img) || !out)
        return -1;

    memset(out, 0, sizeof(image_feature_t));
    out->image_id = image_id;

    total = (size_t)img->width * (size_t)img->height;
    sum_r = 0.0;
    sum_g = 0.0;
    sum_b = 0.0;

    for (i = 0; i < total; i++) {
        unsigned char r = img->data[i * 3 + 0];
        unsigned char g = img->data[i * 3 + 1];
        unsigned char b = img->data[i * 3 + 2];

        out->r_hist[r]++;
        out->g_hist[g]++;
        out->b_hist[b]++;

        sum_r += r;
        sum_g += g;
        sum_b += b;
    }

    out->avg_r = sum_r / (double)total;
    out->avg_g = sum_g / (double)total;
    out->avg_b = sum_b / (double)total;

    return 0;
}

void feature_print_summary(const image_feature_t *feature) {
    printf("Feature for image %d:\n", feature->image_id);
    printf("  Average R: %.2f\n", feature->avg_r);
    printf("  Average G: %.2f\n", feature->avg_g);
    printf("  Average B: %.2f\n", feature->avg_b);
}

void feature_print_full(const image_feature_t *feature) {
    int i, j;

    printf("R Histogram:\n");
    for (i = 0; i < 256; i += 16) {
        printf("  ");
        for (j = 0; j < 16 && (i + j) < 256; j++) {
            printf("%3d:%-6d ", i + j, feature->r_hist[i + j]);
        }
        printf("\n");
    }

    printf("G Histogram:\n");
    for (i = 0; i < 256; i += 16) {
        printf("  ");
        for (j = 0; j < 16 && (i + j) < 256; j++) {
            printf("%3d:%-6d ", i + j, feature->g_hist[i + j]);
        }
        printf("\n");
    }

    printf("B Histogram:\n");
    for (i = 0; i < 256; i += 16) {
        printf("  ");
        for (j = 0; j < 16 && (i + j) < 256; j++) {
            printf("%3d:%-6d ", i + j, feature->b_hist[i + j]);
        }
        printf("\n");
    }
}

double feature_distance_l1(const image_feature_t *a, const image_feature_t *b) {
    int i;
    double dist = 0.0;

    for (i = 0; i < 256; i++) {
        dist += fabs((double)a->r_hist[i] - (double)b->r_hist[i]);
        dist += fabs((double)a->g_hist[i] - (double)b->g_hist[i]);
        dist += fabs((double)a->b_hist[i] - (double)b->b_hist[i]);
    }

    return dist;
}

double feature_distance_l2(const image_feature_t *a, const image_feature_t *b) {
    int i;
    double sum = 0.0;
    double d;

    for (i = 0; i < 256; i++) {
        d = (double)a->r_hist[i] - (double)b->r_hist[i];
        sum += d * d;
        d = (double)a->g_hist[i] - (double)b->g_hist[i];
        sum += d * d;
        d = (double)a->b_hist[i] - (double)b->b_hist[i];
        sum += d * d;
    }

    return sqrt(sum);
}

double feature_intersection(const image_feature_t *a, const image_feature_t *b) {
    int i;
    double score = 0.0;
    long total_a = 0, total_b = 0;

    /* Compute per-channel pixel totals (all channels have same count) */
    for (i = 0; i < 256; i++) {
        total_a += a->r_hist[i];
        total_b += b->r_hist[i];
    }

    if (total_a == 0 || total_b == 0)
        return 0.0;

    for (i = 0; i < 256; i++) {
        double na_r = (double)a->r_hist[i] / (double)total_a;
        double nb_r = (double)b->r_hist[i] / (double)total_b;
        double na_g = (double)a->g_hist[i] / (double)total_a;
        double nb_g = (double)b->g_hist[i] / (double)total_b;
        double na_b = (double)a->b_hist[i] / (double)total_a;
        double nb_b = (double)b->b_hist[i] / (double)total_b;

        score += (na_r < nb_r) ? na_r : nb_r;
        score += (na_g < nb_g) ? na_g : nb_g;
        score += (na_b < nb_b) ? na_b : nb_b;
    }

    return score / 3.0;
}
