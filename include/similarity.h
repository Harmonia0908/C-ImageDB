#ifndef SIMILARITY_H
#define SIMILARITY_H

#include "common.h"

typedef enum similarity_status {
    SIMILARITY_OK = 0,
    SIMILARITY_ERR_INVALID_TOPK = -1,
    SIMILARITY_ERR_BAD_QUERY = -2,
    SIMILARITY_ERR_DB = -3,
    SIMILARITY_ERR_EMPTY_DB = -4,
    SIMILARITY_ERR_NOMEM = -5
} similarity_status_t;

typedef struct similar_image_result {
    int image_id;
    char image_path[MAX_PATH_LEN];
    double distance;
    int order;
} similar_image_result_t;

int similarity_search_ppm(const char *data_dir,
                          const char *query_path,
                          int top_k,
                          similar_image_result_t **results,
                          int *result_count);

#endif
