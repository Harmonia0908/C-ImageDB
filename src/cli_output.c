#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "bmp.h"
#include "cli_output.h"
#include "ppm.h"

static FILE *open_atomic_output(const char *output_path, char **temp_path) {
    size_t length;
    FILE *file;

    if (!output_path || !*output_path || !temp_path)
        return NULL;
    if (strlen(output_path) > SIZE_MAX - sizeof(".tmp"))
        return NULL;
    length = strlen(output_path) + sizeof(".tmp");
    *temp_path = malloc(length);
    if (!*temp_path)
        return NULL;
    if (snprintf(*temp_path, length, "%s.tmp", output_path) < 0) {
        free(*temp_path);
        *temp_path = NULL;
        return NULL;
    }
    file = fopen(*temp_path, "w");
    if (!file) {
        free(*temp_path);
        *temp_path = NULL;
    }
    return file;
}

static int finish_atomic_output(FILE *file, char *temp_path,
                                const char *output_path) {
    int failed;

    if (!file || !temp_path || !output_path) {
        free(temp_path);
        return -1;
    }
    failed = ferror(file) != 0;
    if (fflush(file) != 0)
        failed = 1;
    if (fclose(file) != 0)
        failed = 1;
    if (!failed && rename(temp_path, output_path) == 0) {
        free(temp_path);
        return 0;
    }
    unlink(temp_path);
    free(temp_path);
    return -1;
}

static int write_csv_field(FILE *file, const char *field) {
    const unsigned char *cursor;
    int quoted;

    if (!file || !field)
        return -1;
    quoted = strpbrk(field, ",\"\r\n") != NULL;
    if (quoted && fputc('"', file) == EOF)
        return -1;
    for (cursor = (const unsigned char *)field; *cursor; cursor++) {
        if (*cursor == '"' && fputc('"', file) == EOF)
            return -1;
        if (fputc(*cursor, file) == EOF)
            return -1;
    }
    if (quoted && fputc('"', file) == EOF)
        return -1;
    return 0;
}

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

cli_output_status_t cli_output_write_image(const char *path,
                                           const image_t *image) {
    const char *extension = path ? strrchr(path, '.') : NULL;
    int status;

    if (extension && strcasecmp(extension, ".bmp") == 0)
        status = bmp_write(path, image);
    else
        status = ppm_write(path, image);
    return status == 0 ? CLI_OUTPUT_OK : CLI_OUTPUT_WRITE_FAILED;
}

cli_output_status_t cli_output_write_records(
    const char *path, const image_record_t *records, int count,
    int *exported_count) {
    FILE *file;
    char *temp_path = NULL;
    char time_buffer[64];
    int exported = 0;
    int i;

    file = open_atomic_output(path, &temp_path);
    if (!file)
        return CLI_OUTPUT_OPEN_FAILED;
    fprintf(file,
            "id,name,path,width,height,channels,format,file_size,import_time\n");
    for (i = 0; i < count; i++) {
        const image_record_t *record = &records[i];
        if (record->deleted)
            continue;
        format_time(record->import_time, time_buffer, sizeof(time_buffer));
        fprintf(file, "%d,", record->id);
        write_csv_field(file, record->name);
        fputc(',', file);
        write_csv_field(file, record->path);
        fputc(',', file);
        fprintf(file, "%d,%d,%d,%s,%ld,%s\n",
                record->width, record->height, record->channels,
                record_format(record), record->file_size, time_buffer);
        exported++;
    }
    if (finish_atomic_output(file, temp_path, path) != 0)
        return CLI_OUTPUT_FINISH_FAILED;
    if (exported_count)
        *exported_count = exported;
    return CLI_OUTPUT_OK;
}

cli_output_status_t cli_output_write_histogram(
    const char *path, const image_feature_t *feature, int normalized) {
    FILE *file;
    char *temp_path = NULL;
    int bin;

    file = open_atomic_output(path, &temp_path);
    if (!file)
        return CLI_OUTPUT_OPEN_FAILED;
    if (normalized) {
        int total_r = 0;
        int total_g = 0;
        int total_b = 0;
        for (bin = 0; bin < 256; bin++) {
            total_r += feature->r_hist[bin];
            total_g += feature->g_hist[bin];
            total_b += feature->b_hist[bin];
        }
        if (total_r < 1) total_r = 1;
        if (total_g < 1) total_g = 1;
        if (total_b < 1) total_b = 1;
        fprintf(file, "bin,r_norm,g_norm,b_norm\n");
        for (bin = 0; bin < 256; bin++) {
            fprintf(file, "%d,%.6f,%.6f,%.6f\n", bin,
                    feature->r_hist[bin] / (double)total_r,
                    feature->g_hist[bin] / (double)total_g,
                    feature->b_hist[bin] / (double)total_b);
        }
    } else {
        fprintf(file, "bin,r,g,b\n");
        for (bin = 0; bin < 256; bin++) {
            fprintf(file, "%d,%d,%d,%d\n", bin,
                    feature->r_hist[bin], feature->g_hist[bin],
                    feature->b_hist[bin]);
        }
    }
    return finish_atomic_output(file, temp_path, path) == 0
               ? CLI_OUTPUT_OK : CLI_OUTPUT_FINISH_FAILED;
}

cli_output_status_t cli_output_write_search(
    const char *path, const app_search_export_item_t *items, int count,
    search_metric_t metric) {
    FILE *file;
    char *temp_path = NULL;
    int i;

    file = open_atomic_output(path, &temp_path);
    if (!file)
        return CLI_OUTPUT_OPEN_FAILED;
    fprintf(file, "rank,id,name,metric,value,path\n");
    for (i = 0; i < count; i++) {
        fprintf(file, "%d,%d,", i + 1, items[i].result.image_id);
        write_csv_field(file, items[i].result.name);
        fputc(',', file);
        write_csv_field(file, search_metric_name(metric));
        fprintf(file, ",%.4f,", items[i].result.value);
        write_csv_field(file, items[i].path);
        fputc('\n', file);
    }
    return finish_atomic_output(file, temp_path, path) == 0
               ? CLI_OUTPUT_OK : CLI_OUTPUT_FINISH_FAILED;
}
