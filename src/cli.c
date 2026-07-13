#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include "cli.h"
#include "ppm.h"
#include "bmp.h"
#include "database.h"
#include "process.h"
#include "feature.h"
#include "search.h"
#include "similarity.h"
#include "report.h"
#include "verify.h"
#include "visualize.h"

#define DATA_DIR        "data"
#define COPY_BUF_SIZE   4096

/* Parse a positive integer from s. On success returns 1 and sets *out.
 * On failure (non-numeric, extra chars, overflow, <= 0) returns 0. */
static int parse_positive_int(const char *s, int *out) {
    char *end;
    long val;

    if (!s || !*s) return 0;

    errno = 0;
    val = strtol(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0')
        return 0;
    if (val <= 0 || val > (long)INT_MAX)
        return 0;

    *out = (int)val;
    return 1;
}

/* Parse an integer in [lo, hi] from s. */
static int parse_int_range(const char *s, int lo, int hi, int *out) {
    char *end;
    long val;

    if (!s || !*s) return 0;

    errno = 0;
    val = strtol(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0')
        return 0;
    if (val < (long)lo || val > (long)hi)
        return 0;

    *out = (int)val;
    return 1;
}

static uint64_t hash_pixels(const unsigned char *data, size_t len) {
    uint64_t hash = 5381;
    size_t i;
    for (i = 0; i < len; i++)
        hash = ((hash << 5) + hash) + data[i];
    return hash;
}

static int file_copy(const char *src_path, const char *dst_path) {
    FILE *src, *dst;
    char buf[COPY_BUF_SIZE];
    size_t n;
    int src_ok, dst_ok;

    src = fopen(src_path, "rb");
    if (!src) return -1;

    dst = fopen(dst_path, "wbx");
    if (!dst) {
        fclose(src);
        return -1;
    }

    while ((n = fread(buf, 1, COPY_BUF_SIZE, src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            fclose(src);
            fclose(dst);
            unlink(dst_path);
            return -1;
        }
    }

    if (ferror(src)) {
        fclose(src);
        fclose(dst);
        unlink(dst_path);
        return -1;
    }

    src_ok = fclose(src);
    dst_ok = fclose(dst);
    if (src_ok != 0 || dst_ok != 0) {
        unlink(dst_path);
        return -1;
    }

    return 0;
}

static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;
    return st.st_size;
}

static int check_duplicate(const char *data_dir, uint64_t hash) {
    image_record_t *records;
    int count;
    int i;
    int dup = 0;

    if (db_load_records(data_dir, &records, &count) != 0)
        return 0;

    for (i = 0; i < count; i++) {
        if (!records[i].deleted && records[i].content_hash == hash) {
            dup = 1;
            break;
        }
    }

    free(records);
    return dup;
}

static void format_time(long t, char *buf, size_t bufsize) {
    time_t tt = (time_t)t;
    struct tm *tm_info;

    if (!buf || bufsize == 0)
        return;
    tm_info = localtime(&tt);
    if (!tm_info || strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", tm_info) == 0)
        snprintf(buf, bufsize, "unknown");
}

static FILE *open_atomic_output(const char *output_path, char **tmp_path) {
    size_t len;
    FILE *fp;

    if (!output_path || !*output_path || !tmp_path)
        return NULL;
    if (strlen(output_path) > SIZE_MAX - sizeof(".tmp"))
        return NULL;
    len = strlen(output_path) + sizeof(".tmp");
    *tmp_path = malloc(len);
    if (!*tmp_path)
        return NULL;
    if (snprintf(*tmp_path, len, "%s.tmp", output_path) < 0) {
        free(*tmp_path);
        *tmp_path = NULL;
        return NULL;
    }
    fp = fopen(*tmp_path, "w");
    if (!fp) {
        free(*tmp_path);
        *tmp_path = NULL;
    }
    return fp;
}

static int finish_atomic_output(FILE *fp, char *tmp_path,
                                const char *output_path) {
    int failed;

    if (!fp || !tmp_path || !output_path) {
        free(tmp_path);
        return -1;
    }
    failed = ferror(fp) != 0;
    if (fflush(fp) != 0)
        failed = 1;
    if (fclose(fp) != 0)
        failed = 1;
    if (!failed && rename(tmp_path, output_path) == 0) {
        free(tmp_path);
        return 0;
    }
    unlink(tmp_path);
    free(tmp_path);
    return -1;
}

static int csv_write_field(FILE *fp, const char *field) {
    const unsigned char *p;
    int quoted;

    if (!fp || !field)
        return -1;
    quoted = strpbrk(field, ",\"\r\n") != NULL;
    if (quoted && fputc('"', fp) == EOF)
        return -1;
    for (p = (const unsigned char *)field; *p; p++) {
        if (*p == '"' && fputc('"', fp) == EOF)
            return -1;
        if (fputc(*p, fp) == EOF)
            return -1;
    }
    if (quoted && fputc('"', fp) == EOF)
        return -1;
    return 0;
}

/* Format-aware image I/O dispatchers */
static image_t *image_read_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext && strcasecmp(ext, ".bmp") == 0)
        return bmp_read(path);
    return ppm_read(path);
}

static int image_write_file(const char *path, const image_t *img) {
    const char *ext = strrchr(path, '.');
    if (ext && strcasecmp(ext, ".bmp") == 0)
        return bmp_write(path, img);
    return ppm_write(path, img);
}

/* Store only canonical extensions. This avoids carrying directory components
 * from source paths whose parent directory happens to contain a dot. */
static const char *stored_extension(const char *path) {
    const char *ext = strrchr(path, '.');
    return (ext && strcasecmp(ext, ".bmp") == 0) ? ".bmp" : ".ppm";
}

/* -- commands -- */

static int cmd_init(void) {
    if (db_init(DATA_DIR) != 0) {
        fprintf(stderr, "[ERROR] Failed to initialize store\n");
        return 1;
    }
    printf("Store initialized.\n");
    return 0;
}

static int cmd_import(const char *filepath) {
    image_t *img;
    image_record_t record;
    image_feature_t feature;
    char dst_path[MAX_PATH_LEN];
    char time_buf[64];
    uint64_t hash;
    int id;
    long fsize;
    size_t pixel_bytes;
    const char *basename;
    int path_len;

    img = image_read_file(filepath);
    if (!img) {
        fprintf(stderr, "[ERROR] Failed to read image file: %s\n", filepath);
        return 1;
    }

    pixel_bytes = (size_t)img->width * (size_t)img->height * (size_t)img->channels;
    hash = hash_pixels(img->data, pixel_bytes);

    if (check_duplicate(DATA_DIR, hash)) {
        fprintf(stderr, "[ERROR] Image already exists (content hash match)\n");
        image_destroy(img);
        return 1;
    }

    /* Extract feature early — fail before any I/O if extraction fails */
    if (feature_extract_rgb_hist(img, 0, &feature) != 0) {
        fprintf(stderr, "[ERROR] Failed to extract feature\n");
        image_destroy(img);
        return 1;
    }

    basename = strrchr(filepath, '/');
    basename = basename ? basename + 1 : filepath;
    if (*basename == '\0' || strlen(basename) >= MAX_NAME_LEN) {
        fprintf(stderr, "[ERROR] Image filename is empty or exceeds %d bytes\n",
                MAX_NAME_LEN - 1);
        image_destroy(img);
        return 1;
    }

    id = db_next_id(DATA_DIR);
    if (id < 0) {
        fprintf(stderr, "[ERROR] Failed to allocate ID\n");
        image_destroy(img);
        return 1;
    }
    feature.image_id = id;

    path_len = snprintf(dst_path, sizeof(dst_path), "data/images/%d%s", id,
                        stored_extension(filepath));
    if (path_len < 0 || (size_t)path_len >= sizeof(dst_path)) {
        fprintf(stderr, "[ERROR] Store path is too long\n");
        image_destroy(img);
        return 1;
    }

    if (file_copy(filepath, dst_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to copy image to store\n");
        image_destroy(img);
        return 1;
    }

    fsize = file_size(filepath);
    if (fsize < 0) fsize = 0;

    memset(&record, 0, sizeof(record));
    record.id = id;
    memcpy(record.name, basename, strlen(basename) + 1);
    memcpy(record.path, dst_path, strlen(dst_path) + 1);
    record.width = img->width;
    record.height = img->height;
    record.channels = img->channels;
    record.file_size = fsize;
    record.import_time = (long)time(NULL);
    record.content_hash = hash;
    record.deleted = 0;

    if (db_commit_import(DATA_DIR, &record, &feature) != 0) {
        fprintf(stderr, "[ERROR] Failed to commit import transaction\n");
        image_destroy(img);
        unlink(dst_path);
        return 1;
    }

    image_destroy(img);

    format_time(record.import_time, time_buf, sizeof(time_buf));
    printf("Import success.\n");
    printf("ID: %d\n", id);
    printf("Name: %s\n", record.name);
    printf("Width: %d\n", record.width);
    printf("Height: %d\n", record.height);
    printf("Path: %s\n", record.path);

    return 0;
}

static int cmd_list(void) {
    image_record_t *records;
    int count;
    int i;
    char time_buf[64];

    if (db_load_records(DATA_DIR, &records, &count) != 0) {
        fprintf(stderr, "[ERROR] Failed to load records\n");
        return 1;
    }

    if (count == 0) {
        printf("No images.\n");
        return 0;
    }

    printf("ID  %-20s  %-10s  Import time\n", "Name", "Size");
    printf("--- -------------------- ---------- -------------------\n");

    for (i = 0; i < count; i++) {
        if (records[i].deleted)
            continue;

        format_time(records[i].import_time, time_buf, sizeof(time_buf));
        printf("%-4d %-20s %4dx%-4d %s\n",
               records[i].id,
               records[i].name,
               records[i].width,
               records[i].height,
               time_buf);
    }

    free(records);
    return 0;
}

static int cmd_info(int id) {
    image_record_t record;
    char time_buf[64];

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    format_time(record.import_time, time_buf, sizeof(time_buf));

    printf("ID: %d\n", record.id);
    printf("Name: %s\n", record.name);
    printf("Width: %d x Height: %d\n", record.width, record.height);
    printf("Channels: %d\n", record.channels);
    printf("File size: %ld bytes\n", record.file_size);
    printf("Import time: %s\n", time_buf);
    printf("Path: %s\n", record.path);

    return 0;
}

static int cmd_delete(int id) {
    image_record_t record;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    if (db_mark_deleted(DATA_DIR, id) != 0) {
        fprintf(stderr, "[ERROR] Failed to delete record\n");
        return 1;
    }

    printf("Deleted: ID %d (%s)\n", id, record.name);
    return 0;
}

static int process_cmd(int id, const char *out_path,
                        image_t *(*fn)(const image_t *)) {
    image_record_t record;
    image_t *src, *dst;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    src = image_read_file(record.path);
    if (!src) {
        fprintf(stderr, "[ERROR] Failed to read image: %s\n", record.path);
        return 1;
    }

    dst = fn(src);
    image_destroy(src);

    if (!dst) {
        fprintf(stderr, "[ERROR] Processing failed\n");
        return 1;
    }

    if (image_write_file(out_path, dst) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", out_path);
        image_destroy(dst);
        return 1;
    }

    image_destroy(dst);
    printf("Output written: %s\n", out_path);
    return 0;
}

static int cmd_gray(int id, const char *out_path) {
    return process_cmd(id, out_path, process_gray);
}

static int cmd_binary(int id, int threshold, const char *out_path) {
    image_record_t record;
    image_t *src, *dst;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    if (threshold < 0 || threshold > 255) {
        fprintf(stderr, "[ERROR] Threshold must be 0-255, got %d\n", threshold);
        return 1;
    }

    src = image_read_file(record.path);
    if (!src) {
        fprintf(stderr, "[ERROR] Failed to read image: %s\n", record.path);
        return 1;
    }

    dst = process_binary(src, threshold);
    image_destroy(src);

    if (!dst) {
        fprintf(stderr, "[ERROR] Processing failed\n");
        return 1;
    }

    if (image_write_file(out_path, dst) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", out_path);
        image_destroy(dst);
        return 1;
    }

    image_destroy(dst);
    printf("Output written: %s\n", out_path);
    return 0;
}

static int cmd_blur(int id, const char *out_path) {
    return process_cmd(id, out_path, process_blur3x3);
}

static int cmd_edge(int id, const char *out_path) {
    return process_cmd(id, out_path, process_sobel_edge);
}

static int cmd_hist(int id) {
    image_feature_t feature;
    image_record_t record;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    if (db_find_feature_by_id(DATA_DIR, id, &feature) != 0) {
        fprintf(stderr, "[ERROR] Feature not found for image %d\n", id);
        return 1;
    }

    printf("Histogram for image %d (%s):\n", id, record.name);
    feature_print_summary(&feature);
    feature_print_full(&feature);

    return 0;
}

static int cmd_search(int query_id, int top_k, search_metric_t metric) {
    search_result_t *results;
    int count;
    int i;

    if (top_k <= 0) {
        fprintf(stderr, "[ERROR] top_k must be positive, got %d\n", top_k);
        return 1;
    }

    if (search_similar(DATA_DIR, query_id, top_k, metric, &results, &count) != 0) {
        fprintf(stderr, "[ERROR] Search failed. Check that image %d exists and has a feature.\n", query_id);
        return 1;
    }

    printf("Query image: %d\n", query_id);
    printf("Metric: %s\n", search_metric_name(metric));
    if (count == 0) {
        printf("No similar images found.\n");
    } else {
        printf("Top %d similar images:\n", count);
        for (i = 0; i < count; i++) {
            if (metric == METRIC_INTERSECTION)
                printf("%d. id=%-4d name=%-20s score=%.4f\n",
                       i + 1, results[i].image_id, results[i].name, results[i].value);
            else
                printf("%d. id=%-4d name=%-20s distance=%.2f\n",
                       i + 1, results[i].image_id, results[i].name, results[i].value);
        }
    }

    free(results);
    return 0;
}

static int cmd_search_similar(const char *query_path, int top_k) {
    similar_image_result_t *results = NULL;
    int count = 0;
    int i;
    int status;

    if (top_k <= 0) {
        fprintf(stderr, "[ERROR] topk must be positive, got %d\n", top_k);
        return 1;
    }

    if (access(query_path, R_OK) != 0) {
        fprintf(stderr, "[ERROR] Query file not found or unreadable: %s\n", query_path);
        return 1;
    }

    status = similarity_search_ppm(DATA_DIR, query_path, top_k, &results, &count);
    if (status != SIMILARITY_OK) {
        switch (status) {
            case SIMILARITY_ERR_INVALID_TOPK:
                fprintf(stderr, "[ERROR] Invalid topk: %d\n", top_k);
                break;
            case SIMILARITY_ERR_BAD_QUERY:
                fprintf(stderr, "[ERROR] Invalid PPM query file: %s\n", query_path);
                break;
            case SIMILARITY_ERR_EMPTY_DB:
                fprintf(stderr, "[ERROR] Database is empty\n");
                break;
            case SIMILARITY_ERR_NOMEM:
                fprintf(stderr, "[ERROR] Out of memory during similarity search\n");
                break;
            default:
                fprintf(stderr, "[ERROR] Failed to search similar images\n");
                break;
        }
        free(results);
        return 1;
    }

    printf("rank,image_path,distance\n");
    for (i = 0; i < count; i++) {
        printf("%d,%s,%.2f\n", i + 1, results[i].image_path, results[i].distance);
    }

    free(results);
    return 0;
}

static int cmd_resize(int id, int new_w, int new_h, const char *out_path) {
    image_record_t record;
    image_t *src, *dst;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    if (new_w <= 0 || new_h <= 0) {
        fprintf(stderr, "[ERROR] Invalid dimensions: %dx%d\n", new_w, new_h);
        return 1;
    }

    src = image_read_file(record.path);
    if (!src) {
        fprintf(stderr, "[ERROR] Failed to read image: %s\n", record.path);
        return 1;
    }

    dst = process_resize_nearest(src, new_w, new_h);
    image_destroy(src);

    if (!dst) {
        fprintf(stderr, "[ERROR] Resize failed\n");
        return 1;
    }

    if (image_write_file(out_path, dst) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", out_path);
        image_destroy(dst);
        return 1;
    }

    image_destroy(dst);
    printf("Resized %dx%d -> %dx%d, output: %s\n",
           record.width, record.height, new_w, new_h, out_path);
    return 0;
}

static int cmd_rotate(int id, int degrees, const char *out_path) {
    image_record_t record;
    image_t *src, *dst;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    if (degrees != 90 && degrees != 180 && degrees != 270) {
        fprintf(stderr, "[ERROR] Rotation must be 90, 180, or 270, got %d\n", degrees);
        return 1;
    }

    src = image_read_file(record.path);
    if (!src) {
        fprintf(stderr, "[ERROR] Failed to read image: %s\n", record.path);
        return 1;
    }

    dst = process_rotate(src, degrees);
    image_destroy(src);

    if (!dst) {
        fprintf(stderr, "[ERROR] Rotate failed\n");
        return 1;
    }

    if (image_write_file(out_path, dst) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", out_path);
        image_destroy(dst);
        return 1;
    }

    image_destroy(dst);
    printf("Rotated %d degrees, output: %s\n", degrees, out_path);
    return 0;
}

/* -- Phase 6: image processing commands -- */

static int cmd_equalize(int id, const char *out_path) {
    return process_cmd(id, out_path, process_equalize);
}

static int cmd_median(int id, int kernel_size, const char *out_path) {
    image_record_t record;
    image_t *src, *dst;

    if (kernel_size != 3 && kernel_size != 5) {
        fprintf(stderr, "[ERROR] Kernel size must be 3 or 5, got %d\n", kernel_size);
        return 1;
    }

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    src = image_read_file(record.path);
    if (!src) {
        fprintf(stderr, "[ERROR] Failed to read image: %s\n", record.path);
        return 1;
    }

    dst = process_median(src, kernel_size);
    image_destroy(src);

    if (!dst) {
        fprintf(stderr, "[ERROR] Median filter failed\n");
        return 1;
    }

    if (image_write_file(out_path, dst) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", out_path);
        image_destroy(dst);
        return 1;
    }

    image_destroy(dst);
    printf("Median filter (k=%d) applied, output: %s\n", kernel_size, out_path);
    return 0;
}

static int cmd_gaussian(int id, const char *out_path) {
    return process_cmd(id, out_path, process_gaussian);
}

static int cmd_adjust(int id, int brightness, double contrast, const char *out_path) {
    image_record_t record;
    image_t *src, *dst;

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    src = image_read_file(record.path);
    if (!src) {
        fprintf(stderr, "[ERROR] Failed to read image: %s\n", record.path);
        return 1;
    }

    dst = process_adjust(src, brightness, contrast);
    image_destroy(src);

    if (!dst) {
        fprintf(stderr, "[ERROR] Adjust failed\n");
        return 1;
    }

    if (image_write_file(out_path, dst) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", out_path);
        image_destroy(dst);
        return 1;
    }

    image_destroy(dst);
    printf("Adjusted (brightness=%d, contrast=%.2f), output: %s\n",
           brightness, contrast, out_path);
    return 0;
}

static int cmd_resize_bilinear(int id, int new_w, int new_h, const char *out_path) {
    image_record_t record;
    image_t *src, *dst;

    if (new_w <= 0 || new_h <= 0) {
        fprintf(stderr, "[ERROR] Invalid dimensions: %dx%d\n", new_w, new_h);
        return 1;
    }

    if (db_find_record_by_id(DATA_DIR, id, &record) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    src = image_read_file(record.path);
    if (!src) {
        fprintf(stderr, "[ERROR] Failed to read image: %s\n", record.path);
        return 1;
    }

    dst = process_resize_bilinear(src, new_w, new_h);
    image_destroy(src);

    if (!dst) {
        fprintf(stderr, "[ERROR] Bilinear resize failed\n");
        return 1;
    }

    if (image_write_file(out_path, dst) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", out_path);
        image_destroy(dst);
        return 1;
    }

    image_destroy(dst);
    printf("Bilinear resized %dx%d -> %dx%d, output: %s\n",
           record.width, record.height, new_w, new_h, out_path);
    return 0;
}

/* -- Phase 7: visualization commands -- */

static int cmd_hist_export(int id, const char *output_path, int normalized) {
    image_feature_t feature;
    image_record_t rec;
    FILE *fp;
    char *tmp_path = NULL;
    int bin;

    if (db_find_record_by_id(DATA_DIR, id, &rec) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    if (db_find_feature_by_id(DATA_DIR, id, &feature) != 0) {
        fprintf(stderr, "[ERROR] Feature not found for image %d\n", id);
        return 1;
    }

    fp = open_atomic_output(output_path, &tmp_path);
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open output file: %s\n", output_path);
        return 1;
    }

    if (normalized) {
        /* Compute total pixels for normalization */
        int total_r = 0, total_g = 0, total_b = 0;
        for (bin = 0; bin < 256; bin++) {
            total_r += feature.r_hist[bin];
            total_g += feature.g_hist[bin];
            total_b += feature.b_hist[bin];
        }
        if (total_r < 1) total_r = 1;
        if (total_g < 1) total_g = 1;
        if (total_b < 1) total_b = 1;

        fprintf(fp, "bin,r_norm,g_norm,b_norm\n");
        for (bin = 0; bin < 256; bin++) {
            fprintf(fp, "%d,%.6f,%.6f,%.6f\n",
                    bin,
                    feature.r_hist[bin] / (double)total_r,
                    feature.g_hist[bin] / (double)total_g,
                    feature.b_hist[bin] / (double)total_b);
        }
    } else {
        fprintf(fp, "bin,r,g,b\n");
        for (bin = 0; bin < 256; bin++) {
            fprintf(fp, "%d,%d,%d,%d\n",
                    bin,
                    feature.r_hist[bin],
                    feature.g_hist[bin],
                    feature.b_hist[bin]);
        }
    }

    if (finish_atomic_output(fp, tmp_path, output_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to finish output file: %s\n",
                output_path);
        return 1;
    }
    printf("Histogram exported to %s (%s)\n", output_path,
           normalized ? "normalized" : "raw");
    return 0;
}

static int cmd_hist_image(int id, const char *output_path) {
    image_feature_t feature;
    image_record_t rec;
    image_t *img;

    if (db_find_record_by_id(DATA_DIR, id, &rec) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", id);
        return 1;
    }

    if (db_find_feature_by_id(DATA_DIR, id, &feature) != 0) {
        fprintf(stderr, "[ERROR] Feature not found for image %d\n", id);
        return 1;
    }

    img = visualize_hist_image(&feature);
    if (!img) {
        fprintf(stderr, "[ERROR] Failed to generate histogram image\n");
        return 1;
    }

    if (image_write_file(output_path, img) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", output_path);
        image_destroy(img);
        return 1;
    }

    image_destroy(img);
    printf("Histogram image written to %s\n", output_path);
    return 0;
}

static int cmd_search_export(int query_id, int top_k, const char *output_path,
                              search_metric_t metric) {
    search_result_t *results;
    int count, i;
    FILE *fp;
    char *tmp_path = NULL;

    if (top_k <= 0) {
        fprintf(stderr, "[ERROR] top_k must be positive, got %d\n", top_k);
        return 1;
    }

    if (search_similar(DATA_DIR, query_id, top_k, metric, &results, &count) != 0) {
        fprintf(stderr, "[ERROR] Search failed for image %d\n", query_id);
        return 1;
    }

    fp = open_atomic_output(output_path, &tmp_path);
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open output file: %s\n", output_path);
        free(results);
        return 1;
    }

    fprintf(fp, "rank,id,name,metric,value,path\n");
    for (i = 0; i < count; i++) {
        image_record_t rec;
        char path_str[MAX_PATH_LEN] = "";
        if (db_find_record_by_id(DATA_DIR, results[i].image_id, &rec) == 0)
            snprintf(path_str, sizeof(path_str), "%s", rec.path);

        fprintf(fp, "%d,%d,", i + 1, results[i].image_id);
        csv_write_field(fp, results[i].name);
        fputc(',', fp);
        csv_write_field(fp, search_metric_name(metric));
        fprintf(fp, ",%.4f,", results[i].value);
        csv_write_field(fp, path_str);
        fputc('\n', fp);
    }

    if (finish_atomic_output(fp, tmp_path, output_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to finish output file: %s\n",
                output_path);
        free(results);
        return 1;
    }
    free(results);
    printf("Search results exported to %s (%d results)\n", output_path, count);
    return 0;
}

static int cmd_search_contact(int query_id, int top_k, const char *output_path,
                               search_metric_t metric) {
    search_result_t *results;
    image_record_t query_rec;
    image_t *query_img = NULL;
    image_t **thumbs = NULL;
    image_t *sheet = NULL;
    int count, i;
    int result = 1;

    if (top_k <= 0) {
        fprintf(stderr, "[ERROR] top_k must be positive, got %d\n", top_k);
        return 1;
    }

    /* Safety limit: prevent unreasonable allocations */
    if (top_k > 100) {
        fprintf(stderr, "[ERROR] top_k too large (max 100): %d\n", top_k);
        return 1;
    }

    if (search_similar(DATA_DIR, query_id, top_k, metric, &results, &count) != 0) {
        fprintf(stderr, "[ERROR] Search failed for image %d\n", query_id);
        return 1;
    }

    if (db_find_record_by_id(DATA_DIR, query_id, &query_rec) != 0) {
        fprintf(stderr, "[ERROR] Record not found: ID %d\n", query_id);
        free(results);
        return 1;
    }

    /* Load query image */
    query_img = image_read_file(query_rec.path);
    if (!query_img) {
        fprintf(stderr, "[ERROR] Failed to read query image: %s\n", query_rec.path);
        goto done;
    }

    /* Allocate thumb array: 1 query + count results */
    thumbs = calloc((size_t)(1 + count), sizeof(image_t *));
    if (!thumbs) goto done;

    /* Resize query to 128x128 */
    thumbs[0] = process_resize_nearest(query_img, 128, 128);
    if (!thumbs[0]) goto done;

    /* Load and resize each result image */
    for (i = 0; i < count; i++) {
        image_record_t rec;
        image_t *src;

        if (db_find_record_by_id(DATA_DIR, results[i].image_id, &rec) != 0)
            goto done;

        src = image_read_file(rec.path);
        if (!src) goto done;

        thumbs[i + 1] = process_resize_nearest(src, 128, 128);
        image_destroy(src);
        if (!thumbs[i + 1]) goto done;
    }

    sheet = visualize_contact_sheet(thumbs, 1 + count, 128, 128);
    if (!sheet) goto done;

    if (image_write_file(output_path, sheet) != 0) {
        fprintf(stderr, "[ERROR] Failed to write output: %s\n", output_path);
        goto done;
    }

    printf("Contact sheet written to %s (%d images)\n", output_path, 1 + count);
    result = 0;

done:
    image_destroy(query_img);
    if (thumbs) {
        for (i = 0; i <= count; i++)
            image_destroy(thumbs[i]);
        free(thumbs);
    }
    image_destroy(sheet);
    free(results);
    return result;
}

/* -- Phase 5: database commands -- */

static char *str_tolower(char *dst, const char *src, size_t size) {
    size_t i;
    for (i = 0; i + 1 < size && src[i]; i++)
        dst[i] = (char)((unsigned char)src[i] >= 'A' && (unsigned char)src[i] <= 'Z'
                        ? src[i] + 32 : src[i]);
    dst[i] = '\0';
    return dst;
}

static const char *record_format_str(const image_record_t *rec) {
    const char *ext = strrchr(rec->path, '.');
    if (!ext) return "unknown";
    if (strcasecmp(ext, ".bmp") == 0) return "BMP";
    return "PPM";
}

static int cmd_find_name(const char *keyword) {
    image_record_t *records;
    int count, i;
    int found = 0;
    char name_lower[MAX_NAME_LEN];
    char kw_lower[MAX_NAME_LEN];

    if (!keyword || !*keyword) {
        fprintf(stderr, "[ERROR] Keyword must not be empty\n");
        return 1;
    }

    if (db_load_records(DATA_DIR, &records, &count) != 0) {
        fprintf(stderr, "[ERROR] Failed to load records\n");
        return 1;
    }

    str_tolower(kw_lower, keyword, sizeof(kw_lower));

    for (i = 0; i < count; i++) {
        if (records[i].deleted)
            continue;

        str_tolower(name_lower, records[i].name, sizeof(name_lower));
        if (strstr(name_lower, kw_lower)) {
            if (!found) {
                printf("ID  %-20s  %-10s  Format  Path\n", "Name", "Size");
                printf("--- -------------------- ---------- ------  ----\n");
            }
            printf("%-4d %-20s %4dx%-4d %-6s  %s\n",
                   records[i].id, records[i].name,
                   records[i].width, records[i].height,
                   record_format_str(&records[i]),
                   records[i].path);
            found++;
        }
    }

    if (!found)
        printf("No matched records.\n");

    free(records);
    return 0;
}

static int parse_query_op(const char *op_str, int *op_out) {
    if (strcmp(op_str, "eq") == 0)       { *op_out = 0; return 1; }
    if (strcmp(op_str, "ne") == 0)       { *op_out = 1; return 1; }
    if (strcmp(op_str, "gt") == 0)       { *op_out = 2; return 1; }
    if (strcmp(op_str, "ge") == 0)       { *op_out = 3; return 1; }
    if (strcmp(op_str, "lt") == 0)       { *op_out = 4; return 1; }
    if (strcmp(op_str, "le") == 0)       { *op_out = 5; return 1; }
    if (strcmp(op_str, "contains") == 0) { *op_out = 6; return 1; }
    return 0;
}

/* Check if field+op combination is valid. Returns 1 if valid, 0 if not. */
static int valid_field_op(const char *field, int op) {
    int is_numeric = (strcmp(field, "id") == 0 ||
                      strcmp(field, "width") == 0 ||
                      strcmp(field, "height") == 0 ||
                      strcmp(field, "size") == 0);
    int is_name = (strcmp(field, "name") == 0);
    int is_format = (strcmp(field, "format") == 0);

    if (is_numeric && op >= 0 && op <= 5) return 1;  /* eq/ne/gt/ge/lt/le */
    if (is_name && (op == 0 || op == 1 || op == 6)) return 1;  /* eq/ne/contains */
    if (is_format && (op == 0 || op == 1)) return 1;  /* eq/ne */
    return 0;
}

/* Extract the comparable value from a record for a given field.
 * For numeric fields returns the value in *num.
 * For name/format, copies to *str_buf. */
static void get_field_value(const image_record_t *rec, const char *field,
                            long *num, const char **str_val) {
    if (strcmp(field, "id") == 0)         { *num = rec->id; }
    else if (strcmp(field, "width") == 0)  { *num = rec->width; }
    else if (strcmp(field, "height") == 0) { *num = rec->height; }
    else if (strcmp(field, "size") == 0)   { *num = rec->file_size; }
    else if (strcmp(field, "name") == 0)   { *str_val = rec->name; }
    else if (strcmp(field, "format") == 0) { *str_val = record_format_str(rec); }
}

static int match_record(const image_record_t *rec, const char *field,
                        int op, long num_val, const char *str_val) {
    long rec_num = 0;
    const char *rec_str = NULL;

    get_field_value(rec, field, &rec_num, &rec_str);

    if (strcmp(field, "name") == 0 || strcmp(field, "format") == 0) {
        if (op == 0) return strcmp(rec_str, str_val) == 0;
        if (op == 1) return strcmp(rec_str, str_val) != 0;
        if (op == 6) return strstr(rec_str, str_val) != NULL;
        return 0;
    }

    /* Numeric comparison */
    switch (op) {
        case 0: return rec_num == num_val;
        case 1: return rec_num != num_val;
        case 2: return rec_num > num_val;
        case 3: return rec_num >= num_val;
        case 4: return rec_num < num_val;
        case 5: return rec_num <= num_val;
    }
    return 0;
}

static int cmd_query(const char *field, const char *op_str, const char *value) {
    image_record_t *records;
    int count, i;
    int op;
    long num_val = 0;
    int found = 0;

    /* Validate field */
    if (strcmp(field, "id") != 0 && strcmp(field, "name") != 0 &&
        strcmp(field, "width") != 0 && strcmp(field, "height") != 0 &&
        strcmp(field, "format") != 0 && strcmp(field, "size") != 0) {
        fprintf(stderr, "[ERROR] Unknown field: %s (use id, name, width, height, format, size)\n", field);
        return 1;
    }

    /* Parse operator */
    if (!parse_query_op(op_str, &op)) {
        fprintf(stderr, "[ERROR] Unknown operator: %s\n", op_str);
        return 1;
    }

    /* Validate field+op combination */
    if (!valid_field_op(field, op)) {
        fprintf(stderr, "[ERROR] Operator '%s' not valid for field '%s'\n", op_str, field);
        return 1;
    }

    /* Parse numeric value for numeric fields */
    if (strcmp(field, "id") == 0 || strcmp(field, "width") == 0 ||
        strcmp(field, "height") == 0 || strcmp(field, "size") == 0) {
        char *end;
        errno = 0;
        num_val = strtol(value, &end, 10);
        if (errno == ERANGE || end == value || *end != '\0') {
            fprintf(stderr, "[ERROR] Invalid numeric value for field '%s': %s\n", field, value);
            return 1;
        }
    }

    if (db_load_records(DATA_DIR, &records, &count) != 0) {
        fprintf(stderr, "[ERROR] Failed to load records\n");
        return 1;
    }

    for (i = 0; i < count; i++) {
        if (records[i].deleted)
            continue;

        if (match_record(&records[i], field, op, num_val, value)) {
            if (!found) {
                printf("ID  %-20s  %-10s  Format  Size      Path\n", "Name", "Size");
                printf("--- -------------------- ---------- ------  --------  ----\n");
            }
            printf("%-4d %-20s %4dx%-4d %-6s  %-8ld  %s\n",
                   records[i].id, records[i].name,
                   records[i].width, records[i].height,
                   record_format_str(&records[i]),
                   records[i].file_size,
                   records[i].path);
            found++;
        }
    }

    if (!found)
        printf("No matched records.\n");

    free(records);
    return 0;
}

static int cmd_stats(void) {
    image_record_t *records;
    image_feature_t *features;
    int rec_count, feat_count;
    int active = 0, deleted = 0;
    long total_size = 0;
    double sum_w = 0.0, sum_h = 0.0;
    int ppm_count = 0, bmp_count = 0;
    int i;

    if (db_load_records(DATA_DIR, &records, &rec_count) != 0) {
        fprintf(stderr, "[ERROR] Failed to load records (database may be corrupt)\n");
        return 1;
    }

    if (db_load_features(DATA_DIR, &features, &feat_count) != 0) {
        free(records);
        fprintf(stderr, "[ERROR] Failed to load features (database may be corrupt)\n");
        return 1;
    }

    for (i = 0; i < rec_count; i++) {
        if (records[i].deleted) {
            deleted++;
        } else {
            active++;
            total_size += records[i].file_size;
            sum_w += records[i].width;
            sum_h += records[i].height;

            if (strcmp(record_format_str(&records[i]), "BMP") == 0)
                bmp_count++;
            else
                ppm_count++;
        }
    }

    printf("Database Statistics:\n");
    printf("  Total records:    %d\n", rec_count);
    printf("  Active records:   %d\n", active);
    printf("  Deleted records:  %d\n", deleted);
    printf("  Total image size: %ld bytes\n", total_size);
    printf("  Format counts:\n");
    printf("    PPM: %d\n", ppm_count);
    printf("    BMP: %d\n", bmp_count);
    if (active > 0) {
        printf("  Average width:    %.1f\n", sum_w / active);
        printf("  Average height:   %.1f\n", sum_h / active);
    } else {
        printf("  Average width:    N/A\n");
        printf("  Average height:   N/A\n");
    }
    printf("  Feature records:  %d\n", feat_count);

    free(records);
    free(features);
    return 0;
}

static int cmd_compact(void) {
    int before, after;

    if (db_compact(DATA_DIR, &before, &after) != 0) {
        fprintf(stderr, "[ERROR] Compact failed. Database unchanged.\n");
        return 1;
    }

    printf("Compact complete.\n");
    printf("  Before: %d records\n", before);
    printf("  After:  %d records\n", after);
    printf("  Removed: %d deleted record(s)\n", before - after);
    return 0;
}

static int cmd_export(const char *output_path) {
    image_record_t *records;
    int count, i;
    FILE *fp;
    char *tmp_path = NULL;
    int exported = 0;
    char time_buf[64];

    if (db_load_records(DATA_DIR, &records, &count) != 0) {
        fprintf(stderr, "[ERROR] Failed to load records\n");
        return 1;
    }

    fp = open_atomic_output(output_path, &tmp_path);
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open output file: %s\n", output_path);
        free(records);
        return 1;
    }

    /* CSV header */
    fprintf(fp, "id,name,path,width,height,channels,format,file_size,import_time\n");

    for (i = 0; i < count; i++) {
        const image_record_t *rec = &records[i];
        if (rec->deleted) continue;

        format_time(rec->import_time, time_buf, sizeof(time_buf));

        fprintf(fp, "%d,", rec->id);
        csv_write_field(fp, rec->name);
        fputc(',', fp);
        csv_write_field(fp, rec->path);
        fputc(',', fp);
        fprintf(fp, "%d,%d,%d,%s,%ld,%s\n",
                rec->width, rec->height, rec->channels,
                record_format_str(rec),
                rec->file_size, time_buf);
        exported++;
    }

    if (finish_atomic_output(fp, tmp_path, output_path) != 0) {
        fprintf(stderr, "[ERROR] Failed to finish output file: %s\n",
                output_path);
        free(records);
        return 1;
    }
    free(records);

    printf("Exported %d records to %s\n", exported, output_path);
    return 0;
}

static int cmd_report(const char *output_dir, const char *report_path) {
    if (generate_html_report(output_dir, report_path) != 0)
        return 1;

    printf("Demo report generated: %s\n", report_path);
    return 0;
}

static int cmd_verify(void) {
    verify_summary_t summary;

    if (verify_database(DATA_DIR, &summary) != 0) {
        fprintf(stderr, "[ERROR] Verify failed\n");
        return 1;
    }

    verify_print_summary(&summary);
    return summary.status_failed ? 1 : 0;
}

static int cmd_repair(void) {
    repair_summary_t summary;

    if (repair_database(DATA_DIR, &summary) != 0) {
        fprintf(stderr, "[ERROR] Repair failed\n");
        return 1;
    }

    repair_print_summary(&summary);
    return summary.remaining_issues ? 1 : 0;
}

/* -- main CLI -- */

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

/* Parse a required positive integer argument. Prints error and returns 0 on failure. */
#define PARSE_POSITIVE(arg, var, label) do { \
    if (!parse_positive_int((arg), &(var))) { \
        fprintf(stderr, "[ERROR] Invalid %s: %s\n", label, arg); \
        return 1; \
    } \
} while(0)

int cli_run(int argc, char **argv) {
    if (argc < 2) {
        cli_print_help();
        return 0;
    }

    if (strcmp(argv[1], "help") == 0) {
        if (argc != 2) {
            fprintf(stderr, "[ERROR] help takes no arguments\n");
            return 1;
        }
        cli_print_help();
        return 0;
    }

    if (strcmp(argv[1], "init") == 0) {
        if (argc != 2) {
            fprintf(stderr, "[ERROR] init takes no arguments\n");
            return 1;
        }
        return cmd_init();
    }

    if (strcmp(argv[1], "import") == 0) {
        if (argc != 3) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb import <file>\n");
            return 1;
        }
        return cmd_import(argv[2]);
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) {
            fprintf(stderr, "[ERROR] list takes no arguments\n");
            return 1;
        }
        return cmd_list();
    }

    if (strcmp(argv[1], "info") == 0) {
        int id;
        if (argc != 3) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb info <id>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_info(id);
    }

    if (strcmp(argv[1], "gray") == 0) {
        int id;
        if (argc != 4) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb gray <id> <out>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_gray(id, argv[3]);
    }

    if (strcmp(argv[1], "binary") == 0) {
        int id, threshold;
        if (argc != 5) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb binary <id> <threshold> <out>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        if (!parse_int_range(argv[3], 0, 255, &threshold)) {
            fprintf(stderr, "[ERROR] Invalid threshold: %s\n", argv[3]);
            return 1;
        }
        return cmd_binary(id, threshold, argv[4]);
    }

    if (strcmp(argv[1], "blur") == 0) {
        int id;
        if (argc != 4) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb blur <id> <out>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_blur(id, argv[3]);
    }

    if (strcmp(argv[1], "edge") == 0) {
        int id;
        if (argc != 4) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb edge <id> <out>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_edge(id, argv[3]);
    }

    if (strcmp(argv[1], "hist") == 0) {
        int id;
        if (argc != 3) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb hist <id>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_hist(id);
    }

    if (strcmp(argv[1], "search") == 0) {
        int id, k;
        search_metric_t metric = METRIC_INTERSECTION;

        if (argc < 4 || argc > 6) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb search <id> <k> [--metric l1|l2|intersection]\n");
            return 1;
        }

        PARSE_POSITIVE(argv[2], id, "ID");
        PARSE_POSITIVE(argv[3], k, "k");

        if (argc >= 5) {
            if (argc != 6 || strcmp(argv[4], "--metric") != 0) {
                fprintf(stderr, "[ERROR] Usage: ./imagedb search <id> <k> [--metric l1|l2|intersection]\n");
                return 1;
            }
            if (strcmp(argv[5], "l1") == 0)
                metric = METRIC_L1;
            else if (strcmp(argv[5], "l2") == 0)
                metric = METRIC_L2;
            else if (strcmp(argv[5], "intersection") == 0)
                metric = METRIC_INTERSECTION;
            else {
                fprintf(stderr, "[ERROR] Unknown metric: %s (use l1, l2, or intersection)\n", argv[5]);
                return 1;
            }
        }

        return cmd_search(id, k, metric);
    }

    if (strcmp(argv[1], "search-similar") == 0) {
        int k;

        if (argc != 5 || strcmp(argv[3], "--topk") != 0) {
            fprintf(stderr, "[ERROR] Usage: ./cimagedb search-similar <query.ppm> --topk K\n");
            return 1;
        }

        PARSE_POSITIVE(argv[4], k, "topk");
        return cmd_search_similar(argv[2], k);
    }

    if (strcmp(argv[1], "resize") == 0) {
        int id, new_w, new_h;
        if (argc != 6) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb resize <id> <new_width> <new_height> <output>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        PARSE_POSITIVE(argv[3], new_w, "new_width");
        PARSE_POSITIVE(argv[4], new_h, "new_height");
        return cmd_resize(id, new_w, new_h, argv[5]);
    }

    if (strcmp(argv[1], "rotate") == 0) {
        int id, degrees;
        if (argc != 5) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb rotate <id> <90|180|270> <output>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        if (!parse_int_range(argv[3], 0, 360, &degrees)) {
            fprintf(stderr, "[ERROR] Invalid angle: %s (use 90, 180, or 270)\n", argv[3]);
            return 1;
        }
        return cmd_rotate(id, degrees, argv[4]);
    }

    if (strcmp(argv[1], "delete") == 0) {
        int id;
        if (argc != 3) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb delete <id>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_delete(id);
    }

    if (strcmp(argv[1], "equalize") == 0) {
        int id;
        if (argc != 4) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb equalize <id> <output>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_equalize(id, argv[3]);
    }

    if (strcmp(argv[1], "median") == 0) {
        int id, ks;
        if (argc != 5) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb median <id> <kernel_size> <output>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        PARSE_POSITIVE(argv[3], ks, "kernel_size");
        return cmd_median(id, ks, argv[4]);
    }

    if (strcmp(argv[1], "gaussian") == 0) {
        int id;
        if (argc != 4) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb gaussian <id> <output>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_gaussian(id, argv[3]);
    }

    if (strcmp(argv[1], "adjust") == 0) {
        int id, brightness;
        double contrast;
        char *end;

        if (argc != 6) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb adjust <id> <brightness> <contrast> <output>\n");
            return 1;
        }
        {
            long lb;
            PARSE_POSITIVE(argv[2], id, "ID");
            errno = 0;
            lb = strtol(argv[3], &end, 10);
            if (errno == ERANGE || end == argv[3] || *end != '\0' ||
                lb < (long)INT_MIN || lb > (long)INT_MAX) {
                fprintf(stderr, "[ERROR] Invalid brightness: %s\n", argv[3]);
                return 1;
            }
            brightness = (int)lb;
        }

        errno = 0;
        contrast = strtod(argv[4], &end);
        if (errno == ERANGE || end == argv[4] || *end != '\0' ||
            !isfinite(contrast) || contrast <= 0.0 || contrast > 10.0) {
            fprintf(stderr, "[ERROR] Invalid contrast: %s (must be 0 < x <= 10)\n", argv[4]);
            return 1;
        }

        return cmd_adjust(id, brightness, contrast, argv[5]);
    }

    if (strcmp(argv[1], "resize-bilinear") == 0) {
        int id, new_w, new_h;
        if (argc != 6) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb resize-bilinear <id> <new_w> <new_h> <output>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        PARSE_POSITIVE(argv[3], new_w, "new_width");
        PARSE_POSITIVE(argv[4], new_h, "new_height");
        return cmd_resize_bilinear(id, new_w, new_h, argv[5]);
    }

    if (strcmp(argv[1], "find-name") == 0) {
        if (argc != 3) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb find-name <keyword>\n");
            return 1;
        }
        return cmd_find_name(argv[2]);
    }

    if (strcmp(argv[1], "query") == 0) {
        if (argc != 5) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb query <field> <op> <value>\n");
            return 1;
        }
        return cmd_query(argv[2], argv[3], argv[4]);
    }

    if (strcmp(argv[1], "stats") == 0) {
        if (argc != 2) {
            fprintf(stderr, "[ERROR] stats takes no arguments\n");
            return 1;
        }
        return cmd_stats();
    }

    if (strcmp(argv[1], "compact") == 0) {
        if (argc != 2) {
            fprintf(stderr, "[ERROR] compact takes no arguments\n");
            return 1;
        }
        return cmd_compact();
    }

    if (strcmp(argv[1], "export") == 0) {
        if (argc != 3) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb export <output.csv>\n");
            return 1;
        }
        return cmd_export(argv[2]);
    }

    if (strcmp(argv[1], "report") == 0) {
        if (argc != 4) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb report <output_dir> <report.html>\n");
            return 1;
        }
        return cmd_report(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "verify") == 0) {
        if (argc != 2) {
            fprintf(stderr, "[ERROR] verify takes no arguments\n");
            return 1;
        }
        return cmd_verify();
    }

    if (strcmp(argv[1], "repair") == 0) {
        if (argc != 2) {
            fprintf(stderr, "[ERROR] repair takes no arguments\n");
            return 1;
        }
        return cmd_repair();
    }

    if (strcmp(argv[1], "hist-export") == 0) {
        int id, normalized = 0;
        if (argc < 4 || argc > 5) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb hist-export <id> <output.csv> [--normalized]\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        if (argc == 5) {
            if (strcmp(argv[4], "--normalized") == 0)
                normalized = 1;
            else {
                fprintf(stderr, "[ERROR] Unknown option: %s\n", argv[4]);
                return 1;
            }
        }
        return cmd_hist_export(id, argv[3], normalized);
    }

    if (strcmp(argv[1], "hist-image") == 0) {
        int id;
        if (argc != 4) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb hist-image <id> <output>\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        return cmd_hist_image(id, argv[3]);
    }

    if (strcmp(argv[1], "search-export") == 0) {
        int id, k;
        search_metric_t metric = METRIC_INTERSECTION;

        if (argc < 5 || argc > 7) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb search-export <id> <k> <output.csv> [--metric l1|l2|intersection]\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        PARSE_POSITIVE(argv[3], k, "k");

        if (argc >= 6) {
            if (argc != 7 || strcmp(argv[5], "--metric") != 0) {
                fprintf(stderr, "[ERROR] Usage: ./imagedb search-export <id> <k> <output.csv> [--metric l1|l2|intersection]\n");
                return 1;
            }
            if (strcmp(argv[6], "l1") == 0) metric = METRIC_L1;
            else if (strcmp(argv[6], "l2") == 0) metric = METRIC_L2;
            else if (strcmp(argv[6], "intersection") == 0) metric = METRIC_INTERSECTION;
            else {
                fprintf(stderr, "[ERROR] Unknown metric: %s\n", argv[6]);
                return 1;
            }
        }
        return cmd_search_export(id, k, argv[4], metric);
    }

    if (strcmp(argv[1], "search-contact") == 0) {
        int id, k;
        search_metric_t metric = METRIC_INTERSECTION;

        if (argc < 5 || argc > 7) {
            fprintf(stderr, "[ERROR] Usage: ./imagedb search-contact <id> <k> <output> [--metric l1|l2|intersection]\n");
            return 1;
        }
        PARSE_POSITIVE(argv[2], id, "ID");
        PARSE_POSITIVE(argv[3], k, "k");

        if (argc >= 6) {
            if (argc != 7 || strcmp(argv[5], "--metric") != 0) {
                fprintf(stderr, "[ERROR] Usage: ./imagedb search-contact <id> <k> <output> [--metric l1|l2|intersection]\n");
                return 1;
            }
            if (strcmp(argv[6], "l1") == 0) metric = METRIC_L1;
            else if (strcmp(argv[6], "l2") == 0) metric = METRIC_L2;
            else if (strcmp(argv[6], "intersection") == 0) metric = METRIC_INTERSECTION;
            else {
                fprintf(stderr, "[ERROR] Unknown metric: %s\n", argv[6]);
                return 1;
            }
        }
        return cmd_search_contact(id, k, argv[4], metric);
    }

    fprintf(stderr, "[ERROR] Unknown command: %s\n", argv[1]);
    return 1;
}
