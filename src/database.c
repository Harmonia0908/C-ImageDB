#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "database.h"

#define METADATA_FILE   "/metadata.dat"
#define FEATURES_FILE   "/features.dat"
#define NEXT_ID_FILE    "/.next_id"
#define IMAGES_DIR      "/images"

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0)
        return 0;
    if (mkdir(path, 0755) != 0)
        return -1;
    return 0;
}

static char *make_path(const char *data_dir, const char *suffix) {
    char *path;
    size_t len;

    len = strlen(data_dir) + strlen(suffix) + 1;
    path = malloc(len);
    if (!path)
        return NULL;

    snprintf(path, len, "%s%s", data_dir, suffix);
    return path;
}

int db_init(const char *data_dir) {
    char *img_dir, *next_id_path, *meta_path, *feat_path;
    FILE *fp;

    if (ensure_dir(data_dir) != 0)
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
        fprintf(fp, "1\n");
        fclose(fp);
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
    char *path;
    FILE *fp;
    int id;

    path = make_path(data_dir, NEXT_ID_FILE);
    if (!path) return -1;

    fp = fopen(path, "r+");
    if (!fp) {
        free(path);
        return -1;
    }

    if (fscanf(fp, "%d", &id) != 1) {
        fclose(fp);
        free(path);
        return -1;
    }

    rewind(fp);
    fprintf(fp, "%d\n", id + 1);
    fclose(fp);
    free(path);
    return id;
}

int db_add_record(const char *data_dir, const image_record_t *record) {
    char *path;
    FILE *fp;

    path = make_path(data_dir, METADATA_FILE);
    if (!path) return -1;

    fp = fopen(path, "ab");
    if (!fp) {
        free(path);
        return -1;
    }

    if (fwrite(record, sizeof(image_record_t), 1, fp) != 1) {
        fclose(fp);
        free(path);
        return -1;
    }

    fclose(fp);
    free(path);
    return 0;
}

int db_load_records(const char *data_dir, image_record_t **records, int *count) {
    char *path;
    FILE *fp;
    long fsize;
    int n;

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

    if (fsize % sizeof(image_record_t) != 0)
        { fclose(fp); free(path); return -1; }

    if (fsize == 0) {
        if (fclose(fp) != 0) { free(path); return -1; }
        free(path);
        return 0;
    }

    n = (int)(fsize / sizeof(image_record_t));
    *records = malloc((size_t)fsize);
    if (!*records) { fclose(fp); free(path); return -1; }

    if (fread(*records, sizeof(image_record_t), (size_t)n, fp) != (size_t)n) {
        free(*records); *records = NULL;
        fclose(fp); free(path); return -1;
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

    if (db_load_records(data_dir, &records, &count) != 0)
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
    FILE *fp;

    if (db_load_records(data_dir, &records, &count) != 0)
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

    /* Write to temp, then rename to avoid truncation on failure */
    {
        char *tmp_path, *real_path, *bak_path;
        int ok = 0;

        tmp_path = make_path(data_dir, "/metadata.tmp");
        real_path = make_path(data_dir, METADATA_FILE);
        bak_path = make_path(data_dir, "/metadata.bak");
        if (!tmp_path || !real_path || !bak_path) {
            free(tmp_path); free(real_path); free(bak_path);
            free(records); return -1;
        }

        fp = fopen(tmp_path, "wb");
        if (!fp) goto del_cleanup;

        if (count > 0) {
            if (fwrite(records, sizeof(image_record_t), (size_t)count, fp) != (size_t)count)
                { fclose(fp); goto del_cleanup; }
        }
        fflush(fp);
        if (fclose(fp) != 0) goto del_cleanup;

        if (rename(real_path, bak_path) != 0) goto del_cleanup;
        if (rename(tmp_path, real_path) != 0) {
            rename(bak_path, real_path);
            goto del_cleanup;
        }
        unlink(bak_path);
        ok = 1;

    del_cleanup:
        if (!ok && tmp_path) unlink(tmp_path);
        free(tmp_path); free(real_path); free(bak_path);
        free(records);
        return ok ? 0 : -1;
    }
}

int db_add_feature(const char *data_dir, const image_feature_t *feature) {
    char *path;
    FILE *fp;

    path = make_path(data_dir, FEATURES_FILE);
    if (!path) return -1;

    fp = fopen(path, "ab");
    if (!fp) {
        free(path);
        return -1;
    }

    if (fwrite(feature, sizeof(image_feature_t), 1, fp) != 1) {
        fclose(fp);
        free(path);
        return -1;
    }

    fclose(fp);
    free(path);
    return 0;
}

int db_load_features(const char *data_dir, image_feature_t **features, int *count) {
    char *path;
    FILE *fp;
    long fsize;
    int n;

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

    if (fsize % sizeof(image_feature_t) != 0)
        { fclose(fp); free(path); return -1; }

    if (fsize == 0) {
        if (fclose(fp) != 0) { free(path); return -1; }
        free(path);
        return 0;
    }

    n = (int)(fsize / sizeof(image_feature_t));
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

    if (db_load_features(data_dir, &features, &count) != 0)
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
    FILE *fp;

    tmp_path = make_path(data_dir, "/metadata.tmp");
    real_path = make_path(data_dir, METADATA_FILE);
    if (!tmp_path || !real_path) { free(tmp_path); free(real_path); return -1; }

    fp = fopen(tmp_path, "wb");
    if (!fp) { free(tmp_path); free(real_path); return -1; }

    if (count > 0) {
        if (fwrite(records, sizeof(image_record_t), (size_t)count, fp) != (size_t)count)
            { fclose(fp); unlink(tmp_path); free(tmp_path); free(real_path); return -1; }
    }
    fflush(fp);
    if (fclose(fp) != 0) { unlink(tmp_path); free(tmp_path); free(real_path); return -1; }

    if (rename(tmp_path, real_path) != 0)
        { unlink(tmp_path); free(tmp_path); free(real_path); return -1; }
    free(tmp_path); free(real_path);
    return 0;
}

int db_write_features(const char *data_dir, const image_feature_t *features, int count) {
    char *tmp_path, *real_path;
    FILE *fp;

    tmp_path = make_path(data_dir, "/features.tmp");
    real_path = make_path(data_dir, FEATURES_FILE);
    if (!tmp_path || !real_path) { free(tmp_path); free(real_path); return -1; }

    fp = fopen(tmp_path, "wb");
    if (!fp) { free(tmp_path); free(real_path); return -1; }

    if (count > 0) {
        if (fwrite(features, sizeof(image_feature_t), (size_t)count, fp) != (size_t)count)
            { fclose(fp); unlink(tmp_path); free(tmp_path); free(real_path); return -1; }
    }
    fflush(fp);
    if (fclose(fp) != 0) { unlink(tmp_path); free(tmp_path); free(real_path); return -1; }

    if (rename(tmp_path, real_path) != 0)
        { unlink(tmp_path); free(tmp_path); free(real_path); return -1; }
    free(tmp_path); free(real_path);
    return 0;
}

int db_commit_import(const char *data_dir,
                     const image_record_t *record,
                     const image_feature_t *feature) {
    image_record_t *records = NULL;
    image_feature_t *features = NULL;
    int rec_count = 0, feat_count = 0;
    char *tmp_meta = NULL, *tmp_feat = NULL;
    char *bak_meta = NULL, *bak_feat = NULL;
    char *real_meta = NULL, *real_feat = NULL;
    FILE *fp;
    int result = -1;
    int meta_renamed = 0, feat_renamed = 0;

    if (db_load_records(data_dir, &records, &rec_count) != 0)
        return -1;
    if (db_load_features(data_dir, &features, &feat_count) != 0) {
        free(records);
        return -1;
    }

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

    tmp_meta = make_path(data_dir, "/metadata.tmp");
    tmp_feat = make_path(data_dir, "/features.tmp");
    bak_meta = make_path(data_dir, "/metadata.bak");
    bak_feat = make_path(data_dir, "/features.bak");
    real_meta = make_path(data_dir, METADATA_FILE);
    real_feat = make_path(data_dir, FEATURES_FILE);
    if (!tmp_meta || !tmp_feat || !bak_meta || !bak_feat ||
        !real_meta || !real_feat)
        goto cleanup;

    fp = fopen(tmp_meta, "wb");
    if (!fp)
        goto cleanup;
    if (fwrite(records, sizeof(image_record_t), (size_t)rec_count, fp)
        != (size_t)rec_count) {
        fclose(fp);
        goto cleanup;
    }
    fflush(fp);
    if (fclose(fp) != 0)
        goto cleanup;

    fp = fopen(tmp_feat, "wb");
    if (!fp)
        goto cleanup;
    if (fwrite(features, sizeof(image_feature_t), (size_t)feat_count, fp)
        != (size_t)feat_count) {
        fclose(fp);
        goto cleanup;
    }
    fflush(fp);
    if (fclose(fp) != 0)
        goto cleanup;

    if (rename(real_meta, bak_meta) != 0)
        goto cleanup;
    meta_renamed = 1;
    if (rename(real_feat, bak_feat) != 0)
        goto cleanup;
    feat_renamed = 1;
    if (rename(tmp_meta, real_meta) != 0)
        goto rollback;
    free(tmp_meta);
    tmp_meta = NULL;
    if (rename(tmp_feat, real_feat) != 0)
        goto rollback;
    free(tmp_feat);
    tmp_feat = NULL;

    unlink(bak_meta);
    meta_renamed = 0;
    unlink(bak_feat);
    feat_renamed = 0;
    result = 0;
    goto cleanup;

rollback:
    if (tmp_meta) {
        unlink(tmp_meta);
        free(tmp_meta);
        tmp_meta = NULL;
    }
    if (tmp_feat) {
        unlink(tmp_feat);
        free(tmp_feat);
        tmp_feat = NULL;
    }
    if (meta_renamed) {
        rename(bak_meta, real_meta);
        meta_renamed = 0;
    }
    if (feat_renamed) {
        rename(bak_feat, real_feat);
        feat_renamed = 0;
    }

cleanup:
    free(records);
    free(features);
    if (tmp_meta) {
        unlink(tmp_meta);
        free(tmp_meta);
    }
    if (tmp_feat) {
        unlink(tmp_feat);
        free(tmp_feat);
    }
    if (meta_renamed) {
        rename(bak_meta, real_meta);
        unlink(bak_meta);
    }
    if (feat_renamed) {
        rename(bak_feat, real_feat);
        unlink(bak_feat);
    }
    free(bak_meta); free(bak_feat); free(real_meta); free(real_feat);
    return result;
}

int db_compact(const char *data_dir, int *before_count, int *after_count) {
    image_record_t *records = NULL;
    image_feature_t *features = NULL;
    image_record_t *keep_records = NULL;
    image_feature_t *keep_features = NULL;
    int rec_count = 0, feat_count = 0;
    int keep_rec = 0, keep_feat = 0;
    char *tmp_meta = NULL, *tmp_feat = NULL;
    char *bak_meta = NULL, *bak_feat = NULL;
    char *real_meta = NULL, *real_feat = NULL;
    FILE *fp;
    int i, j;
    int *keep_id = NULL;
    int keep_id_count = 0;
    int result = -1;
    int meta_renamed = 0, feat_renamed = 0;

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

    tmp_meta = make_path(data_dir, "/metadata.tmp");
    tmp_feat = make_path(data_dir, "/features.tmp");
    bak_meta = make_path(data_dir, "/metadata.bak");
    bak_feat = make_path(data_dir, "/features.bak");
    real_meta = make_path(data_dir, METADATA_FILE);
    real_feat = make_path(data_dir, FEATURES_FILE);
    if (!tmp_meta || !tmp_feat || !bak_meta || !bak_feat || !real_meta || !real_feat)
        goto cleanup;

    /* Write metadata.tmp */
    fp = fopen(tmp_meta, "wb");
    if (!fp) goto cleanup;
    if (keep_rec > 0) {
        if (fwrite(keep_records, sizeof(image_record_t), (size_t)keep_rec, fp) != (size_t)keep_rec)
            { fclose(fp); goto cleanup; }
    }
    fflush(fp);
    if (fclose(fp) != 0) goto cleanup;

    /* Write features.tmp */
    fp = fopen(tmp_feat, "wb");
    if (!fp) goto cleanup;
    if (keep_feat > 0) {
        if (fwrite(keep_features, sizeof(image_feature_t), (size_t)keep_feat, fp) != (size_t)keep_feat)
            { fclose(fp); goto cleanup; }
    }
    fflush(fp);
    if (fclose(fp) != 0) goto cleanup;

    /* Phase 1: backup originals as .bak */
    if (rename(real_meta, bak_meta) != 0) goto cleanup;
    meta_renamed = 1;
    if (rename(real_feat, bak_feat) != 0) goto cleanup;
    feat_renamed = 1;

    /* Phase 2: install new files */
    if (rename(tmp_meta, real_meta) != 0) goto rollback;
    free(tmp_meta); tmp_meta = NULL;

    if (rename(tmp_feat, real_feat) != 0) goto rollback;
    free(tmp_feat); tmp_feat = NULL;

    /* Phase 3: remove backups on success */
    unlink(bak_meta);
    unlink(bak_feat);

    meta_renamed = 0;
    feat_renamed = 0;

    if (after_count) *after_count = keep_rec;
    result = 0;
    goto cleanup;

rollback:
    /* Restore originals from backups */
    if (tmp_meta) { unlink(tmp_meta); free(tmp_meta); tmp_meta = NULL; }
    if (tmp_feat) { unlink(tmp_feat); free(tmp_feat); tmp_feat = NULL; }
    if (meta_renamed) { rename(bak_meta, real_meta); meta_renamed = 0; }
    if (feat_renamed) { rename(bak_feat, real_feat); feat_renamed = 0; }
    result = -1;

cleanup:
    free(records);
    free(features);
    free(keep_records);
    free(keep_features);
    free(keep_id);
    if (tmp_meta) { unlink(tmp_meta); free(tmp_meta); }
    if (tmp_feat) { unlink(tmp_feat); free(tmp_feat); }
    if (meta_renamed) { rename(bak_meta, real_meta); unlink(bak_meta); }
    if (feat_renamed) { rename(bak_feat, real_feat); unlink(bak_feat); }
    free(bak_meta);
    free(bak_feat);
    free(real_meta);
    free(real_feat);
    return result;
}
