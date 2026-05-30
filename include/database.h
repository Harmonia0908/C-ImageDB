#ifndef DATABASE_H
#define DATABASE_H

#include "common.h"
#include "feature.h"

typedef struct image_record {
    int id;
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    int width;
    int height;
    int channels;
    long file_size;
    long import_time;
    uint64_t content_hash;
    int deleted;
} image_record_t;

int db_init(const char *data_dir);
int db_next_id(const char *data_dir);

int db_add_record(const char *data_dir, const image_record_t *record);
int db_load_records(const char *data_dir, image_record_t **records, int *count);
int db_find_record_by_id(const char *data_dir, int id, image_record_t *out);
int db_mark_deleted(const char *data_dir, int id);

int db_add_feature(const char *data_dir, const image_feature_t *feature);
int db_load_features(const char *data_dir, image_feature_t **features, int *count);
int db_find_feature_by_id(const char *data_dir, int id, image_feature_t *out);

/* Compact: remove deleted records from metadata.dat and features.dat.
 * Uses temp files + rename for safe atomic replacement.
 * Returns 0 on success, -1 on error (original files preserved).
 * *before_count and *after_count are set to record counts before/after. */
int db_compact(const char *data_dir, int *before_count, int *after_count);

/* Write an array of records to metadata.dat (fully replaces file).
 * Returns 0 on success, -1 on error. */
int db_write_records(const char *data_dir, const image_record_t *records, int count);

/* Write an array of features to features.dat (fully replaces file).
 * Returns 0 on success, -1 on error. */
int db_write_features(const char *data_dir, const image_feature_t *features, int count);

/* Atomically append a record and its feature to the store.
 * Uses temp+rename for both files. On failure, neither file is modified.
 * Returns 0 on success, -1 on error. */
int db_commit_import(const char *data_dir,
                     const image_record_t *record,
                     const image_feature_t *feature);

#endif
