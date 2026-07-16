#ifndef APP_H
#define APP_H

#include "database.h"
#include "image.h"
#include "search.h"
#include "similarity.h"
#include "verify.h"

typedef enum app_command_kind {
    APP_COMMAND_HELP,
    APP_COMMAND_INIT,
    APP_COMMAND_IMPORT,
    APP_COMMAND_LIST,
    APP_COMMAND_INFO,
    APP_COMMAND_GRAY,
    APP_COMMAND_BINARY,
    APP_COMMAND_BLUR,
    APP_COMMAND_EDGE,
    APP_COMMAND_HIST,
    APP_COMMAND_SEARCH,
    APP_COMMAND_SEARCH_SIMILAR,
    APP_COMMAND_RESIZE,
    APP_COMMAND_ROTATE,
    APP_COMMAND_DELETE,
    APP_COMMAND_EQUALIZE,
    APP_COMMAND_MEDIAN,
    APP_COMMAND_GAUSSIAN,
    APP_COMMAND_ADJUST,
    APP_COMMAND_RESIZE_BILINEAR,
    APP_COMMAND_FIND_NAME,
    APP_COMMAND_QUERY,
    APP_COMMAND_STATS,
    APP_COMMAND_COMPACT,
    APP_COMMAND_EXPORT,
    APP_COMMAND_REPORT,
    APP_COMMAND_VERIFY,
    APP_COMMAND_REPAIR,
    APP_COMMAND_HIST_EXPORT,
    APP_COMMAND_HIST_IMAGE,
    APP_COMMAND_SEARCH_EXPORT,
    APP_COMMAND_SEARCH_CONTACT
} app_command_kind_t;

typedef enum app_query_field {
    APP_QUERY_ID,
    APP_QUERY_NAME,
    APP_QUERY_WIDTH,
    APP_QUERY_HEIGHT,
    APP_QUERY_FORMAT,
    APP_QUERY_SIZE
} app_query_field_t;

typedef enum app_query_operator {
    APP_QUERY_EQ,
    APP_QUERY_NE,
    APP_QUERY_GT,
    APP_QUERY_GE,
    APP_QUERY_LT,
    APP_QUERY_LE,
    APP_QUERY_CONTAINS
} app_query_operator_t;

typedef struct app_command {
    app_command_kind_t kind;
    union {
        struct { const char *path; } import_file;
        struct { int id; } id;
        struct { int id; const char *output_path; } image_output;
        struct { int id; int threshold; const char *output_path; } binary;
        struct { int id; int top_k; search_metric_t metric; } search;
        struct { const char *query_path; int top_k; } search_similar;
        struct { int id; int width; int height; const char *output_path; } resize;
        struct { int id; int degrees; const char *output_path; } rotate;
        struct { int id; int kernel_size; const char *output_path; } median;
        struct {
            int id;
            int brightness;
            double contrast;
            const char *output_path;
        } adjust;
        struct { const char *keyword; } find_name;
        struct {
            app_query_field_t field;
            app_query_operator_t op;
            long numeric_value;
            const char *text_value;
        } query;
        struct { const char *output_path; } export_file;
        struct { const char *output_dir; const char *report_path; } report;
        struct { int id; const char *output_path; int normalized; } hist_export;
        struct {
            int id;
            int top_k;
            const char *output_path;
            search_metric_t metric;
        } search_output;
    } args;
} app_command_t;

typedef enum app_status {
    APP_STATUS_OK = 0,
    APP_STATUS_INVALID_ARGUMENT,
    APP_STATUS_STORE_INIT_FAILED,
    APP_STATUS_IMAGE_FILE_READ_FAILED,
    APP_STATUS_DUPLICATE_IMAGE,
    APP_STATUS_FEATURE_EXTRACT_FAILED,
    APP_STATUS_INVALID_FILENAME,
    APP_STATUS_ID_ALLOCATION_FAILED,
    APP_STATUS_STORE_PATH_TOO_LONG,
    APP_STATUS_IMAGE_COPY_FAILED,
    APP_STATUS_IMPORT_COMMIT_FAILED,
    APP_STATUS_RECORDS_LOAD_FAILED,
    APP_STATUS_RECORD_NOT_FOUND,
    APP_STATUS_DELETE_FAILED,
    APP_STATUS_STORED_IMAGE_READ_FAILED,
    APP_STATUS_PROCESS_FAILED,
    APP_STATUS_FEATURE_NOT_FOUND,
    APP_STATUS_SEARCH_FAILED,
    APP_STATUS_QUERY_FILE_UNREADABLE,
    APP_STATUS_INVALID_QUERY_IMAGE,
    APP_STATUS_EMPTY_STORE,
    APP_STATUS_OUT_OF_MEMORY,
    APP_STATUS_SIMILARITY_FAILED,
    APP_STATUS_INVALID_DIMENSIONS,
    APP_STATUS_INVALID_ROTATION,
    APP_STATUS_INVALID_KERNEL_SIZE,
    APP_STATUS_KEYWORD_EMPTY,
    APP_STATUS_FEATURES_LOAD_FAILED,
    APP_STATUS_COMPACT_FAILED,
    APP_STATUS_REPORT_FAILED,
    APP_STATUS_VERIFY_FAILED,
    APP_STATUS_REPAIR_FAILED,
    APP_STATUS_HISTOGRAM_IMAGE_FAILED,
    APP_STATUS_TOP_K_TOO_LARGE,
    APP_STATUS_CONTACT_SHEET_FAILED
} app_status_t;

typedef struct app_context {
    /* Borrowed Store directory path; it must remain valid for the call. */
    const char *data_dir;
} app_context_t;

typedef struct app_record_list {
    image_record_t *items;
    int count;
} app_record_list_t;

typedef struct app_image_result {
    image_record_t source_record;
    image_t *image;
    int image_count;
} app_image_result_t;

typedef struct app_histogram_result {
    image_record_t record;
    image_feature_t feature;
} app_histogram_result_t;

typedef struct app_search_results {
    search_result_t *items;
    int count;
} app_search_results_t;

typedef struct app_similarity_results {
    similar_image_result_t *items;
    int count;
} app_similarity_results_t;

typedef struct app_search_export_item {
    search_result_t result;
    char path[MAX_PATH_LEN];
} app_search_export_item_t;

typedef struct app_search_export_results {
    app_search_export_item_t *items;
    int count;
} app_search_export_results_t;

typedef struct app_stats_result {
    int total_records;
    int active_records;
    int deleted_records;
    long total_image_size;
    int ppm_count;
    int bmp_count;
    double average_width;
    double average_height;
    int feature_records;
} app_stats_result_t;

typedef struct app_compact_result {
    int before_count;
    int after_count;
} app_compact_result_t;

typedef struct app_result {
    app_command_kind_t kind;
    char detail_path[MAX_PATH_LEN];
    int detail_value;
    int auxiliary_status;
    union {
        image_record_t record;
        app_record_list_t records;
        app_image_result_t image;
        app_histogram_result_t histogram;
        app_search_results_t search;
        app_similarity_results_t similarity;
        app_search_export_results_t search_export;
        app_stats_result_t stats;
        app_compact_result_t compact;
        verify_summary_t verify;
        repair_summary_t repair;
    } data;
} app_result_t;

/* Execute one already-parsed command without reading argv or writing terminal
 * output. String members in command and context are borrowed for the call.
 * On valid pointer arguments, result is always initialized and may be passed
 * to app_result_destroy after either success or failure. Successful result
 * payload allocations and Image instances are owned by result. */
app_status_t app_execute(const app_context_t *context,
                         const app_command_t *command,
                         app_result_t *result);
/* Release every allocation owned by result. Safe for zeroed results. */
void app_result_destroy(app_result_t *result);

#endif
