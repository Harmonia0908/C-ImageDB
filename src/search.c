#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "search.h"
#include "feature.h"
#include "database.h"

static int is_deleted(const char *data_dir, int image_id) {
    image_record_t *records;
    int count, i;

    if (db_load_records(data_dir, &records, &count) != 0)
        return 1;

    for (i = 0; i < count; i++) {
        if (records[i].id == image_id) {
            int d = records[i].deleted;
            free(records);
            return d;
        }
    }

    free(records);
    return 1;
}

/* l1 and l2: ascending (lower distance = more similar) */
static int cmp_asc(const void *a, const void *b) {
    const search_result_t *ra = (const search_result_t *)a;
    const search_result_t *rb = (const search_result_t *)b;
    if (ra->value < rb->value) return -1;
    if (ra->value > rb->value) return 1;
    return 0;
}

/* intersection score: descending (higher score = more similar) */
static int cmp_desc(const void *a, const void *b) {
    const search_result_t *ra = (const search_result_t *)a;
    const search_result_t *rb = (const search_result_t *)b;
    if (ra->value > rb->value) return -1;
    if (ra->value < rb->value) return 1;
    return 0;
}

const char *search_metric_name(search_metric_t metric) {
    switch (metric) {
        case METRIC_L1:            return "l1";
        case METRIC_L2:            return "l2";
        case METRIC_INTERSECTION:  return "intersection";
    }
    return "unknown";
}

int search_similar(const char *data_dir, int query_id, int top_k,
                   search_metric_t metric,
                   search_result_t **results, int *result_count) {
    image_feature_t *features;
    image_feature_t query_feat;
    image_record_t query_rec;
    image_record_t *records;
    int rec_count, feat_count;
    int i;
    search_result_t *tmp;
    int tmp_count;

    if (!data_dir || !results || !result_count)
        return -1;

    *results = NULL;
    *result_count = 0;

    if (top_k <= 0 ||
        (metric != METRIC_L1 && metric != METRIC_L2 &&
         metric != METRIC_INTERSECTION))
        return -1;

    /* Reject deleted or non-existent query images */
    if (db_find_record_by_id(data_dir, query_id, &query_rec) != 0)
        return -1;

    /* Load query feature */
    if (db_find_feature_by_id(data_dir, query_id, &query_feat) != 0)
        return -1;

    /* Load all features */
    if (db_load_features(data_dir, &features, &feat_count) != 0)
        return -1;

    /* Load all records (for names) */
    if (db_load_records(data_dir, &records, &rec_count) != 0) {
        free(features);
        return -1;
    }

    if (feat_count == 0) {
        free(features);
        free(records);
        return 0;
    }

    tmp = malloc((size_t)feat_count * sizeof(search_result_t));
    if (!tmp) {
        free(features);
        free(records);
        return -1;
    }

    tmp_count = 0;
    for (i = 0; i < feat_count; i++) {
        int j;

        if (features[i].image_id == query_id)
            continue;
        if (is_deleted(data_dir, features[i].image_id))
            continue;

        tmp[tmp_count].image_id = features[i].image_id;

        /* Look up name */
        tmp[tmp_count].name[0] = '\0';
        for (j = 0; j < rec_count; j++) {
            if (records[j].id == features[i].image_id) {
                snprintf(tmp[tmp_count].name, MAX_NAME_LEN, "%s", records[j].name);
                break;
            }
        }

        switch (metric) {
            case METRIC_L1:
                tmp[tmp_count].value = feature_distance_l1(&query_feat, &features[i]);
                break;
            case METRIC_L2:
                tmp[tmp_count].value = feature_distance_l2(&query_feat, &features[i]);
                break;
            case METRIC_INTERSECTION:
                tmp[tmp_count].value = feature_intersection(&query_feat, &features[i]);
                break;
        }
        tmp_count++;
    }

    free(features);
    free(records);

    if (metric == METRIC_INTERSECTION)
        qsort(tmp, (size_t)tmp_count, sizeof(search_result_t), cmp_desc);
    else
        qsort(tmp, (size_t)tmp_count, sizeof(search_result_t), cmp_asc);

    if (tmp_count > top_k)
        tmp_count = top_k;

    *results = tmp;
    *result_count = tmp_count;

    if (tmp_count == 0) {
        free(tmp);
        *results = NULL;
    }

    return 0;
}
