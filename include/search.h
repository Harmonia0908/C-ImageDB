#ifndef SEARCH_H
#define SEARCH_H

#include "common.h"

typedef enum {
    METRIC_L1,
    METRIC_L2,
    METRIC_INTERSECTION
} search_metric_t;

typedef struct search_result {
    int image_id;
    char name[MAX_NAME_LEN];
    double value;       /* distance (l1/l2) or similarity score (intersection) */
} search_result_t;

int search_similar(const char *data_dir, int query_id, int top_k,
                   search_metric_t metric,
                   search_result_t **results, int *result_count);

const char *search_metric_name(search_metric_t metric);

#endif
