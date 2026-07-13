#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include "database.h"

#define METADATA_FILE   "/metadata.dat"
#define FEATURES_FILE   "/features.dat"
#define NEXT_ID_FILE    "/.next_id"
#define IMAGES_DIR      "/images"

static int ensure_dir(const char *path) {
    struct stat st;

    if (!path || !*path)
        return -1;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode) ? 0 : -1;
    if (errno != ENOENT)
        return -1;
    if (mkdir(path, 0755) != 0)
        return -1;
    return 0;
}

static char *make_path(const char *data_dir, const char *suffix) {
    char *path;
    size_t len;

    if (!data_dir || !suffix)
        return NULL;
    if (strlen(data_dir) > SIZE_MAX - strlen(suffix) - 1)
        return NULL;
    len = strlen(data_dir) + strlen(suffix) + 1;
    path = malloc(len);
    if (!path)
        return NULL;

    snprintf(path, len, "%s%s", data_dir, suffix);
    return path;
}

static int finish_write(FILE *fp) {
    int result = 0;

    if (fflush(fp) != 0)
        result = -1;
    if (fclose(fp) != 0)
        result = -1;
    return result;
}

static int write_array_file(const char *path, const void *items,
                            size_t item_size, int count) {
    FILE *fp;

    if (!path || item_size == 0 || count < 0 || (count > 0 && !items))
        return -1;
    fp = fopen(path, "wb");
    if (!fp)
        return -1;
    if (count > 0 &&
        fwrite(items, item_size, (size_t)count, fp) != (size_t)count) {
        fclose(fp);
        return -1;
    }
    return finish_write(fp);
}

int db_init(const char *data_dir) {
    char *img_dir, *next_id_path, *meta_path, *feat_path;
    FILE *fp;

    if (!data_dir || ensure_dir(data_dir) != 0)
        return -1;

    img_dir = make_path(data_dir, IMAGES_DIR);
    if (!img_dir) return -1;
    if (ensure_dir(img_dir) != 0) {
        free(img_dir);
        return -1;
    }
    free(img_dir);

    /* Ensure output/ directory */
    if (ensure_dir("output") != 0)
        return -1;

    /* Initialize .next_id if not exists */
    next_id_path = make_path(data_dir, NEXT_ID_FILE);
    if (!next_id_path) return -1;
    fp = fopen(next_id_path, "r");
    if (!fp) {
        fp = fopen(next_id_path, "w");
        if (!fp) {
            free(next_id_path);
            return -1;
        }
        if (fprintf(fp, "1\n") < 0) {
            fclose(fp);
            free(next_id_path);
            return -1;
        }
        if (fclose(fp) != 0) {
            free(next_id_path);
            return -1;
        }
    } else {
        fclose(fp);
    }
    free(next_id_path);

    /* Initialize metadata.dat if not exists */
    meta_path = make_path(data_dir, METADATA_FILE);
    if (!meta_path) return -1;
    fp = fopen(meta_path, "r");
    if (!fp) {
        fp = fopen(meta_path, "wb");
        if (!fp) {
            free(meta_path);
            return -1;
        }
        fclose(fp);
    } else {
        fclose(fp);
    }
    free(meta_path);

    /* Initialize features.dat if not exists */
    feat_path = make_path(data_dir, FEATURES_FILE);
    if (!feat_path) return -1;
    fp = fopen(feat_path, "r");
    if (!fp) {
        fp = fopen(feat_path, "wb");
        if (!fp) {
            free(feat_path);
            return -1;
        }
        fclose(fp);
    } else {
        fclose(fp);
    }
    free(feat_path);

    return 0;
}

int db_next_id(const char *data_dir) {
    char *path, *tmp_path;
    FILE *fp;
    char line[64];
    char *end;
    long value;
    int result = -1;

    path = make_path(data_dir, NEXT_ID_FILE);
    if (!path) return -1;
    tmp_path = make_path(data_dir, "/.next_id.tmp");
    if (!tmp_path) {
        free(path);
        return -1;
    }

    fp = fopen(path, "r");
    if (!fp) {
        goto cleanup;
    }

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        fp = NULL;
        goto cleanup;
    }
    if (fclose(fp) != 0) {
        fp = NULL;
        goto cleanup;
    }
    fp = NULL;

    errno = 0;
    value = strtol(line, &end, 10);
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
        end++;
    if (errno == ERANGE || end == line || *end != '\0' ||
        value <= 0 || value >= (long)INT_MAX)
        goto cleanup;

    fp = fopen(tmp_path, "w");
    if (!fp)
        goto cleanup;
    {
        int write_failed = fprintf(fp, "%ld\n", value + 1) < 0;
        if (finish_write(fp) != 0)
            write_failed = 1;
        fp = NULL;
        if (write_failed) {
            unlink(tmp_path);
            goto cleanup;
        }
    }
    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        goto cleanup;
    }
    result = (int)value;

cleanup:
    if (fp)
        fclose(fp);
    free(path);
    free(tmp_path);
    return result;
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
    char *path;
    FILE *fp;
    long fsize;
    int n;

    if (!data_dir || !records || !count)
        return -1;

    *records = NULL;
    *count = 0;

    path = make_path(data_dir, METADATA_FILE);
    if (!path) return -1;

    fp = fopen(path, "rb");
    if (!fp) {
        free(path);
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
        { fclose(fp); free(path); return -1; }
    fsize = ftell(fp);
    if (fsize < 0) { fclose(fp); free(path); return -1; }
    rewind(fp);

    if ((size_t)fsize % sizeof(image_record_t) != 0)
        { fclose(fp); free(path); return -1; }

    if (fsize == 0) {
        if (fclose(fp) != 0) { free(path); return -1; }
        free(path);
        return 0;
    }

    if ((unsigned long)fsize / sizeof(image_record_t) > (unsigned long)INT_MAX)
        { fclose(fp); free(path); return -1; }
    n = (int)((unsigned long)fsize / sizeof(image_record_t));
    *records = malloc((size_t)fsize);
    if (!*records) { fclose(fp); free(path); return -1; }

    if (fread(*records, sizeof(image_record_t), (size_t)n, fp) != (size_t)n) {
        free(*records); *records = NULL;
        fclose(fp); free(path); return -1;
    }

    for (int i = 0; i < n; i++) {
        if (!memchr((*records)[i].name, '\0', MAX_NAME_LEN) ||
            !memchr((*records)[i].path, '\0', MAX_PATH_LEN)) {
            free(*records); *records = NULL;
            fclose(fp); free(path); return -1;
        }
    }

    *count = n;
    if (fclose(fp) != 0) { free(*records); *records = NULL; *count = 0; free(path); return -1; }
    free(path);
    return 0;
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
    char *path;
    FILE *fp;
    long fsize;
    int n;

    if (!data_dir || !features || !count)
        return -1;

    *features = NULL;
    *count = 0;

    path = make_path(data_dir, FEATURES_FILE);
    if (!path) return -1;

    fp = fopen(path, "rb");
    if (!fp) {
        free(path);
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
        { fclose(fp); free(path); return -1; }
    fsize = ftell(fp);
    if (fsize < 0) { fclose(fp); free(path); return -1; }
    rewind(fp);

    if ((size_t)fsize % sizeof(image_feature_t) != 0)
        { fclose(fp); free(path); return -1; }

    if (fsize == 0) {
        if (fclose(fp) != 0) { free(path); return -1; }
        free(path);
        return 0;
    }

    if ((unsigned long)fsize / sizeof(image_feature_t) > (unsigned long)INT_MAX)
        { fclose(fp); free(path); return -1; }
    n = (int)((unsigned long)fsize / sizeof(image_feature_t));
    *features = malloc((size_t)fsize);
    if (!*features) { fclose(fp); free(path); return -1; }

    if (fread(*features, sizeof(image_feature_t), (size_t)n, fp) != (size_t)n) {
        free(*features); *features = NULL;
        fclose(fp); free(path); return -1;
    }

    *count = n;
    if (fclose(fp) != 0) { free(*features); *features = NULL; *count = 0; free(path); return -1; }
    free(path);
    return 0;
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
    char *tmp_path, *real_path;

    if (!data_dir || count < 0 || (count > 0 && !records))
        return -1;

    tmp_path = make_path(data_dir, "/metadata.tmp");
    real_path = make_path(data_dir, METADATA_FILE);
    if (!tmp_path || !real_path) { free(tmp_path); free(real_path); return -1; }

    if (write_array_file(tmp_path, records, sizeof(image_record_t), count) != 0)
        { unlink(tmp_path); free(tmp_path); free(real_path); return -1; }

    if (rename(tmp_path, real_path) != 0)
        { unlink(tmp_path); free(tmp_path); free(real_path); return -1; }
    free(tmp_path); free(real_path);
    return 0;
}

int db_write_features(const char *data_dir, const image_feature_t *features, int count) {
    char *tmp_path, *real_path;

    if (!data_dir || count < 0 || (count > 0 && !features))
        return -1;

    tmp_path = make_path(data_dir, "/features.tmp");
    real_path = make_path(data_dir, FEATURES_FILE);
    if (!tmp_path || !real_path) { free(tmp_path); free(real_path); return -1; }

    if (write_array_file(tmp_path, features, sizeof(image_feature_t), count) != 0)
        { unlink(tmp_path); free(tmp_path); free(real_path); return -1; }

    if (rename(tmp_path, real_path) != 0)
        { unlink(tmp_path); free(tmp_path); free(real_path); return -1; }
    free(tmp_path); free(real_path);
    return 0;
}

int db_replace_store(const char *data_dir,
                     const image_record_t *records, int record_count,
                     const image_feature_t *features, int feature_count) {
    char *tmp_meta = NULL, *tmp_feat = NULL;
    char *bak_meta = NULL, *bak_feat = NULL;
    char *real_meta = NULL, *real_feat = NULL;
    int meta_backed_up = 0, feat_backed_up = 0;
    int meta_installed = 0, feat_installed = 0;
    int result = -1;

    if (!data_dir || record_count < 0 || feature_count < 0 ||
        (record_count > 0 && !records) || (feature_count > 0 && !features))
        return -1;

    tmp_meta = make_path(data_dir, "/metadata.tmp");
    tmp_feat = make_path(data_dir, "/features.tmp");
    bak_meta = make_path(data_dir, "/metadata.bak");
    bak_feat = make_path(data_dir, "/features.bak");
    real_meta = make_path(data_dir, METADATA_FILE);
    real_feat = make_path(data_dir, FEATURES_FILE);
    if (!tmp_meta || !tmp_feat || !bak_meta || !bak_feat ||
        !real_meta || !real_feat)
        goto cleanup;

    if (write_array_file(tmp_meta, records, sizeof(image_record_t),
                         record_count) != 0 ||
        write_array_file(tmp_feat, features, sizeof(image_feature_t),
                         feature_count) != 0)
        goto cleanup;

    if (rename(real_meta, bak_meta) != 0)
        goto cleanup;
    meta_backed_up = 1;
    if (rename(real_feat, bak_feat) != 0)
        goto rollback;
    feat_backed_up = 1;

    if (rename(tmp_meta, real_meta) != 0)
        goto rollback;
    meta_installed = 1;
    if (rename(tmp_feat, real_feat) != 0)
        goto rollback;
    feat_installed = 1;

    unlink(bak_meta);
    unlink(bak_feat);
    meta_backed_up = 0;
    feat_backed_up = 0;
    result = 0;
    goto cleanup;

rollback:
    if (meta_installed && unlink(real_meta) == 0)
        meta_installed = 0;
    if (feat_installed && unlink(real_feat) == 0)
        feat_installed = 0;
    if (meta_backed_up && rename(bak_meta, real_meta) == 0)
        meta_backed_up = 0;
    if (feat_backed_up && rename(bak_feat, real_feat) == 0)
        feat_backed_up = 0;

cleanup:
    if (tmp_meta)
        unlink(tmp_meta);
    if (tmp_feat)
        unlink(tmp_feat);
    /* If rollback itself failed, retain .bak as the recoverable original. */
    free(tmp_meta); free(tmp_feat);
    free(bak_meta); free(bak_feat);
    free(real_meta); free(real_feat);
    return result;
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
