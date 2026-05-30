#ifndef FEATURE_H
#define FEATURE_H

#include "image.h"

typedef struct image_feature {
    int image_id;
    int r_hist[256];
    int g_hist[256];
    int b_hist[256];
    double avg_r;
    double avg_g;
    double avg_b;
} image_feature_t;

int feature_extract_rgb_hist(const image_t *img, int image_id, image_feature_t *out);
void feature_print_summary(const image_feature_t *feature);
void feature_print_full(const image_feature_t *feature);

/* Distance / similarity metrics.
 * l1 and l2 return distance (lower = more similar).
 * intersection returns similarity score in [0.0, 1.0] (higher = more similar). */
double feature_distance_l1(const image_feature_t *a, const image_feature_t *b);
double feature_distance_l2(const image_feature_t *a, const image_feature_t *b);
double feature_intersection(const image_feature_t *a, const image_feature_t *b);

#endif
