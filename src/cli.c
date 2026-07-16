#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "app.h"
#include "cli.h"
#include "cli_output.h"
#include "cli_parse.h"
#include "report.h"

#define DATA_DIR "data"

static void format_time(long value, char *buffer, size_t buffer_size) {
    time_t timestamp = (time_t)value;
    struct tm *time_info;

    if (!buffer || buffer_size == 0)
        return;
    time_info = localtime(&timestamp);
    if (!time_info ||
        strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", time_info) == 0)
        snprintf(buffer, buffer_size, "unknown");
}

static const char *record_format(const image_record_t *record) {
    const char *extension = strrchr(record->path, '.');
    if (!extension)
        return "unknown";
    return strcasecmp(extension, ".bmp") == 0 ? "BMP" : "PPM";
}

static const char *command_name(app_command_kind_t kind) {
    switch (kind) {
        case APP_COMMAND_HELP: return "help";
        case APP_COMMAND_INIT: return "init";
        case APP_COMMAND_IMPORT: return "import";
        case APP_COMMAND_LIST: return "list";
        case APP_COMMAND_INFO: return "info";
        case APP_COMMAND_GRAY: return "gray";
        case APP_COMMAND_BINARY: return "binary";
        case APP_COMMAND_BLUR: return "blur";
        case APP_COMMAND_EDGE: return "edge";
        case APP_COMMAND_HIST: return "hist";
        case APP_COMMAND_SEARCH: return "search";
        case APP_COMMAND_SEARCH_SIMILAR: return "search-similar";
        case APP_COMMAND_RESIZE: return "resize";
        case APP_COMMAND_ROTATE: return "rotate";
        case APP_COMMAND_DELETE: return "delete";
        case APP_COMMAND_EQUALIZE: return "equalize";
        case APP_COMMAND_MEDIAN: return "median";
        case APP_COMMAND_GAUSSIAN: return "gaussian";
        case APP_COMMAND_ADJUST: return "adjust";
        case APP_COMMAND_RESIZE_BILINEAR: return "resize-bilinear";
        case APP_COMMAND_FIND_NAME: return "find-name";
        case APP_COMMAND_QUERY: return "query";
        case APP_COMMAND_STATS: return "stats";
        case APP_COMMAND_COMPACT: return "compact";
        case APP_COMMAND_EXPORT: return "export";
        case APP_COMMAND_REPORT: return "report";
        case APP_COMMAND_VERIFY: return "verify";
        case APP_COMMAND_REPAIR: return "repair";
        case APP_COMMAND_HIST_EXPORT: return "hist-export";
        case APP_COMMAND_HIST_IMAGE: return "hist-image";
        case APP_COMMAND_SEARCH_EXPORT: return "search-export";
        case APP_COMMAND_SEARCH_CONTACT: return "search-contact";
    }
    return "";
}

static void print_usage_error(app_command_kind_t kind) {
    switch (kind) {
        case APP_COMMAND_IMPORT:
            fprintf(stderr, "[ERROR] Usage: ./imagedb import <file>\n");
            break;
        case APP_COMMAND_INFO:
            fprintf(stderr, "[ERROR] Usage: ./imagedb info <id>\n");
            break;
        case APP_COMMAND_GRAY:
            fprintf(stderr, "[ERROR] Usage: ./imagedb gray <id> <out>\n");
            break;
        case APP_COMMAND_BINARY:
            fprintf(stderr,
                    "[ERROR] Usage: ./imagedb binary <id> <threshold> <out>\n");
            break;
        case APP_COMMAND_BLUR:
            fprintf(stderr, "[ERROR] Usage: ./imagedb blur <id> <out>\n");
            break;
        case APP_COMMAND_EDGE:
            fprintf(stderr, "[ERROR] Usage: ./imagedb edge <id> <out>\n");
            break;
        case APP_COMMAND_HIST:
            fprintf(stderr, "[ERROR] Usage: ./imagedb hist <id>\n");
            break;
        case APP_COMMAND_SEARCH:
            fprintf(stderr, "[ERROR] Usage: ./imagedb search <id> <k> "
                            "[--metric l1|l2|intersection]\n");
            break;
        case APP_COMMAND_SEARCH_SIMILAR:
            fprintf(stderr, "[ERROR] Usage: ./cimagedb search-similar "
                            "<query.ppm> --topk K\n");
            break;
        case APP_COMMAND_RESIZE:
            fprintf(stderr, "[ERROR] Usage: ./imagedb resize <id> <new_width> "
                            "<new_height> <output>\n");
            break;
        case APP_COMMAND_ROTATE:
            fprintf(stderr, "[ERROR] Usage: ./imagedb rotate <id> "
                            "<90|180|270> <output>\n");
            break;
        case APP_COMMAND_DELETE:
            fprintf(stderr, "[ERROR] Usage: ./imagedb delete <id>\n");
            break;
        case APP_COMMAND_EQUALIZE:
            fprintf(stderr, "[ERROR] Usage: ./imagedb equalize <id> <output>\n");
            break;
        case APP_COMMAND_MEDIAN:
            fprintf(stderr, "[ERROR] Usage: ./imagedb median <id> "
                            "<kernel_size> <output>\n");
            break;
        case APP_COMMAND_GAUSSIAN:
            fprintf(stderr, "[ERROR] Usage: ./imagedb gaussian <id> <output>\n");
            break;
        case APP_COMMAND_ADJUST:
            fprintf(stderr, "[ERROR] Usage: ./imagedb adjust <id> <brightness> "
                            "<contrast> <output>\n");
            break;
        case APP_COMMAND_RESIZE_BILINEAR:
            fprintf(stderr, "[ERROR] Usage: ./imagedb resize-bilinear <id> "
                            "<new_w> <new_h> <output>\n");
            break;
        case APP_COMMAND_FIND_NAME:
            fprintf(stderr, "[ERROR] Usage: ./imagedb find-name <keyword>\n");
            break;
        case APP_COMMAND_QUERY:
            fprintf(stderr,
                    "[ERROR] Usage: ./imagedb query <field> <op> <value>\n");
            break;
        case APP_COMMAND_EXPORT:
            fprintf(stderr, "[ERROR] Usage: ./imagedb export <output.csv>\n");
            break;
        case APP_COMMAND_REPORT:
            fprintf(stderr, "[ERROR] Usage: ./imagedb report <output_dir> "
                            "<report.html>\n");
            break;
        case APP_COMMAND_HIST_EXPORT:
            fprintf(stderr, "[ERROR] Usage: ./imagedb hist-export <id> "
                            "<output.csv> [--normalized]\n");
            break;
        case APP_COMMAND_HIST_IMAGE:
            fprintf(stderr, "[ERROR] Usage: ./imagedb hist-image <id> <output>\n");
            break;
        case APP_COMMAND_SEARCH_EXPORT:
            fprintf(stderr, "[ERROR] Usage: ./imagedb search-export <id> <k> "
                            "<output.csv> [--metric l1|l2|intersection]\n");
            break;
        case APP_COMMAND_SEARCH_CONTACT:
            fprintf(stderr, "[ERROR] Usage: ./imagedb search-contact <id> <k> "
                            "<output> [--metric l1|l2|intersection]\n");
            break;
        default:
            break;
    }
}

static void print_parse_error(const cli_parse_error_t *error) {
    switch (error->code) {
        case CLI_PARSE_ERROR_USAGE:
            print_usage_error(error->command);
            break;
        case CLI_PARSE_ERROR_TAKES_NO_ARGUMENTS:
            fprintf(stderr, "[ERROR] %s takes no arguments\n",
                    command_name(error->command));
            break;
        case CLI_PARSE_ERROR_INVALID_POSITIVE:
            fprintf(stderr, "[ERROR] Invalid %s: %s\n",
                    error->label, error->argument);
            break;
        case CLI_PARSE_ERROR_INVALID_THRESHOLD:
            fprintf(stderr, "[ERROR] Invalid threshold: %s\n", error->argument);
            break;
        case CLI_PARSE_ERROR_INVALID_ANGLE:
            fprintf(stderr, "[ERROR] Invalid angle: %s "
                            "(use 90, 180, or 270)\n", error->argument);
            break;
        case CLI_PARSE_ERROR_INVALID_BRIGHTNESS:
            fprintf(stderr, "[ERROR] Invalid brightness: %s\n", error->argument);
            break;
        case CLI_PARSE_ERROR_INVALID_CONTRAST:
            fprintf(stderr, "[ERROR] Invalid contrast: %s "
                            "(must be 0 < x <= 10)\n", error->argument);
            break;
        case CLI_PARSE_ERROR_UNKNOWN_METRIC_WITH_HINT:
            fprintf(stderr, "[ERROR] Unknown metric: %s "
                            "(use l1, l2, or intersection)\n", error->argument);
            break;
        case CLI_PARSE_ERROR_UNKNOWN_METRIC:
            fprintf(stderr, "[ERROR] Unknown metric: %s\n", error->argument);
            break;
        case CLI_PARSE_ERROR_UNKNOWN_OPTION:
            fprintf(stderr, "[ERROR] Unknown option: %s\n", error->argument);
            break;
        case CLI_PARSE_ERROR_UNKNOWN_COMMAND:
            fprintf(stderr, "[ERROR] Unknown command: %s\n", error->argument);
            break;
        case CLI_PARSE_ERROR_UNKNOWN_FIELD:
            fprintf(stderr, "[ERROR] Unknown field: %s "
                            "(use id, name, width, height, format, size)\n",
                    error->argument);
            break;
        case CLI_PARSE_ERROR_UNKNOWN_OPERATOR:
            fprintf(stderr, "[ERROR] Unknown operator: %s\n", error->argument);
            break;
        case CLI_PARSE_ERROR_INVALID_FIELD_OPERATOR:
            fprintf(stderr, "[ERROR] Operator '%s' not valid for field '%s'\n",
                    error->argument, error->secondary);
            break;
        case CLI_PARSE_ERROR_INVALID_NUMERIC_VALUE:
            fprintf(stderr, "[ERROR] Invalid numeric value for field '%s': %s\n",
                    error->secondary, error->argument);
            break;
    }
}

static int command_id(const app_command_t *command) {
    switch (command->kind) {
        case APP_COMMAND_INFO:
        case APP_COMMAND_DELETE:
        case APP_COMMAND_HIST:
            return command->args.id.id;
        case APP_COMMAND_BINARY: return command->args.binary.id;
        case APP_COMMAND_SEARCH: return command->args.search.id;
        case APP_COMMAND_RESIZE:
        case APP_COMMAND_RESIZE_BILINEAR:
            return command->args.resize.id;
        case APP_COMMAND_ROTATE: return command->args.rotate.id;
        case APP_COMMAND_MEDIAN: return command->args.median.id;
        case APP_COMMAND_ADJUST: return command->args.adjust.id;
        case APP_COMMAND_HIST_EXPORT: return command->args.hist_export.id;
        case APP_COMMAND_SEARCH_EXPORT:
        case APP_COMMAND_SEARCH_CONTACT:
            return command->args.search_output.id;
        case APP_COMMAND_GRAY:
        case APP_COMMAND_BLUR:
        case APP_COMMAND_EDGE:
        case APP_COMMAND_EQUALIZE:
        case APP_COMMAND_GAUSSIAN:
        case APP_COMMAND_HIST_IMAGE:
            return command->args.image_output.id;
        default:
            return 0;
    }
}

static const char *command_output_path(const app_command_t *command) {
    switch (command->kind) {
        case APP_COMMAND_BINARY: return command->args.binary.output_path;
        case APP_COMMAND_RESIZE:
        case APP_COMMAND_RESIZE_BILINEAR:
            return command->args.resize.output_path;
        case APP_COMMAND_ROTATE: return command->args.rotate.output_path;
        case APP_COMMAND_MEDIAN: return command->args.median.output_path;
        case APP_COMMAND_ADJUST: return command->args.adjust.output_path;
        case APP_COMMAND_HIST_EXPORT:
            return command->args.hist_export.output_path;
        case APP_COMMAND_SEARCH_EXPORT:
        case APP_COMMAND_SEARCH_CONTACT:
            return command->args.search_output.output_path;
        case APP_COMMAND_EXPORT: return command->args.export_file.output_path;
        default: return command->args.image_output.output_path;
    }
}

static void print_report_error(const app_command_t *command,
                               const app_result_t *result) {
    switch ((report_status_t)result->auxiliary_status) {
        case REPORT_STATUS_OUTPUT_DIR_MISSING:
            fprintf(stderr, "[ERROR] Output directory does not exist: %s\n",
                    command->args.report.output_dir
                        ? command->args.report.output_dir : "(null)");
            break;
        case REPORT_STATUS_PATH_REQUIRED:
            fprintf(stderr, "[ERROR] Report path is required\n");
            break;
        case REPORT_STATUS_PATH_TOO_LONG:
            fprintf(stderr, "[ERROR] Report path is too long\n");
            break;
        case REPORT_STATUS_OPEN_FAILED:
            fprintf(stderr, "[ERROR] Cannot write report: %s\n",
                    command->args.report.report_path);
            break;
        case REPORT_STATUS_FINISH_FAILED:
            fprintf(stderr, "[ERROR] Failed to finish writing report: %s\n",
                    command->args.report.report_path);
            break;
        case REPORT_STATUS_OK:
            break;
    }
}

static void print_app_error(const app_command_t *command,
                            app_status_t status,
                            const app_result_t *result) {
    int id = command_id(command);

    switch (status) {
        case APP_STATUS_INVALID_ARGUMENT:
            if (command->kind == APP_COMMAND_SEARCH)
                fprintf(stderr, "[ERROR] top_k must be positive, got %d\n",
                        command->args.search.top_k);
            else if (command->kind == APP_COMMAND_SEARCH_SIMILAR)
                fprintf(stderr, "[ERROR] topk must be positive, got %d\n",
                        command->args.search_similar.top_k);
            else if (command->kind == APP_COMMAND_BINARY)
                fprintf(stderr, "[ERROR] Threshold must be 0-255, got %d\n",
                        command->args.binary.threshold);
            break;
        case APP_STATUS_STORE_INIT_FAILED:
            fprintf(stderr, "[ERROR] Failed to initialize store\n");
            break;
        case APP_STATUS_IMAGE_FILE_READ_FAILED:
            fprintf(stderr, "[ERROR] Failed to read image file: %s\n",
                    command->args.import_file.path);
            break;
        case APP_STATUS_DUPLICATE_IMAGE:
            fprintf(stderr, "[ERROR] Image already exists (content hash match)\n");
            break;
        case APP_STATUS_FEATURE_EXTRACT_FAILED:
            fprintf(stderr, "[ERROR] Failed to extract feature\n");
            break;
        case APP_STATUS_INVALID_FILENAME:
            fprintf(stderr, "[ERROR] Image filename is empty or exceeds %d bytes\n",
                    MAX_NAME_LEN - 1);
            break;
        case APP_STATUS_ID_ALLOCATION_FAILED:
            fprintf(stderr, "[ERROR] Failed to allocate ID\n");
            break;
        case APP_STATUS_STORE_PATH_TOO_LONG:
            fprintf(stderr, "[ERROR] Store path is too long\n");
            break;
        case APP_STATUS_IMAGE_COPY_FAILED:
            fprintf(stderr, "[ERROR] Failed to copy image to store\n");
            break;
        case APP_STATUS_IMPORT_COMMIT_FAILED:
            fprintf(stderr, "[ERROR] Failed to commit import transaction\n");
            break;
        case APP_STATUS_RECORDS_LOAD_FAILED:
            if (command->kind == APP_COMMAND_STATS)
                fprintf(stderr, "[ERROR] Failed to load records "
                                "(database may be corrupt)\n");
            else
                fprintf(stderr, "[ERROR] Failed to load records\n");
            break;
        case APP_STATUS_RECORD_NOT_FOUND:
            fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
            break;
        case APP_STATUS_DELETE_FAILED:
            fprintf(stderr, "[ERROR] Failed to delete record\n");
            break;
        case APP_STATUS_STORED_IMAGE_READ_FAILED:
            if (command->kind == APP_COMMAND_SEARCH_CONTACT)
                fprintf(stderr, "[ERROR] Failed to read query image: %s\n",
                        result->detail_path);
            else
                fprintf(stderr, "[ERROR] Failed to read image: %s\n",
                        result->detail_path);
            break;
        case APP_STATUS_PROCESS_FAILED:
            if (command->kind == APP_COMMAND_RESIZE)
                fprintf(stderr, "[ERROR] Resize failed\n");
            else if (command->kind == APP_COMMAND_ROTATE)
                fprintf(stderr, "[ERROR] Rotate failed\n");
            else if (command->kind == APP_COMMAND_MEDIAN)
                fprintf(stderr, "[ERROR] Median filter failed\n");
            else if (command->kind == APP_COMMAND_ADJUST)
                fprintf(stderr, "[ERROR] Adjust failed\n");
            else if (command->kind == APP_COMMAND_RESIZE_BILINEAR)
                fprintf(stderr, "[ERROR] Bilinear resize failed\n");
            else
                fprintf(stderr, "[ERROR] Processing failed\n");
            break;
        case APP_STATUS_FEATURE_NOT_FOUND:
            fprintf(stderr, "[ERROR] Feature not found for image %d\n", id);
            break;
        case APP_STATUS_SEARCH_FAILED:
            if (command->kind == APP_COMMAND_SEARCH)
                fprintf(stderr, "[ERROR] Search failed. Check that image %d "
                                "exists and has a feature.\n", id);
            else
                fprintf(stderr, "[ERROR] Search failed for image %d\n", id);
            break;
        case APP_STATUS_QUERY_FILE_UNREADABLE:
            fprintf(stderr, "[ERROR] Query file not found or unreadable: %s\n",
                    command->args.search_similar.query_path);
            break;
        case APP_STATUS_INVALID_QUERY_IMAGE:
            fprintf(stderr, "[ERROR] Invalid PPM query file: %s\n",
                    command->args.search_similar.query_path);
            break;
        case APP_STATUS_EMPTY_STORE:
            fprintf(stderr, "[ERROR] Database is empty\n");
            break;
        case APP_STATUS_OUT_OF_MEMORY:
            if (command->kind == APP_COMMAND_SEARCH_SIMILAR)
                fprintf(stderr, "[ERROR] Out of memory during similarity search\n");
            else if (command->kind == APP_COMMAND_SEARCH_EXPORT)
                fprintf(stderr, "[ERROR] Search failed for image %d\n", id);
            break;
        case APP_STATUS_SIMILARITY_FAILED:
            fprintf(stderr, "[ERROR] Failed to search similar images\n");
            break;
        case APP_STATUS_INVALID_DIMENSIONS:
            fprintf(stderr, "[ERROR] Invalid dimensions: %dx%d\n",
                    command->args.resize.width, command->args.resize.height);
            break;
        case APP_STATUS_INVALID_ROTATION:
            fprintf(stderr, "[ERROR] Rotation must be 90, 180, or 270, got %d\n",
                    command->args.rotate.degrees);
            break;
        case APP_STATUS_INVALID_KERNEL_SIZE:
            fprintf(stderr, "[ERROR] Kernel size must be 3 or 5, got %d\n",
                    command->args.median.kernel_size);
            break;
        case APP_STATUS_KEYWORD_EMPTY:
            fprintf(stderr, "[ERROR] Keyword must not be empty\n");
            break;
        case APP_STATUS_FEATURES_LOAD_FAILED:
            fprintf(stderr, "[ERROR] Failed to load features "
                            "(database may be corrupt)\n");
            break;
        case APP_STATUS_COMPACT_FAILED:
            fprintf(stderr, "[ERROR] Compact failed. Database unchanged.\n");
            break;
        case APP_STATUS_REPORT_FAILED:
            print_report_error(command, result);
            break;
        case APP_STATUS_VERIFY_FAILED:
            fprintf(stderr, "[ERROR] Verify failed\n");
            break;
        case APP_STATUS_REPAIR_FAILED:
            fprintf(stderr, "[ERROR] Repair failed\n");
            break;
        case APP_STATUS_HISTOGRAM_IMAGE_FAILED:
            fprintf(stderr, "[ERROR] Failed to generate histogram image\n");
            break;
        case APP_STATUS_TOP_K_TOO_LARGE:
            fprintf(stderr, "[ERROR] top_k too large (max 100): %d\n",
                    command->args.search_output.top_k);
            break;
        case APP_STATUS_CONTACT_SHEET_FAILED:
        case APP_STATUS_OK:
            break;
    }
}

static void print_feature(const image_feature_t *feature) {
    int channel;
    int base;
    int offset;
    const int *histograms[] = {
        feature->r_hist, feature->g_hist, feature->b_hist
    };
    const char labels[] = {'R', 'G', 'B'};

    printf("Feature for image %d:\n", feature->image_id);
    printf("  Average R: %.2f\n", feature->avg_r);
    printf("  Average G: %.2f\n", feature->avg_g);
    printf("  Average B: %.2f\n", feature->avg_b);
    for (channel = 0; channel < 3; channel++) {
        printf("%c Histogram:\n", labels[channel]);
        for (base = 0; base < 256; base += 16) {
            printf("  ");
            for (offset = 0; offset < 16; offset++)
                printf("%3d:%-6d ", base + offset,
                       histograms[channel][base + offset]);
            printf("\n");
        }
    }
}

static void print_list(const app_record_list_t *records) {
    char time_buffer[64];
    int i;

    if (records->count == 0) {
        printf("No images.\n");
        return;
    }
    printf("ID  %-20s  %-10s  Import time\n", "Name", "Size");
    printf("--- -------------------- ---------- -------------------\n");
    for (i = 0; i < records->count; i++) {
        if (records->items[i].deleted)
            continue;
        format_time(records->items[i].import_time,
                    time_buffer, sizeof(time_buffer));
        printf("%-4d %-20s %4dx%-4d %s\n", records->items[i].id,
               records->items[i].name, records->items[i].width,
               records->items[i].height, time_buffer);
    }
}

static void print_find_results(const app_record_list_t *records) {
    int i;
    if (records->count == 0) {
        printf("No matched records.\n");
        return;
    }
    printf("ID  %-20s  %-10s  Format  Path\n", "Name", "Size");
    printf("--- -------------------- ---------- ------  ----\n");
    for (i = 0; i < records->count; i++) {
        printf("%-4d %-20s %4dx%-4d %-6s  %s\n",
               records->items[i].id, records->items[i].name,
               records->items[i].width, records->items[i].height,
               record_format(&records->items[i]), records->items[i].path);
    }
}

static void print_query_results(const app_record_list_t *records) {
    int i;
    if (records->count == 0) {
        printf("No matched records.\n");
        return;
    }
    printf("ID  %-20s  %-10s  Format  Size      Path\n", "Name", "Size");
    printf("--- -------------------- ---------- ------  --------  ----\n");
    for (i = 0; i < records->count; i++) {
        printf("%-4d %-20s %4dx%-4d %-6s  %-8ld  %s\n",
               records->items[i].id, records->items[i].name,
               records->items[i].width, records->items[i].height,
               record_format(&records->items[i]),
               records->items[i].file_size, records->items[i].path);
    }
}

static int print_output_error(cli_output_status_t status, const char *path) {
    if (status == CLI_OUTPUT_OPEN_FAILED)
        fprintf(stderr, "[ERROR] Cannot open output file: %s\n", path);
    else if (status == CLI_OUTPUT_FINISH_FAILED)
        fprintf(stderr, "[ERROR] Failed to finish output file: %s\n", path);
    else
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", path);
    return 1;
}

static int write_processed_image(const app_command_t *command,
                                 const app_result_t *result) {
    const char *path = command_output_path(command);
    cli_output_status_t status =
        cli_output_write_image(path, result->data.image.image);
    const image_record_t *record = &result->data.image.source_record;

    if (status != CLI_OUTPUT_OK)
        return print_output_error(status, path);
    switch (command->kind) {
        case APP_COMMAND_RESIZE:
            printf("Resized %dx%d -> %dx%d, output: %s\n",
                   record->width, record->height, command->args.resize.width,
                   command->args.resize.height, path);
            break;
        case APP_COMMAND_ROTATE:
            printf("Rotated %d degrees, output: %s\n",
                   command->args.rotate.degrees, path);
            break;
        case APP_COMMAND_MEDIAN:
            printf("Median filter (k=%d) applied, output: %s\n",
                   command->args.median.kernel_size, path);
            break;
        case APP_COMMAND_ADJUST:
            printf("Adjusted (brightness=%d, contrast=%.2f), output: %s\n",
                   command->args.adjust.brightness,
                   command->args.adjust.contrast, path);
            break;
        case APP_COMMAND_RESIZE_BILINEAR:
            printf("Bilinear resized %dx%d -> %dx%d, output: %s\n",
                   record->width, record->height, command->args.resize.width,
                   command->args.resize.height, path);
            break;
        default:
            printf("Output written: %s\n", path);
            break;
    }
    return 0;
}

static int print_success(const app_command_t *command,
                         const app_result_t *result) {
    char time_buffer[64];
    cli_output_status_t output_status;
    int exported;
    int i;

    switch (command->kind) {
        case APP_COMMAND_INIT:
            printf("Store initialized.\n");
            break;
        case APP_COMMAND_IMPORT:
            printf("Import success.\n");
            printf("ID: %d\n", result->data.record.id);
            printf("Name: %s\n", result->data.record.name);
            printf("Width: %d\n", result->data.record.width);
            printf("Height: %d\n", result->data.record.height);
            printf("Path: %s\n", result->data.record.path);
            break;
        case APP_COMMAND_LIST:
            print_list(&result->data.records);
            break;
        case APP_COMMAND_INFO:
            format_time(result->data.record.import_time,
                        time_buffer, sizeof(time_buffer));
            printf("ID: %d\n", result->data.record.id);
            printf("Name: %s\n", result->data.record.name);
            printf("Width: %d x Height: %d\n", result->data.record.width,
                   result->data.record.height);
            printf("Channels: %d\n", result->data.record.channels);
            printf("File size: %ld bytes\n", result->data.record.file_size);
            printf("Import time: %s\n", time_buffer);
            printf("Path: %s\n", result->data.record.path);
            break;
        case APP_COMMAND_DELETE:
            printf("Deleted: ID %d (%s)\n", result->data.record.id,
                   result->data.record.name);
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
            return write_processed_image(command, result);
        case APP_COMMAND_HIST:
            printf("Histogram for image %d (%s):\n",
                   result->data.histogram.record.id,
                   result->data.histogram.record.name);
            print_feature(&result->data.histogram.feature);
            break;
        case APP_COMMAND_SEARCH:
            printf("Query image: %d\n", command->args.search.id);
            printf("Metric: %s\n", search_metric_name(command->args.search.metric));
            if (result->data.search.count == 0) {
                printf("No similar images found.\n");
            } else {
                printf("Top %d similar images:\n", result->data.search.count);
                for (i = 0; i < result->data.search.count; i++) {
                    const search_result_t *item = &result->data.search.items[i];
                    if (command->args.search.metric == METRIC_INTERSECTION)
                        printf("%d. id=%-4d name=%-20s score=%.4f\n",
                               i + 1, item->image_id, item->name, item->value);
                    else
                        printf("%d. id=%-4d name=%-20s distance=%.2f\n",
                               i + 1, item->image_id, item->name, item->value);
                }
            }
            break;
        case APP_COMMAND_SEARCH_SIMILAR:
            printf("rank,image_path,distance\n");
            for (i = 0; i < result->data.similarity.count; i++) {
                printf("%d,%s,%.2f\n", i + 1,
                       result->data.similarity.items[i].image_path,
                       result->data.similarity.items[i].distance);
            }
            break;
        case APP_COMMAND_FIND_NAME:
            print_find_results(&result->data.records);
            break;
        case APP_COMMAND_QUERY:
            print_query_results(&result->data.records);
            break;
        case APP_COMMAND_STATS:
            printf("Database Statistics:\n");
            printf("  Total records:    %d\n", result->data.stats.total_records);
            printf("  Active records:   %d\n", result->data.stats.active_records);
            printf("  Deleted records:  %d\n", result->data.stats.deleted_records);
            printf("  Total image size: %ld bytes\n",
                   result->data.stats.total_image_size);
            printf("  Format counts:\n");
            printf("    PPM: %d\n", result->data.stats.ppm_count);
            printf("    BMP: %d\n", result->data.stats.bmp_count);
            if (result->data.stats.active_records > 0) {
                printf("  Average width:    %.1f\n",
                       result->data.stats.average_width);
                printf("  Average height:   %.1f\n",
                       result->data.stats.average_height);
            } else {
                printf("  Average width:    N/A\n");
                printf("  Average height:   N/A\n");
            }
            printf("  Feature records:  %d\n",
                   result->data.stats.feature_records);
            break;
        case APP_COMMAND_COMPACT:
            printf("Compact complete.\n");
            printf("  Before: %d records\n", result->data.compact.before_count);
            printf("  After:  %d records\n", result->data.compact.after_count);
            printf("  Removed: %d deleted record(s)\n",
                   result->data.compact.before_count -
                   result->data.compact.after_count);
            break;
        case APP_COMMAND_EXPORT:
            output_status = cli_output_write_records(
                command->args.export_file.output_path,
                result->data.records.items, result->data.records.count,
                &exported);
            if (output_status != CLI_OUTPUT_OK)
                return print_output_error(output_status,
                    command->args.export_file.output_path);
            printf("Exported %d records to %s\n", exported,
                   command->args.export_file.output_path);
            break;
        case APP_COMMAND_REPORT:
            printf("Demo report generated: %s\n",
                   command->args.report.report_path);
            break;
        case APP_COMMAND_VERIFY:
            printf("Verify summary:\n");
            printf("total_records=%d\n", result->data.verify.total_records);
            printf("missing_files=%d\n", result->data.verify.missing_files);
            printf("missing_histograms=%d\n",
                   result->data.verify.missing_histograms);
            printf("duplicate_ids=%d\n", result->data.verify.duplicate_ids);
            printf("duplicate_paths=%d\n", result->data.verify.duplicate_paths);
            printf("invalid_records=%d\n", result->data.verify.invalid_records);
            printf("dimension_mismatches=%d\n",
                   result->data.verify.dimension_mismatches);
            printf("metadata_missing=%d\n", result->data.verify.metadata_missing);
            printf("feature_store_missing=%d\n",
                   result->data.verify.feature_store_missing);
            printf("status=%s\n",
                   result->data.verify.status_failed ? "FAILED" : "OK");
            return result->data.verify.status_failed ? 1 : 0;
        case APP_COMMAND_REPAIR:
            printf("Repair summary:\n");
            printf("removed_records=%d\n", result->data.repair.removed_records);
            printf("regenerated_histograms=%d\n",
                   result->data.repair.regenerated_histograms);
            printf("fixed_dimensions=%d\n",
                   result->data.repair.fixed_dimensions);
            printf("remaining_issues=%d\n",
                   result->data.repair.remaining_issues);
            return result->data.repair.remaining_issues ? 1 : 0;
        case APP_COMMAND_HIST_EXPORT:
            output_status = cli_output_write_histogram(
                command->args.hist_export.output_path,
                &result->data.histogram.feature,
                command->args.hist_export.normalized);
            if (output_status != CLI_OUTPUT_OK)
                return print_output_error(output_status,
                    command->args.hist_export.output_path);
            printf("Histogram exported to %s (%s)\n",
                   command->args.hist_export.output_path,
                   command->args.hist_export.normalized ? "normalized" : "raw");
            break;
        case APP_COMMAND_HIST_IMAGE:
            output_status = cli_output_write_image(
                command->args.image_output.output_path,
                result->data.image.image);
            if (output_status != CLI_OUTPUT_OK)
                return print_output_error(output_status,
                    command->args.image_output.output_path);
            printf("Histogram image written to %s\n",
                   command->args.image_output.output_path);
            break;
        case APP_COMMAND_SEARCH_EXPORT:
            output_status = cli_output_write_search(
                command->args.search_output.output_path,
                result->data.search_export.items,
                result->data.search_export.count,
                command->args.search_output.metric);
            if (output_status != CLI_OUTPUT_OK)
                return print_output_error(output_status,
                    command->args.search_output.output_path);
            printf("Search results exported to %s (%d results)\n",
                   command->args.search_output.output_path,
                   result->data.search_export.count);
            break;
        case APP_COMMAND_SEARCH_CONTACT:
            output_status = cli_output_write_image(
                command->args.search_output.output_path,
                result->data.image.image);
            if (output_status != CLI_OUTPUT_OK)
                return print_output_error(output_status,
                    command->args.search_output.output_path);
            printf("Contact sheet written to %s (%d images)\n",
                   command->args.search_output.output_path,
                   result->data.image.image_count);
            break;
        case APP_COMMAND_HELP:
            break;
    }
    return 0;
}

void cli_print_help(void) {
    printf("Usage: ./imagedb <command> [args...]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  init                  Initialize the store\n");
    printf("  import <file>         Import a PPM image\n");
    printf("  list                  List all images\n");
    printf("  info <id>             Show image details\n");
    printf("  gray <id> <out>       Convert to grayscale\n");
    printf("  binary <id> <t> <out> Apply binary threshold\n");
    printf("  blur <id> <out>       Apply 3x3 mean filter\n");
    printf("  edge <id> <out>       Sobel edge detection\n");
    printf("  hist <id>             Show color histogram\n");
    printf("  search <id> <k> [--metric l1|l2|intersection]  Find similar images\n");
    printf("  search-similar <query.ppm> --topk <k>  Top-K L1 search by query PPM\n");
    printf("  resize <id> <w> <h> <out>  Resize image (nearest neighbor)\n");
    printf("  resize-bilinear <id> <w> <h> <out>  Resize (bilinear interpolation)\n");
    printf("  rotate <id> <deg> <out>    Rotate image (90/180/270)\n");
    printf("  equalize <id> <out>    Histogram equalization\n");
    printf("  median <id> <k> <out>  Median filter (k=3 or 5)\n");
    printf("  gaussian <id> <out>    Gaussian 3x3 smooth\n");
    printf("  adjust <id> <b> <c> <out>  Brightness/contrast adjust\n");
    printf("  delete <id>           Delete an image record\n");
    printf("  find-name <keyword>   Find records by filename substring\n");
    printf("  query <f> <op> <v>    Query records (fields: id/name/width/height/format/size)\n");
    printf("  stats                 Show database statistics\n");
    printf("  compact               Remove deleted records permanently\n");
    printf("  export <file.csv>     Export records to CSV\n");
    printf("  report <output_dir> <html>  Generate HTML demo report\n");
    printf("  verify                Verify store metadata/files/features consistency\n");
    printf("  repair                Repair missing files/features/dimensions where possible\n");
    printf("  hist-export <id> <csv> [--normalized]  Export histogram to CSV\n");
    printf("  hist-image <id> <out> Draw histogram as image\n");
    printf("  search-export <id> <k> <csv> [--metric ...]  Export search to CSV\n");
    printf("  search-contact <id> <k> <out> [--metric ...]  Search result contact sheet\n");
    printf("  help                  Show this help\n");
}

int cli_run(int argc, char **argv) {
    const app_context_t context = {DATA_DIR};
    app_command_t command;
    app_result_t result;
    cli_parse_error_t parse_error;
    cli_parse_status_t parse_status;
    app_status_t app_status;
    int exit_code;

    parse_status = cli_parse(argc, argv, &command, &parse_error);
    if (parse_status == CLI_PARSE_HELP) {
        cli_print_help();
        return 0;
    }
    if (parse_status == CLI_PARSE_ERROR) {
        print_parse_error(&parse_error);
        return 1;
    }

    app_status = app_execute(&context, &command, &result);
    if (app_status != APP_STATUS_OK) {
        print_app_error(&command, app_status, &result);
        app_result_destroy(&result);
        return 1;
    }
    exit_code = print_success(&command, &result);
    app_result_destroy(&result);
    return exit_code;
}
