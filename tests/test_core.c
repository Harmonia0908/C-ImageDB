#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "feature.h"
#include "image.h"
#include "ppm.h"
#include "process.h"

static int failures;
static unsigned int fixture_id;

#define CHECK(condition) do {                                                   \
    if (!(condition)) {                                                        \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++;                                                            \
    }                                                                          \
} while (0)

static int write_fixture(const unsigned char *header, size_t header_len,
                         const unsigned char *pixels, size_t pixel_len,
                         char *path, size_t path_size) {
    FILE *fp;
    int n;

    n = snprintf(path, path_size, "/tmp/cimagedb_core_%ld_%u.ppm",
                 (long)getpid(), fixture_id++);
    if (n < 0 || (size_t)n >= path_size)
        return -1;

    fp = fopen(path, "wb");
    if (!fp)
        return -1;
    if (fwrite(header, 1, header_len, fp) != header_len ||
        (pixel_len > 0 && fwrite(pixels, 1, pixel_len, fp) != pixel_len) ||
        fclose(fp) != 0) {
        unlink(path);
        return -1;
    }
    return 0;
}

static image_t *read_fixture(const char *header,
                             const unsigned char *pixels, size_t pixel_len) {
    char path[128];
    image_t *img;

    if (write_fixture((const unsigned char *)header, strlen(header), pixels,
                      pixel_len, path, sizeof(path)) != 0)
        return NULL;
    img = ppm_read(path);
    unlink(path);
    return img;
}

static void test_ppm_valid_and_comments(void) {
    const unsigned char pixel[] = {1, 2, 3};
    image_t *img = read_fixture("P6\n# comment\n1 1\n255\n", pixel,
                                sizeof(pixel));

    CHECK(img != NULL);
    if (img) {
        CHECK(img->width == 1);
        CHECK(img->height == 1);
        CHECK(img->channels == 3);
        CHECK(memcmp(img->data, pixel, sizeof(pixel)) == 0);
    }
    image_destroy(img);
}

static void test_ppm_crlf_header(void) {
    const unsigned char pixel[] = {17, 34, 51};
    image_t *img = read_fixture("P6\r\n1 1\r\n255\r\n", pixel,
                                sizeof(pixel));

    CHECK(img != NULL);
    if (img)
        CHECK(memcmp(img->data, pixel, sizeof(pixel)) == 0);
    image_destroy(img);
}

static void test_ppm_rejects_malformed_input(void) {
    const unsigned char pixel[] = {1, 2, 3};
    image_t *img;

    img = read_fixture("P3\n1 1\n255\n", pixel, sizeof(pixel));
    CHECK(img == NULL);
    image_destroy(img);

    img = read_fixture("P6\n0 1\n255\n", pixel, sizeof(pixel));
    CHECK(img == NULL);
    image_destroy(img);

    img = read_fixture("P6\n16385 1\n255\n", pixel, sizeof(pixel));
    CHECK(img == NULL);
    image_destroy(img);

    img = read_fixture("P6\n999999999999999999999 2\n255\n", pixel,
                       sizeof(pixel));
    CHECK(img == NULL);
    image_destroy(img);

    img = read_fixture("P6\n1 1\n65535\n", pixel, sizeof(pixel));
    CHECK(img == NULL);
    image_destroy(img);

    img = read_fixture("P6\n1 1\n255\n", pixel, 2);
    CHECK(img == NULL);
    image_destroy(img);
}

static void test_ppm_preserves_binary_whitespace(void) {
    const unsigned char pixel[] = {'\n', ' ', '#'};
    image_t *img = read_fixture("P6\n1 1\n255\n", pixel, sizeof(pixel));

    CHECK(img != NULL);
    if (img)
        CHECK(memcmp(img->data, pixel, sizeof(pixel)) == 0);
    image_destroy(img);
}

static void test_image_model_guards(void) {
    unsigned char byte = 7;
    image_t malformed = {1, 1, 1, &byte};
    image_t oversized = {MAX_IMAGE_WIDTH + 1, 1, 3, &byte};
    image_feature_t feature;

    CHECK(image_create(1, 1, 1) == NULL);
    CHECK(!image_valid(&malformed));
    CHECK(!image_valid(&oversized));
    CHECK(process_gray(&malformed) == NULL);
    CHECK(feature_extract_rgb_hist(&malformed, 1, &feature) == -1);
}

static void test_one_pixel_processing(void) {
    image_t *src = image_create(1, 1, 3);
    image_t *result;
    image_feature_t feature;

    CHECK(src != NULL);
    if (!src)
        return;
    src->data[0] = 30;
    src->data[1] = 60;
    src->data[2] = 90;

    result = process_gray(src);
    CHECK(result != NULL);
    if (result)
        CHECK(result->data[0] == 54 && result->data[1] == 54 &&
              result->data[2] == 54);
    image_destroy(result);

    result = process_blur3x3(src);
    CHECK(result != NULL);
    if (result)
        CHECK(memcmp(result->data, src->data, 3) == 0);
    image_destroy(result);

    result = process_sobel_edge(src);
    CHECK(result != NULL);
    if (result)
        CHECK(result->data[0] == 0 && result->data[1] == 0 &&
              result->data[2] == 0);
    image_destroy(result);

    result = process_median(src, 3);
    CHECK(result != NULL);
    if (result)
        CHECK(memcmp(result->data, src->data, 3) == 0);
    image_destroy(result);

    result = process_gaussian(src);
    CHECK(result != NULL);
    if (result)
        CHECK(memcmp(result->data, src->data, 3) == 0);
    image_destroy(result);

    result = process_resize_bilinear(src, 1, 7);
    CHECK(result != NULL);
    if (result)
        CHECK(result->width == 1 && result->height == 7);
    image_destroy(result);

    CHECK(process_rotate(src, 45) == NULL);
    CHECK(process_adjust(src, 0, NAN) == NULL);
    CHECK(process_adjust(src, 0, INFINITY) == NULL);
    CHECK(process_adjust(src, 0, 0.0) == NULL);

    CHECK(feature_extract_rgb_hist(src, 7, &feature) == 0);
    CHECK(feature.image_id == 7);
    CHECK(feature.r_hist[30] == 1);
    CHECK(feature.g_hist[60] == 1);
    CHECK(feature.b_hist[90] == 1);
    CHECK(feature_intersection(&feature, &feature) == 1.0);

    image_destroy(src);
}

int main(void) {
    test_ppm_valid_and_comments();
    test_ppm_crlf_header();
    test_ppm_rejects_malformed_input();
    test_ppm_preserves_binary_whitespace();
    test_image_model_guards();
    test_one_pixel_processing();

    if (failures != 0) {
        fprintf(stderr, "%d core test(s) failed\n", failures);
        return 1;
    }
    puts("core unit tests: PASS");
    return 0;
}
