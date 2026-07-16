#ifndef STORE_FILE_H
#define STORE_FILE_H

#include "database.h"

/* Initialize the existing Store directory layout and files. This preserves
 * the legacy probe-or-create behavior: readable files are left unchanged;
 * a read-open failure is treated as a missing file and retried in create
 * mode. Returns -1 for invalid paths and directory/open/create failures.
 * To preserve legacy behavior, close errors from successful read probes and
 * from newly created empty Record/Feature placeholders are not reported. */
int store_file_init(const char *data_dir);

/* Read and advance .next_id using the existing decimal text format.
 * On success, returns 0 and stores the allocated positive ID in *out_id.
 * On failure, returns -1 and leaves *out_id set to -1. */
int store_file_next_id(const char *data_dir, int *out_id);

/* Load raw native-layout Store arrays. On success, the caller owns the
 * returned allocation and must free it. Empty files return NULL/0. On a
 * failure after valid arguments are supplied, outputs are reset to NULL/0. */
int store_file_load_records(const char *data_dir,
                            image_record_t **records, int *count);
int store_file_load_features(const char *data_dir,
                             image_feature_t **features, int *count);

/* Atomically replace one Store array using the existing temp+rename flow.
 * Input arrays are borrowed for the duration of the call. */
int store_file_replace_records(const char *data_dir,
                               const image_record_t *records, int count);
int store_file_replace_features(const char *data_dir,
                                const image_feature_t *features, int count);

/* Replace metadata and features as one logical Store update. Runtime write
 * and install failures are rolled back using the existing .bak files. After
 * a successful install, failure to remove a .bak file is not reported,
 * preserving legacy behavior. The historical process-crash window between
 * POSIX renames is unchanged. */
int store_file_replace_store(const char *data_dir,
                             const image_record_t *records, int record_count,
                             const image_feature_t *features,
                             int feature_count);

#endif
