#include <stdlib.h>
#include <limits.h>
#include "database.h"
#include "storage/store_file.h"

int db_init(const char *data_dir) {
    return store_file_init(data_dir);
}

int db_next_id(const char *data_dir) {
    int id;

    if (store_file_next_id(data_dir, &id) != 0)
        return -1;
    return id;
}

int db_add_record(const char *data_dir, const image_record_t *record) {
    image_record_t *records = NULL;
    image_record_t *grown;
    int count = 0;
    int result;

    if (!record)
        return -1;
    if (db_load_records(data_dir, &records, &count) != 0 || count == INT_MAX)
        return -1;
    grown = realloc(records, (size_t)(count + 1) * sizeof(*records));
    if (!grown) {
        free(records);
        return -1;
    }
    records = grown;
    records[count] = *record;
    result = db_write_records(data_dir, records, count + 1);
    free(records);
    return result;
}

int db_load_records(const char *data_dir, image_record_t **records, int *count) {
    return store_file_load_records(data_dir, records, count);
}

int db_find_record_by_id(const char *data_dir, int id, image_record_t *out) {
    image_record_t *records;
    int count;
    int i;
    int found = 0;

    if (!out || id <= 0 || db_load_records(data_dir, &records, &count) != 0)
        return -1;

    for (i = 0; i < count; i++) {
        if (records[i].id == id && !records[i].deleted) {
            *out = records[i];
            found = 1;
            break;
        }
    }

    free(records);
    return found ? 0 : -1;
}

int db_mark_deleted(const char *data_dir, int id) {
    image_record_t *records;
    int count;
    int i;
    int found = 0;
    int result;

    if (id <= 0 || db_load_records(data_dir, &records, &count) != 0)
        return -1;

    for (i = 0; i < count; i++) {
        if (records[i].id == id && !records[i].deleted) {
            records[i].deleted = 1;
            found = 1;
            break;
        }
    }

    if (!found) {
        free(records);
        return -1;
    }

    result = db_write_records(data_dir, records, count);
    free(records);
    return result;
}

int db_add_feature(const char *data_dir, const image_feature_t *feature) {
    image_feature_t *features = NULL;
    image_feature_t *grown;
    int count = 0;
    int result;

    if (!feature)
        return -1;
    if (db_load_features(data_dir, &features, &count) != 0 || count == INT_MAX)
        return -1;
    grown = realloc(features, (size_t)(count + 1) * sizeof(*features));
    if (!grown) {
        free(features);
        return -1;
    }
    features = grown;
    features[count] = *feature;
    result = db_write_features(data_dir, features, count + 1);
    free(features);
    return result;
}

int db_load_features(const char *data_dir, image_feature_t **features, int *count) {
    return store_file_load_features(data_dir, features, count);
}

int db_find_feature_by_id(const char *data_dir, int id, image_feature_t *out) {
    image_feature_t *features;
    int count;
    int i;
    int found = 0;

    if (!out || id <= 0 || db_load_features(data_dir, &features, &count) != 0)
        return -1;

    for (i = 0; i < count; i++) {
        if (features[i].image_id == id) {
            *out = features[i];
            found = 1;
            break;
        }
    }

    free(features);
    return found ? 0 : -1;
}

int db_write_records(const char *data_dir, const image_record_t *records, int count) {
    return store_file_replace_records(data_dir, records, count);
}

int db_write_features(const char *data_dir, const image_feature_t *features, int count) {
    return store_file_replace_features(data_dir, features, count);
}

int db_replace_store(const char *data_dir,
                     const image_record_t *records, int record_count,
                     const image_feature_t *features, int feature_count) {
    return store_file_replace_store(data_dir, records, record_count,
                                    features, feature_count);
}

int db_commit_import(const char *data_dir,
                     const image_record_t *record,
                     const image_feature_t *feature) {
    image_record_t *records = NULL;
    image_feature_t *features = NULL;
    int rec_count = 0, feat_count = 0;
    int result = -1;

    if (!record || !feature)
        return -1;

    if (db_load_records(data_dir, &records, &rec_count) != 0)
        return -1;
    if (db_load_features(data_dir, &features, &feat_count) != 0) {
        free(records);
        return -1;
    }

    if (rec_count == INT_MAX || feat_count == INT_MAX)
        goto cleanup;

    {
        image_record_t *t = realloc(records,
            (size_t)(rec_count + 1) * sizeof(image_record_t));
        if (!t)
            goto cleanup;
        records = t;
    }
    records[rec_count] = *record;
    rec_count++;

    {
        image_feature_t *t = realloc(features,
            (size_t)(feat_count + 1) * sizeof(image_feature_t));
        if (!t)
            goto cleanup;
        features = t;
    }
    features[feat_count] = *feature;
    feat_count++;

    result = db_replace_store(data_dir, records, rec_count,
                              features, feat_count);

cleanup:
    free(records);
    free(features);
    return result;
}

int db_compact(const char *data_dir, int *before_count, int *after_count) {
    image_record_t *records = NULL;
    image_feature_t *features = NULL;
    image_record_t *keep_records = NULL;
    image_feature_t *keep_features = NULL;
    int rec_count = 0, feat_count = 0;
    int keep_rec = 0, keep_feat = 0;
    int i, j;
    int *keep_id = NULL;
    int keep_id_count = 0;
    int result = -1;

    if (before_count) *before_count = 0;
    if (after_count) *after_count = 0;

    if (db_load_records(data_dir, &records, &rec_count) != 0)
        goto cleanup;
    if (db_load_features(data_dir, &features, &feat_count) != 0)
        goto cleanup;

    if (before_count) *before_count = rec_count;

    if (rec_count == 0) {
        if (after_count) *after_count = 0;
        result = 0;
        goto cleanup;
    }

    keep_records = malloc((size_t)rec_count * sizeof(image_record_t));
    keep_id = malloc((size_t)rec_count * sizeof(int));
    if (!keep_records || !keep_id)
        goto cleanup;

    for (i = 0; i < rec_count; i++) {
        if (!records[i].deleted) {
            keep_records[keep_rec] = records[i];
            keep_id[keep_id_count++] = records[i].id;
            keep_rec++;
        }
    }

    keep_features = malloc((size_t)feat_count * sizeof(image_feature_t));
    if (!keep_features && feat_count > 0)
        goto cleanup;

    for (i = 0; i < feat_count; i++) {
        int found = 0;
        for (j = 0; j < keep_id_count; j++) {
            if (features[i].image_id == keep_id[j]) {
                found = 1;
                break;
            }
        }
        if (found)
            keep_features[keep_feat++] = features[i];
    }

    result = db_replace_store(data_dir, keep_records, keep_rec,
                              keep_features, keep_feat);
    if (result == 0 && after_count)
        *after_count = keep_rec;

cleanup:
    free(records);
    free(features);
    free(keep_records);
    free(keep_features);
    free(keep_id);
    return result;
}
