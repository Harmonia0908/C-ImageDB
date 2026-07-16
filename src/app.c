#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app.h"
#include "bmp.h"
#include "feature.h"
#include "ppm.h"
#include "process.h"
#include "report.h"
#include "visualize.h"

#define COPY_BUFFER_SIZE 4096

static image_t *read_image_file(const char *path) {
    const char *extension = strrchr(path, '.');
    if (extension && strcasecmp(extension, ".bmp") == 0)
        return bmp_read(path);
    return ppm_read(path);
}

static const char *stored_extension(const char *path) {
    const char *extension = strrchr(path, '.');
    return extension && strcasecmp(extension, ".bmp") == 0 ? ".bmp" : ".ppm";
}

static const char *record_format(const image_record_t *record) {
    const char *extension = strrchr(record->path, '.');
    if (!extension)
        return "unknown";
    return strcasecmp(extension, ".bmp") == 0 ? "BMP" : "PPM";
}

static uint64_t hash_pixels(const unsigned char *data, size_t length) {
    uint64_t hash = 5381;
    size_t i;
    for (i = 0; i < length; i++)
        hash = ((hash << 5) + hash) + data[i];
    return hash;
}

static int copy_file(const char *source_path, const char *destination_path) {
    FILE *source;
    FILE *destination;
    char buffer[COPY_BUFFER_SIZE];
    size_t count;
    int source_close;
    int destination_close;

    source = fopen(source_path, "rb");
    if (!source)
        return -1;
    destination = fopen(destination_path, "wbx");
    if (!destination) {
        fclose(source);
        return -1;
    }

    while ((count = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, count, destination) != count) {
            fclose(source);
            fclose(destination);
            unlink(destination_path);
            return -1;
        }
    }
    if (ferror(source)) {
        fclose(source);
        fclose(destination);
        unlink(destination_path);
        return -1;
    }

    source_close = fclose(source);
    destination_close = fclose(destination);
    if (source_close != 0 || destination_close != 0) {
        unlink(destination_path);
        return -1;
    }
    return 0;
}

static long get_file_size(const char *path) {
    struct stat status;
    return stat(path, &status) == 0 ? status.st_size : -1;
}

static int image_is_duplicate(const char *data_dir, uint64_t hash) {
    image_record_t *records;
    int count;
    int i;
    int duplicate = 0;

    if (db_load_records(data_dir, &records, &count) != 0)
        return 0;
    for (i = 0; i < count; i++) {
        if (!records[i].deleted && records[i].content_hash == hash) {
            duplicate = 1;
            break;
        }
    }
    free(records);
    return duplicate;
}

static void set_detail_path(app_result_t *result, const char *path) {
    if (!path)
        return;
    snprintf(result->detail_path, sizeof(result->detail_path), "%s", path);
}

static app_status_t execute_import(const app_context_t *context,
                                   const app_command_t *command,
                                   app_result_t *result) {
    const char *file_path = command->args.import_file.path;
    const char *base_name;
    const char *separator;
    image_t *image;
    image_feature_t feature;
    image_record_t record;
    char destination[MAX_PATH_LEN];
    uint64_t hash;
    size_t pixel_bytes;
    size_t dir_length;
    long file_size;
    int id;
    int length;

    if (!file_path)
        return APP_STATUS_INVALID_ARGUMENT;
    image = read_image_file(file_path);
    if (!image)
        return APP_STATUS_IMAGE_FILE_READ_FAILED;

    pixel_bytes = (size_t)image->width * (size_t)image->height *
                  (size_t)image->channels;
    hash = hash_pixels(image->data, pixel_bytes);
    if (image_is_duplicate(context->data_dir, hash)) {
        image_destroy(image);
        return APP_STATUS_DUPLICATE_IMAGE;
    }
    if (feature_extract_rgb_hist(image, 0, &feature) != 0) {
        image_destroy(image);
        return APP_STATUS_FEATURE_EXTRACT_FAILED;
    }

    base_name = strrchr(file_path, '/');
    base_name = base_name ? base_name + 1 : file_path;
    if (*base_name == '\0' || strlen(base_name) >= MAX_NAME_LEN) {
        image_destroy(image);
        return APP_STATUS_INVALID_FILENAME;
    }

    id = db_next_id(context->data_dir);
    if (id < 0) {
        image_destroy(image);
        return APP_STATUS_ID_ALLOCATION_FAILED;
    }
    feature.image_id = id;

    dir_length = strlen(context->data_dir);
    separator = dir_length > 0 && context->data_dir[dir_length - 1] == '/'
                    ? "" : "/";
    length = snprintf(destination, sizeof(destination), "%s%simages/%d%s",
                      context->data_dir, separator, id,
                      stored_extension(file_path));
    if (length < 0 || (size_t)length >= sizeof(destination)) {
        image_destroy(image);
        return APP_STATUS_STORE_PATH_TOO_LONG;
    }
    if (copy_file(file_path, destination) != 0) {
        image_destroy(image);
        return APP_STATUS_IMAGE_COPY_FAILED;
    }

    file_size = get_file_size(file_path);
    if (file_size < 0)
        file_size = 0;
    memset(&record, 0, sizeof(record));
    record.id = id;
    memcpy(record.name, base_name, strlen(base_name) + 1);
    memcpy(record.path, destination, strlen(destination) + 1);
    record.width = image->width;
    record.height = image->height;
    record.channels = image->channels;
    record.file_size = file_size;
    record.import_time = (long)time(NULL);
    record.content_hash = hash;

    if (db_commit_import(context->data_dir, &record, &feature) != 0) {
        unlink(destination);
        image_destroy(image);
        return APP_STATUS_IMPORT_COMMIT_FAILED;
    }

    image_destroy(image);
    result->data.record = record;
    return APP_STATUS_OK;
}

static app_status_t execute_record_lookup(const app_context_t *context,
                                          int id,
                                          app_result_t *result) {
    if (db_find_record_by_id(context->data_dir, id,
                             &result->data.record) != 0) {
        result->detail_value = id;
        return APP_STATUS_RECORD_NOT_FOUND;
    }
    return APP_STATUS_OK;
}

static app_status_t execute_delete(const app_context_t *context, int id,
                                   app_result_t *result) {
    app_status_t status = execute_record_lookup(context, id, result);
    if (status != APP_STATUS_OK)
        return status;
    if (db_mark_deleted(context->data_dir, id) != 0)
        return APP_STATUS_DELETE_FAILED;
    return APP_STATUS_OK;
}

static app_status_t load_records(const app_context_t *context,
                                 app_result_t *result) {
    if (db_load_records(context->data_dir, &result->data.records.items,
                        &result->data.records.count) != 0)
        return APP_STATUS_RECORDS_LOAD_FAILED;
    return APP_STATUS_OK;
}

static app_status_t execute_image_process(const app_context_t *context,
                                          const app_command_t *command,
                                          app_result_t *result) {
    image_record_t record;
    image_t *source;
    image_t *output = NULL;
    int id;

    if (command->kind == APP_COMMAND_BINARY)
        id = command->args.binary.id;
    else if (command->kind == APP_COMMAND_RESIZE ||
             command->kind == APP_COMMAND_RESIZE_BILINEAR)
        id = command->args.resize.id;
    else if (command->kind == APP_COMMAND_ROTATE)
        id = command->args.rotate.id;
    else if (command->kind == APP_COMMAND_MEDIAN)
        id = command->args.median.id;
    else if (command->kind == APP_COMMAND_ADJUST)
        id = command->args.adjust.id;
    else
        id = command->args.image_output.id;

    if (command->kind == APP_COMMAND_MEDIAN &&
        command->args.median.kernel_size != 3 &&
        command->args.median.kernel_size != 5) {
        result->detail_value = command->args.median.kernel_size;
        return APP_STATUS_INVALID_KERNEL_SIZE;
    }
    if (command->kind == APP_COMMAND_RESIZE_BILINEAR &&
        (command->args.resize.width <= 0 || command->args.resize.height <= 0))
        return APP_STATUS_INVALID_DIMENSIONS;

    if (db_find_record_by_id(context->data_dir, id, &record) != 0) {
        result->detail_value = id;
        return APP_STATUS_RECORD_NOT_FOUND;
    }
    if (command->kind == APP_COMMAND_BINARY &&
        (command->args.binary.threshold < 0 ||
         command->args.binary.threshold > 255)) {
        result->detail_value = command->args.binary.threshold;
        return APP_STATUS_INVALID_ARGUMENT;
    }
    if (command->kind == APP_COMMAND_RESIZE &&
        (command->args.resize.width <= 0 || command->args.resize.height <= 0))
        return APP_STATUS_INVALID_DIMENSIONS;
    if (command->kind == APP_COMMAND_ROTATE &&
        command->args.rotate.degrees != 90 &&
        command->args.rotate.degrees != 180 &&
        command->args.rotate.degrees != 270) {
        result->detail_value = command->args.rotate.degrees;
        return APP_STATUS_INVALID_ROTATION;
    }

    source = read_image_file(record.path);
    if (!source) {
        set_detail_path(result, record.path);
        return APP_STATUS_STORED_IMAGE_READ_FAILED;
    }

    switch (command->kind) {
        case APP_COMMAND_GRAY: output = process_gray(source); break;
        case APP_COMMAND_BINARY:
            output = process_binary(source, command->args.binary.threshold);
            break;
        case APP_COMMAND_BLUR: output = process_blur3x3(source); break;
        case APP_COMMAND_EDGE: output = process_sobel_edge(source); break;
        case APP_COMMAND_RESIZE:
            output = process_resize_nearest(source, command->args.resize.width,
                                            command->args.resize.height);
            break;
        case APP_COMMAND_ROTATE:
            output = process_rotate(source, command->args.rotate.degrees);
            break;
        case APP_COMMAND_EQUALIZE: output = process_equalize(source); break;
        case APP_COMMAND_MEDIAN:
            output = process_median(source, command->args.median.kernel_size);
            break;
        case APP_COMMAND_GAUSSIAN: output = process_gaussian(source); break;
        case APP_COMMAND_ADJUST:
            output = process_adjust(source, command->args.adjust.brightness,
                                    command->args.adjust.contrast);
            break;
        case APP_COMMAND_RESIZE_BILINEAR:
            output = process_resize_bilinear(source,
                                             command->args.resize.width,
                                             command->args.resize.height);
            break;
        default: break;
    }
    image_destroy(source);
    if (!output)
        return APP_STATUS_PROCESS_FAILED;

    result->data.image.source_record = record;
    result->data.image.image = output;
    result->data.image.image_count = 1;
    return APP_STATUS_OK;
}

static app_status_t execute_histogram(const app_context_t *context, int id,
                                      app_result_t *result) {
    if (db_find_record_by_id(context->data_dir, id,
                             &result->data.histogram.record) != 0) {
        result->detail_value = id;
        return APP_STATUS_RECORD_NOT_FOUND;
    }
    if (db_find_feature_by_id(context->data_dir, id,
                              &result->data.histogram.feature) != 0) {
        result->detail_value = id;
        return APP_STATUS_FEATURE_NOT_FOUND;
    }
    return APP_STATUS_OK;
}

static app_status_t execute_search(const app_context_t *context,
                                   const app_command_t *command,
                                   app_result_t *result) {
    if (command->args.search.top_k <= 0) {
        result->detail_value = command->args.search.top_k;
        return APP_STATUS_INVALID_ARGUMENT;
    }
    if (search_similar(context->data_dir, command->args.search.id,
                       command->args.search.top_k, command->args.search.metric,
                       &result->data.search.items,
                       &result->data.search.count) != 0) {
        result->detail_value = command->args.search.id;
        return APP_STATUS_SEARCH_FAILED;
    }
    return APP_STATUS_OK;
}

static app_status_t execute_similarity(const app_context_t *context,
                                       const app_command_t *command,
                                       app_result_t *result) {
    int status;

    if (command->args.search_similar.top_k <= 0) {
        result->detail_value = command->args.search_similar.top_k;
        return APP_STATUS_INVALID_ARGUMENT;
    }
    if (access(command->args.search_similar.query_path, R_OK) != 0)
        return APP_STATUS_QUERY_FILE_UNREADABLE;

    status = similarity_search_ppm(context->data_dir,
                   command->args.search_similar.query_path,
                   command->args.search_similar.top_k,
                   &result->data.similarity.items,
                   &result->data.similarity.count);
    result->auxiliary_status = status;
    switch (status) {
        case SIMILARITY_OK: return APP_STATUS_OK;
        case SIMILARITY_ERR_BAD_QUERY: return APP_STATUS_INVALID_QUERY_IMAGE;
        case SIMILARITY_ERR_EMPTY_DB: return APP_STATUS_EMPTY_STORE;
        case SIMILARITY_ERR_NOMEM: return APP_STATUS_OUT_OF_MEMORY;
        case SIMILARITY_ERR_INVALID_TOPK:
            result->detail_value = command->args.search_similar.top_k;
            return APP_STATUS_INVALID_ARGUMENT;
        default: return APP_STATUS_SIMILARITY_FAILED;
    }
}

static char *lowercase_copy(char *destination, const char *source, size_t size) {
    size_t i;
    for (i = 0; i + 1 < size && source[i]; i++) {
        unsigned char ch = (unsigned char)source[i];
        destination[i] = (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
    }
    destination[i] = '\0';
    return destination;
}

static app_status_t filter_by_name(const app_context_t *context,
                                   const char *keyword,
                                   app_result_t *result) {
    image_record_t *records;
    char keyword_lower[MAX_NAME_LEN];
    char name_lower[MAX_NAME_LEN];
    int count;
    int match_count = 0;
    int i;

    if (!keyword || !*keyword)
        return APP_STATUS_KEYWORD_EMPTY;
    if (db_load_records(context->data_dir, &records, &count) != 0)
        return APP_STATUS_RECORDS_LOAD_FAILED;
    lowercase_copy(keyword_lower, keyword, sizeof(keyword_lower));
    for (i = 0; i < count; i++) {
        if (records[i].deleted)
            continue;
        lowercase_copy(name_lower, records[i].name, sizeof(name_lower));
        if (strstr(name_lower, keyword_lower))
            records[match_count++] = records[i];
    }
    result->data.records.items = records;
    result->data.records.count = match_count;
    return APP_STATUS_OK;
}

static long numeric_field(const image_record_t *record,
                          app_query_field_t field) {
    switch (field) {
        case APP_QUERY_ID: return record->id;
        case APP_QUERY_WIDTH: return record->width;
        case APP_QUERY_HEIGHT: return record->height;
        case APP_QUERY_SIZE: return record->file_size;
        default: return 0;
    }
}

static int numeric_matches(long record_value, app_query_operator_t op,
                           long query_value) {
    switch (op) {
        case APP_QUERY_EQ: return record_value == query_value;
        case APP_QUERY_NE: return record_value != query_value;
        case APP_QUERY_GT: return record_value > query_value;
        case APP_QUERY_GE: return record_value >= query_value;
        case APP_QUERY_LT: return record_value < query_value;
        case APP_QUERY_LE: return record_value <= query_value;
        default: return 0;
    }
}

static int record_matches(const image_record_t *record,
                          const app_command_t *command) {
    app_query_field_t field = command->args.query.field;
    app_query_operator_t op = command->args.query.op;
    const char *record_text;

    if (field != APP_QUERY_NAME && field != APP_QUERY_FORMAT)
        return numeric_matches(numeric_field(record, field), op,
                               command->args.query.numeric_value);
    record_text = field == APP_QUERY_NAME ? record->name : record_format(record);
    if (op == APP_QUERY_EQ)
        return strcmp(record_text, command->args.query.text_value) == 0;
    if (op == APP_QUERY_NE)
        return strcmp(record_text, command->args.query.text_value) != 0;
    return op == APP_QUERY_CONTAINS &&
           strstr(record_text, command->args.query.text_value) != NULL;
}

static int query_is_valid(const app_command_t *command) {
    app_query_field_t field = command->args.query.field;
    app_query_operator_t op = command->args.query.op;

    if (field < APP_QUERY_ID || field > APP_QUERY_SIZE ||
        op < APP_QUERY_EQ || op > APP_QUERY_CONTAINS)
        return 0;
    if (field == APP_QUERY_NAME) {
        return command->args.query.text_value &&
               (op == APP_QUERY_EQ || op == APP_QUERY_NE ||
                op == APP_QUERY_CONTAINS);
    }
    if (field == APP_QUERY_FORMAT) {
        return command->args.query.text_value &&
               (op == APP_QUERY_EQ || op == APP_QUERY_NE);
    }
    return op >= APP_QUERY_EQ && op <= APP_QUERY_LE;
}

static app_status_t execute_query(const app_context_t *context,
                                  const app_command_t *command,
                                  app_result_t *result) {
    image_record_t *records;
    int count;
    int match_count = 0;
    int i;

    if (!query_is_valid(command))
        return APP_STATUS_INVALID_ARGUMENT;
    if (db_load_records(context->data_dir, &records, &count) != 0)
        return APP_STATUS_RECORDS_LOAD_FAILED;
    for (i = 0; i < count; i++) {
        if (!records[i].deleted && record_matches(&records[i], command))
            records[match_count++] = records[i];
    }
    result->data.records.items = records;
    result->data.records.count = match_count;
    return APP_STATUS_OK;
}

static app_status_t execute_stats(const app_context_t *context,
                                  app_result_t *result) {
    image_record_t *records;
    image_feature_t *features;
    int record_count;
    int feature_count;
    double width_sum = 0.0;
    double height_sum = 0.0;
    int i;

    if (db_load_records(context->data_dir, &records, &record_count) != 0)
        return APP_STATUS_RECORDS_LOAD_FAILED;
    if (db_load_features(context->data_dir, &features, &feature_count) != 0) {
        free(records);
        return APP_STATUS_FEATURES_LOAD_FAILED;
    }

    result->data.stats.total_records = record_count;
    result->data.stats.feature_records = feature_count;
    for (i = 0; i < record_count; i++) {
        if (records[i].deleted) {
            result->data.stats.deleted_records++;
            continue;
        }
        result->data.stats.active_records++;
        result->data.stats.total_image_size += records[i].file_size;
        width_sum += records[i].width;
        height_sum += records[i].height;
        if (strcmp(record_format(&records[i]), "BMP") == 0)
            result->data.stats.bmp_count++;
        else
            result->data.stats.ppm_count++;
    }
    if (result->data.stats.active_records > 0) {
        result->data.stats.average_width =
            width_sum / result->data.stats.active_records;
        result->data.stats.average_height =
            height_sum / result->data.stats.active_records;
    }
    free(records);
    free(features);
    return APP_STATUS_OK;
}

static app_status_t execute_compact(const app_context_t *context,
                                    app_result_t *result) {
    if (db_compact(context->data_dir,
                   &result->data.compact.before_count,
                   &result->data.compact.after_count) != 0)
        return APP_STATUS_COMPACT_FAILED;
    return APP_STATUS_OK;
}

static app_status_t execute_histogram_image(const app_context_t *context,
                                            int id,
                                            app_result_t *result) {
    image_feature_t feature;
    image_record_t record;

    if (db_find_record_by_id(context->data_dir, id, &record) != 0) {
        result->detail_value = id;
        return APP_STATUS_RECORD_NOT_FOUND;
    }
    if (db_find_feature_by_id(context->data_dir, id, &feature) != 0) {
        result->detail_value = id;
        return APP_STATUS_FEATURE_NOT_FOUND;
    }
    result->data.image.image = visualize_hist_image(&feature);
    if (!result->data.image.image)
        return APP_STATUS_HISTOGRAM_IMAGE_FAILED;
    result->data.image.source_record = record;
    result->data.image.image_count = 1;
    return APP_STATUS_OK;
}

static app_status_t execute_search_export(const app_context_t *context,
                                          const app_command_t *command,
                                          app_result_t *result) {
    search_result_t *search_results;
    app_search_export_item_t *items;
    image_record_t record;
    int count;
    int i;

    if (command->args.search_output.top_k <= 0) {
        result->detail_value = command->args.search_output.top_k;
        return APP_STATUS_INVALID_ARGUMENT;
    }
    if (search_similar(context->data_dir, command->args.search_output.id,
                       command->args.search_output.top_k,
                       command->args.search_output.metric,
                       &search_results, &count) != 0) {
        result->detail_value = command->args.search_output.id;
        return APP_STATUS_SEARCH_FAILED;
    }
    items = count > 0 ? calloc((size_t)count, sizeof(*items)) : NULL;
    if (count > 0 && !items) {
        free(search_results);
        return APP_STATUS_OUT_OF_MEMORY;
    }
    for (i = 0; i < count; i++) {
        items[i].result = search_results[i];
        if (db_find_record_by_id(context->data_dir,
                                 search_results[i].image_id, &record) == 0)
            snprintf(items[i].path, sizeof(items[i].path), "%s", record.path);
    }
    free(search_results);
    result->data.search_export.items = items;
    result->data.search_export.count = count;
    return APP_STATUS_OK;
}

static app_status_t execute_contact_sheet(const app_context_t *context,
                                          const app_command_t *command,
                                          app_result_t *result) {
    search_result_t *search_results = NULL;
    image_record_t query_record;
    image_t *query_image = NULL;
    image_t **thumbnails = NULL;
    image_t *sheet = NULL;
    int count = 0;
    int i;
    app_status_t status = APP_STATUS_CONTACT_SHEET_FAILED;

    if (command->args.search_output.top_k <= 0) {
        result->detail_value = command->args.search_output.top_k;
        return APP_STATUS_INVALID_ARGUMENT;
    }
    if (command->args.search_output.top_k > 100) {
        result->detail_value = command->args.search_output.top_k;
        return APP_STATUS_TOP_K_TOO_LARGE;
    }
    if (search_similar(context->data_dir, command->args.search_output.id,
                       command->args.search_output.top_k,
                       command->args.search_output.metric,
                       &search_results, &count) != 0) {
        result->detail_value = command->args.search_output.id;
        return APP_STATUS_SEARCH_FAILED;
    }
    if (db_find_record_by_id(context->data_dir,
                             command->args.search_output.id,
                             &query_record) != 0) {
        result->detail_value = command->args.search_output.id;
        status = APP_STATUS_RECORD_NOT_FOUND;
        goto cleanup;
    }
    query_image = read_image_file(query_record.path);
    if (!query_image) {
        set_detail_path(result, query_record.path);
        status = APP_STATUS_STORED_IMAGE_READ_FAILED;
        goto cleanup;
    }

    thumbnails = calloc((size_t)(1 + count), sizeof(*thumbnails));
    if (!thumbnails)
        goto cleanup;
    thumbnails[0] = process_resize_nearest(query_image, 128, 128);
    if (!thumbnails[0])
        goto cleanup;

    for (i = 0; i < count; i++) {
        image_record_t record;
        image_t *source;
        if (db_find_record_by_id(context->data_dir,
                                 search_results[i].image_id, &record) != 0)
            goto cleanup;
        source = read_image_file(record.path);
        if (!source)
            goto cleanup;
        thumbnails[i + 1] = process_resize_nearest(source, 128, 128);
        image_destroy(source);
        if (!thumbnails[i + 1])
            goto cleanup;
    }
    sheet = visualize_contact_sheet(thumbnails, 1 + count, 128, 128);
    if (!sheet)
        goto cleanup;

    result->data.image.source_record = query_record;
    result->data.image.image = sheet;
    result->data.image.image_count = 1 + count;
    sheet = NULL;
    status = APP_STATUS_OK;

cleanup:
    image_destroy(query_image);
    if (thumbnails) {
        for (i = 0; i <= count; i++)
            image_destroy(thumbnails[i]);
        free(thumbnails);
    }
    image_destroy(sheet);
    free(search_results);
    return status;
}

app_status_t app_execute(const app_context_t *context,
                         const app_command_t *command,
                         app_result_t *result) {
    report_status_t report_status;

    if (!result)
        return APP_STATUS_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if (!context || !context->data_dir || !*context->data_dir || !command)
        return APP_STATUS_INVALID_ARGUMENT;
    result->kind = command->kind;

    switch (command->kind) {
        case APP_COMMAND_INIT:
            return db_init(context->data_dir) == 0
                       ? APP_STATUS_OK : APP_STATUS_STORE_INIT_FAILED;
        case APP_COMMAND_IMPORT:
            return execute_import(context, command, result);
        case APP_COMMAND_LIST:
        case APP_COMMAND_EXPORT:
            return load_records(context, result);
        case APP_COMMAND_INFO:
            return execute_record_lookup(context, command->args.id.id, result);
        case APP_COMMAND_DELETE:
            return execute_delete(context, command->args.id.id, result);
        case APP_COMMAND_GRAY:
        case APP_COMMAND_BINARY:
        case APP_COMMAND_BLUR:
        case APP_COMMAND_EDGE:
        case APP_COMMAND_RESIZE:
        case APP_COMMAND_ROTATE:
        case APP_COMMAND_EQUALIZE:
        case APP_COMMAND_MEDIAN:
        case APP_COMMAND_GAUSSIAN:
        case APP_COMMAND_ADJUST:
        case APP_COMMAND_RESIZE_BILINEAR:
            return execute_image_process(context, command, result);
        case APP_COMMAND_HIST:
            return execute_histogram(context, command->args.id.id, result);
        case APP_COMMAND_SEARCH:
            return execute_search(context, command, result);
        case APP_COMMAND_SEARCH_SIMILAR:
            return execute_similarity(context, command, result);
        case APP_COMMAND_FIND_NAME:
            return filter_by_name(context, command->args.find_name.keyword,
                                  result);
        case APP_COMMAND_QUERY:
            return execute_query(context, command, result);
        case APP_COMMAND_STATS:
            return execute_stats(context, result);
        case APP_COMMAND_COMPACT:
            return execute_compact(context, result);
        case APP_COMMAND_REPORT:
            report_status = generate_html_report_status(
                command->args.report.output_dir,
                command->args.report.report_path);
            result->auxiliary_status = (int)report_status;
            return report_status == REPORT_STATUS_OK
                       ? APP_STATUS_OK : APP_STATUS_REPORT_FAILED;
        case APP_COMMAND_VERIFY:
            if (verify_database(context->data_dir, &result->data.verify) != 0)
                return APP_STATUS_VERIFY_FAILED;
            return APP_STATUS_OK;
        case APP_COMMAND_REPAIR:
            if (repair_database(context->data_dir, &result->data.repair) != 0)
                return APP_STATUS_REPAIR_FAILED;
            return APP_STATUS_OK;
        case APP_COMMAND_HIST_EXPORT:
            return execute_histogram(context, command->args.hist_export.id,
                                     result);
        case APP_COMMAND_HIST_IMAGE:
            return execute_histogram_image(context,
                                            command->args.image_output.id,
                                            result);
        case APP_COMMAND_SEARCH_EXPORT:
            return execute_search_export(context, command, result);
        case APP_COMMAND_SEARCH_CONTACT:
            return execute_contact_sheet(context, command, result);
        case APP_COMMAND_HELP:
            return APP_STATUS_INVALID_ARGUMENT;
    }
    return APP_STATUS_INVALID_ARGUMENT;
}

void app_result_destroy(app_result_t *result) {
    if (!result)
        return;
    switch (result->kind) {
        case APP_COMMAND_LIST:
        case APP_COMMAND_FIND_NAME:
        case APP_COMMAND_QUERY:
        case APP_COMMAND_EXPORT:
            free(result->data.records.items);
            break;
        case APP_COMMAND_GRAY:
        case APP_COMMAND_BINARY:
        case APP_COMMAND_BLUR:
        case APP_COMMAND_EDGE:
        case APP_COMMAND_RESIZE:
        case APP_COMMAND_ROTATE:
        case APP_COMMAND_EQUALIZE:
        case APP_COMMAND_MEDIAN:
        case APP_COMMAND_GAUSSIAN:
        case APP_COMMAND_ADJUST:
        case APP_COMMAND_RESIZE_BILINEAR:
        case APP_COMMAND_HIST_IMAGE:
        case APP_COMMAND_SEARCH_CONTACT:
            image_destroy(result->data.image.image);
            break;
        case APP_COMMAND_SEARCH:
            free(result->data.search.items);
            break;
        case APP_COMMAND_SEARCH_SIMILAR:
            free(result->data.similarity.items);
            break;
        case APP_COMMAND_SEARCH_EXPORT:
            free(result->data.search_export.items);
            break;
        default:
            break;
    }
    memset(result, 0, sizeof(*result));
}
