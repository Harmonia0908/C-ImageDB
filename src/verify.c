#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "verify.h"
#include "database.h"
#include "ppm.h"
#include "bmp.h"

static int file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int make_store_path(char *buf, size_t size,
                           const char *data_dir, const char *name) {
    int n;

    if (!buf || !data_dir || !name || size == 0)
        return -1;

    n = snprintf(buf, size, "%s/%s", data_dir, name);
    return (n < 0 || (size_t)n >= size) ? -1 : 0;
}

static image_t *read_image_any(const char *path) {
    const char *ext = strrchr(path, '.');

    if (ext && strcasecmp(ext, ".bmp") == 0)
        return bmp_read(path);
    return ppm_read(path);
}

static int valid_record_fields(const image_record_t *rec) {
    if (!rec)
        return 0;
    if (rec->id <= 0)
        return 0;
    if (rec->name[0] == '\0' || rec->path[0] == '\0')
        return 0;
    if (rec->width <= 0 || rec->height <= 0 || rec->channels != 3)
        return 0;
    if (rec->file_size < 0 || rec->import_time < 0)
        return 0;
    return 1;
}

static int has_feature_for_id(const image_feature_t *features,
                              int feature_count, int image_id) {
    int i;

    for (i = 0; i < feature_count; i++) {
        if (features[i].image_id == image_id)
            return 1;
    }

    return 0;
}

static int feature_id_is_active(const image_record_t *records,
                                int record_count, int image_id) {
    int i;

    for (i = 0; i < record_count; i++) {
        if (!records[i].deleted && records[i].id == image_id)
            return 1;
    }

    return 0;
}

static void calculate_status(verify_summary_t *summary) {
    summary->status_failed =
        summary->metadata_missing ||
        summary->feature_store_missing ||
        summary->missing_files ||
        summary->missing_histograms ||
        summary->duplicate_ids ||
        summary->duplicate_paths ||
        summary->invalid_records ||
        summary->dimension_mismatches;
}

int verify_database(const char *data_dir, verify_summary_t *summary) {
    image_record_t *records = NULL;
    image_feature_t *features = NULL;
    int record_count = 0;
    int feature_count = 0;
    int i, j;
    char path_buf[512];

    if (!summary)
        return -1;

    memset(summary, 0, sizeof(*summary));

    if (make_store_path(path_buf, sizeof(path_buf), data_dir, "metadata.dat") != 0 ||
        !file_exists(path_buf)) {
        summary->metadata_missing = 1;
        summary->status_failed = 1;
        return 0;
    }

    if (make_store_path(path_buf, sizeof(path_buf), data_dir, "features.dat") != 0 ||
        !file_exists(path_buf)) {
        summary->feature_store_missing = 1;
    }

    if (db_load_records(data_dir, &records, &record_count) != 0) {
        summary->metadata_missing = 1;
        summary->status_failed = 1;
        return 0;
    }

    if (db_load_features(data_dir, &features, &feature_count) != 0) {
        summary->feature_store_missing = 1;
        feature_count = 0;
        features = NULL;
    }

    summary->total_records = record_count;

    for (i = 0; i < record_count; i++) {
        image_t *img;

        if (records[i].deleted)
            continue;

        if (!valid_record_fields(&records[i]))
            summary->invalid_records++;

        for (j = i + 1; j < record_count; j++) {
            if (records[j].deleted)
                continue;
            if (records[i].id == records[j].id)
                summary->duplicate_ids++;
            if (records[i].path[0] && strcmp(records[i].path, records[j].path) == 0)
                summary->duplicate_paths++;
        }

        if (!file_exists(records[i].path)) {
            summary->missing_files++;
        } else {
            img = read_image_any(records[i].path);
            if (!img) {
                summary->invalid_records++;
            } else {
                if (img->width != records[i].width ||
                    img->height != records[i].height ||
                    img->channels != records[i].channels)
                    summary->dimension_mismatches++;
                image_destroy(img);
            }
        }

        if (!has_feature_for_id(features, feature_count, records[i].id))
            summary->missing_histograms++;
    }

    for (i = 0; i < feature_count; i++) {
        if (!feature_id_is_active(records, record_count, features[i].image_id))
            summary->invalid_records++;
    }

    calculate_status(summary);
    free(records);
    free(features);
    return 0;
}

int repair_database(const char *data_dir, repair_summary_t *summary) {
    image_record_t *records = NULL;
    image_feature_t *features = NULL;
    image_record_t *new_records = NULL;
    image_feature_t *new_features = NULL;
    int record_count = 0;
    int feature_count = 0;
    int new_record_count = 0;
    int new_feature_count = 0;
    int i;
    verify_summary_t verify_after;

    if (!summary)
        return -1;

    memset(summary, 0, sizeof(*summary));

    if (db_load_records(data_dir, &records, &record_count) != 0)
        return -1;
    if (db_load_features(data_dir, &features, &feature_count) != 0) {
        free(records);
        return -1;
    }

    if (record_count > 0) {
        new_records = malloc((size_t)record_count * sizeof(image_record_t));
        new_features = malloc((size_t)record_count * sizeof(image_feature_t));
        if (!new_records || !new_features) {
            free(records);
            free(features);
            free(new_records);
            free(new_features);
            return -1;
        }
    }

    for (i = 0; i < record_count; i++) {
        image_t *img;
        image_feature_t feature;
        int has_feature;

        if (records[i].deleted)
            continue;

        if (!valid_record_fields(&records[i]) || !file_exists(records[i].path)) {
            summary->removed_records++;
            continue;
        }

        img = read_image_any(records[i].path);
        if (!img) {
            summary->removed_records++;
            continue;
        }

        if (records[i].width != img->width ||
            records[i].height != img->height ||
            records[i].channels != img->channels) {
            records[i].width = img->width;
            records[i].height = img->height;
            records[i].channels = img->channels;
            summary->fixed_dimensions++;
        }

        has_feature = 0;
        for (int j = 0; j < feature_count; j++) {
            if (features[j].image_id == records[i].id) {
                feature = features[j];
                has_feature = 1;
                break;
            }
        }

        if (!has_feature) {
            if (feature_extract_rgb_hist(img, records[i].id, &feature) != 0) {
                image_destroy(img);
                free(records);
                free(features);
                free(new_records);
                free(new_features);
                return -1;
            }
            summary->regenerated_histograms++;
        }

        image_destroy(img);
        new_records[new_record_count++] = records[i];
        new_features[new_feature_count++] = feature;
    }

    if (db_write_records(data_dir, new_records, new_record_count) != 0 ||
        db_write_features(data_dir, new_features, new_feature_count) != 0) {
        free(records);
        free(features);
        free(new_records);
        free(new_features);
        return -1;
    }

    if (verify_database(data_dir, &verify_after) != 0) {
        free(records);
        free(features);
        free(new_records);
        free(new_features);
        return -1;
    }
    summary->remaining_issues = verify_after.status_failed ? 1 : 0;

    free(records);
    free(features);
    free(new_records);
    free(new_features);
    return 0;
}

void verify_print_summary(const verify_summary_t *summary) {
    printf("Verify summary:\n");
    printf("total_records=%d\n", summary->total_records);
    printf("missing_files=%d\n", summary->missing_files);
    printf("missing_histograms=%d\n", summary->missing_histograms);
    printf("duplicate_ids=%d\n", summary->duplicate_ids);
    printf("duplicate_paths=%d\n", summary->duplicate_paths);
    printf("invalid_records=%d\n", summary->invalid_records);
    printf("dimension_mismatches=%d\n", summary->dimension_mismatches);
    printf("metadata_missing=%d\n", summary->metadata_missing);
    printf("feature_store_missing=%d\n", summary->feature_store_missing);
    printf("status=%s\n", summary->status_failed ? "FAILED" : "OK");
}

void repair_print_summary(const repair_summary_t *summary) {
    printf("Repair summary:\n");
    printf("removed_records=%d\n", summary->removed_records);
    printf("regenerated_histograms=%d\n", summary->regenerated_histograms);
    printf("fixed_dimensions=%d\n", summary->fixed_dimensions);
    printf("remaining_issues=%d\n", summary->remaining_issues);
}
