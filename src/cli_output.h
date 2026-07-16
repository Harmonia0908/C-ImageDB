#ifndef CLI_OUTPUT_H
#define CLI_OUTPUT_H

#include "app.h"

typedef enum cli_output_status {
    CLI_OUTPUT_OK = 0,
    CLI_OUTPUT_OPEN_FAILED,
    CLI_OUTPUT_WRITE_FAILED,
    CLI_OUTPUT_FINISH_FAILED
} cli_output_status_t;

cli_output_status_t cli_output_write_image(const char *path,
                                           const image_t *image);
cli_output_status_t cli_output_write_records(
    const char *path, const image_record_t *records, int count,
    int *exported_count);
cli_output_status_t cli_output_write_histogram(
    const char *path, const image_feature_t *feature, int normalized);
cli_output_status_t cli_output_write_search(
    const char *path, const app_search_export_item_t *items, int count,
    search_metric_t metric);

#endif
