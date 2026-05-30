#include <stdlib.h>
#include <stdio.h>
#include "similarity.h"
#include "database.h"
#include "feature.h"
#include "ppm.h"

static int cmp_similarity_result(const void *a, const void *b) {
    const similar_image_result_t *ra = (const similar_image_result_t *)a;
    const similar_image_result_t *rb = (const similar_image_result_t *)b;

    if (ra->distance < rb->distance) return -1;
    if (ra->distance > rb->distance) return 1;
    return ra->order - rb->order;
}

static int find_feature(const image_feature_t *features, int count,
                        int image_id, image_feature_t *out) {
    int i;

    for (i = 0; i < count; i++) {
        if (features[i].image_id == image_id) {
            *out = features[i];
            return 1;
        }
    }

    return 0;
}

int similarity_search_ppm(const char *data_dir,
                          const char *query_path,
                          int top_k,
                          similar_image_result_t **results,
                          int *result_count) {
    image_t *query_img = NULL;
    image_feature_t query_feature;
    image_record_t *records = NULL;
    image_feature_t *features = NULL;
    similar_image_result_t *tmp = NULL;
    int rec_count = 0;
    int feat_count = 0;
    int active_count = 0;
    int tmp_count = 0;
    int i;

    if (!results || !result_count)
        return SIMILARITY_ERR_DB;

    *results = NULL;
    *result_count = 0;

    if (top_k <= 0)
        return SIMILARITY_ERR_INVALID_TOPK;

    query_img = ppm_read(query_path);
    if (!query_img)
        return SIMILARITY_ERR_BAD_QUERY;

    if (feature_extract_rgb_hist(query_img, 0, &query_feature) != 0) {
        image_destroy(query_img);
        return SIMILARITY_ERR_BAD_QUERY;
    }
    image_destroy(query_img);

    if (db_load_records(data_dir, &records, &rec_count) != 0)
        return SIMILARITY_ERR_DB;

    if (db_load_features(data_dir, &features, &feat_count) != 0) {
        free(records);
        return SIMILARITY_ERR_DB;
    }

    for (i = 0; i < rec_count; i++) {
        if (!records[i].deleted)
            active_count++;
    }

    if (active_count == 0) {
        free(records);
        free(features);
        return SIMILARITY_ERR_EMPTY_DB;
    }

    tmp = malloc((size_t)active_count * sizeof(similar_image_result_t));
    if (!tmp) {
        free(records);
        free(features);
        return SIMILARITY_ERR_NOMEM;
    }

    for (i = 0; i < rec_count; i++) {
        image_feature_t db_feature;

        if (records[i].deleted)
            continue;

        if (!find_feature(features, feat_count, records[i].id, &db_feature))
            continue;

        tmp[tmp_count].image_id = records[i].id;
        snprintf(tmp[tmp_count].image_path, MAX_PATH_LEN, "%s", records[i].path);
        tmp[tmp_count].distance = feature_distance_l1(&query_feature, &db_feature);
        tmp[tmp_count].order = i;
        tmp_count++;
    }

    free(records);
    free(features);

    if (tmp_count == 0) {
        free(tmp);
        return SIMILARITY_ERR_EMPTY_DB;
    }

    qsort(tmp, (size_t)tmp_count, sizeof(similar_image_result_t),
          cmp_similarity_result);

    if (tmp_count > top_k)
        tmp_count = top_k;

    *results = tmp;
    *result_count = tmp_count;
    return SIMILARITY_OK;
}
