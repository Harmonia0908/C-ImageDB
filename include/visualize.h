#ifndef VISUALIZE_H
#define VISUALIZE_H

#include "image.h"
#include "feature.h"

/* Draw an RGB histogram as a 768x256 image.
 * Left 256 cols = R, middle = G, right = B.
 * Bar height normalized by max bin value across all three channels. */
image_t *visualize_hist_image(const image_feature_t *feature);

/* Create a horizontal contact sheet from an array of pre-scaled images.
 * images[0..count-1] are concatenated left-to-right.
 * Each image must be the same dimensions (thumb_w x thumb_h).
 * Returns a new image_t; caller frees it. */
image_t *visualize_contact_sheet(image_t **images, int count,
                                 int thumb_w, int thumb_h);

#endif
