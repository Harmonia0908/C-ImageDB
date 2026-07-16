#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "store_file.h"

#define METADATA_FILE   "/metadata.dat"
#define FEATURES_FILE   "/features.dat"
#define NEXT_ID_FILE    "/.next_id"
#define NEXT_ID_TMP     "/.next_id.tmp"
#define IMAGES_DIR      "/images"
#define METADATA_TMP    "/metadata.tmp"
#define FEATURES_TMP    "/features.tmp"
#define METADATA_BAK    "/metadata.bak"
#define FEATURES_BAK    "/features.bak"

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

    if (!data_dir || !*data_dir || !suffix)
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

static int record_path_is_store_local(const char *data_dir,
                                      const image_record_t *record) {
    char base[MAX_PATH_LEN];
    const char *separator;
    const char *suffix;
    size_t dir_length;
    int length;

    if (!data_dir || !*data_dir || !record || record->id <= 0)
        return 0;
    dir_length = strlen(data_dir);
    separator = data_dir[dir_length - 1] == '/' ? "" : "/";
    length = snprintf(base, sizeof(base), "%s%simages/%d",
                      data_dir, separator, record->id);
    if (length < 0 || (size_t)length >= sizeof(base) ||
        strncmp(record->path, base, (size_t)length) != 0)
        return 0;

    suffix = record->path + length;
    return *suffix == '\0' ||
           (*suffix == '.' && strchr(suffix, '/') == NULL);
}

static int records_are_safe(const char *data_dir,
                            const image_record_t *records, int count) {
    int i;

    if (!data_dir || !*data_dir || count < 0 || (count > 0 && !records))
        return 0;
    for (i = 0; i < count; i++) {
        if (!memchr(records[i].name, '\0', MAX_NAME_LEN) ||
            !memchr(records[i].path, '\0', MAX_PATH_LEN) ||
            !record_path_is_store_local(data_dir, &records[i]))
            return 0;
    }
    return 1;
}

static int finish_write(FILE *fp) {
    int result = 0;

    if (fflush(fp) != 0)
        result = -1;
    if (fclose(fp) != 0)
        result = -1;
    return result;
}

int store_file_init(const char *data_dir) {
    char *img_dir;
    char *next_id_path;
    char *meta_path;
    char *feat_path;
    FILE *fp;

    if (!data_dir || ensure_dir(data_dir) != 0)
        return -1;

    img_dir = make_path(data_dir, IMAGES_DIR);
    if (!img_dir)
        return -1;
    if (ensure_dir(img_dir) != 0) {
        free(img_dir);
        return -1;
    }
    free(img_dir);

    if (ensure_dir("output") != 0)
        return -1;

    next_id_path = make_path(data_dir, NEXT_ID_FILE);
    if (!next_id_path)
        return -1;
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

    meta_path = make_path(data_dir, METADATA_FILE);
    if (!meta_path)
        return -1;
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

    feat_path = make_path(data_dir, FEATURES_FILE);
    if (!feat_path)
        return -1;
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

int store_file_next_id(const char *data_dir, int *out_id) {
    char *path;
    char *tmp_path;
    FILE *fp = NULL;
    char line[64];
    char *end;
    long value;
    int result = -1;

    if (!out_id)
        return -1;
    *out_id = -1;

    path = make_path(data_dir, NEXT_ID_FILE);
    if (!path)
        return -1;
    tmp_path = make_path(data_dir, NEXT_ID_TMP);
    if (!tmp_path) {
        free(path);
        return -1;
    }

    fp = fopen(path, "r");
    if (!fp)
        goto cleanup;

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
    *out_id = (int)value;
    result = 0;

cleanup:
    if (fp)
        fclose(fp);
    free(path);
    free(tmp_path);
    return result;
}

static int read_array(const char *path, size_t item_size,
                      void **items, int *count) {
    FILE *fp;
    void *loaded_items = NULL;
    long file_size;
    size_t byte_size;
    int item_count;

    if (!items || !count)
        return -1;

    *items = NULL;
    *count = 0;

    if (!path || item_size == 0)
        return -1;

    fp = fopen(path, "rb");
    if (!fp)
        return -1;

    if (fseek(fp, 0, SEEK_END) != 0)
        goto fail;
    file_size = ftell(fp);
    if (file_size < 0)
        goto fail;
    rewind(fp);

    byte_size = (size_t)file_size;
    if (byte_size % item_size != 0)
        goto fail;
    if (byte_size == 0)
        return fclose(fp) == 0 ? 0 : -1;
    if (byte_size / item_size > (size_t)INT_MAX)
        goto fail;

    item_count = (int)(byte_size / item_size);
    loaded_items = malloc(byte_size);
    if (!loaded_items)
        goto fail;
    if (fread(loaded_items, item_size, (size_t)item_count, fp) !=
        (size_t)item_count)
        goto fail;
    if (fclose(fp) != 0) {
        free(loaded_items);
        return -1;
    }

    *items = loaded_items;
    *count = item_count;
    return 0;

fail:
    free(loaded_items);
    fclose(fp);
    return -1;
}

static int write_array(const char *path, const void *items,
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

int store_file_load_records(const char *data_dir,
                            image_record_t **records, int *count) {
    char *path;
    void *items = NULL;
    int loaded_count = 0;

    if (!data_dir || !records || !count)
        return -1;
    *records = NULL;
    *count = 0;

    path = make_path(data_dir, METADATA_FILE);
    if (!path)
        return -1;
    if (read_array(path, sizeof(image_record_t),
                   &items, &loaded_count) != 0) {
        free(path);
        return -1;
    }
    free(path);

    if (!records_are_safe(data_dir, items, loaded_count)) {
        free(items);
        return -1;
    }

    *records = items;
    *count = loaded_count;
    return 0;
}

int store_file_load_features(const char *data_dir,
                             image_feature_t **features, int *count) {
    char *path;
    void *items = NULL;
    int loaded_count = 0;

    if (!data_dir || !features || !count)
        return -1;
    *features = NULL;
    *count = 0;

    path = make_path(data_dir, FEATURES_FILE);
    if (!path)
        return -1;
    if (read_array(path, sizeof(image_feature_t),
                   &items, &loaded_count) != 0) {
        free(path);
        return -1;
    }
    free(path);

    *features = items;
    *count = loaded_count;
    return 0;
}

int store_file_replace_records(const char *data_dir,
                               const image_record_t *records, int count) {
    char *tmp_path;
    char *real_path;
    int result = -1;

    if (!records_are_safe(data_dir, records, count))
        return -1;

    tmp_path = make_path(data_dir, METADATA_TMP);
    real_path = make_path(data_dir, METADATA_FILE);
    if (!tmp_path || !real_path)
        goto cleanup;
    if (write_array(tmp_path, records,
                    sizeof(image_record_t), count) != 0) {
        unlink(tmp_path);
        goto cleanup;
    }
    if (rename(tmp_path, real_path) != 0) {
        unlink(tmp_path);
        goto cleanup;
    }
    result = 0;

cleanup:
    free(tmp_path);
    free(real_path);
    return result;
}

int store_file_replace_features(const char *data_dir,
                                const image_feature_t *features, int count) {
    char *tmp_path;
    char *real_path;
    int result = -1;

    if (!data_dir || count < 0 || (count > 0 && !features))
        return -1;

    tmp_path = make_path(data_dir, FEATURES_TMP);
    real_path = make_path(data_dir, FEATURES_FILE);
    if (!tmp_path || !real_path)
        goto cleanup;
    if (write_array(tmp_path, features,
                    sizeof(image_feature_t), count) != 0) {
        unlink(tmp_path);
        goto cleanup;
    }
    if (rename(tmp_path, real_path) != 0) {
        unlink(tmp_path);
        goto cleanup;
    }
    result = 0;

cleanup:
    free(tmp_path);
    free(real_path);
    return result;
}

int store_file_replace_store(const char *data_dir,
                             const image_record_t *records, int record_count,
                             const image_feature_t *features,
                             int feature_count) {
    char *tmp_meta = NULL;
    char *tmp_feat = NULL;
    char *bak_meta = NULL;
    char *bak_feat = NULL;
    char *real_meta = NULL;
    char *real_feat = NULL;
    int meta_backed_up = 0;
    int feat_backed_up = 0;
    int meta_installed = 0;
    int feat_installed = 0;
    int result = -1;

    if (!records_are_safe(data_dir, records, record_count) ||
        feature_count < 0 || (feature_count > 0 && !features))
        return -1;

    tmp_meta = make_path(data_dir, METADATA_TMP);
    tmp_feat = make_path(data_dir, FEATURES_TMP);
    bak_meta = make_path(data_dir, METADATA_BAK);
    bak_feat = make_path(data_dir, FEATURES_BAK);
    real_meta = make_path(data_dir, METADATA_FILE);
    real_feat = make_path(data_dir, FEATURES_FILE);
    if (!tmp_meta || !tmp_feat || !bak_meta || !bak_feat ||
        !real_meta || !real_feat)
        goto cleanup;

    if (write_array(tmp_meta, records, sizeof(image_record_t),
                    record_count) != 0 ||
        write_array(tmp_feat, features, sizeof(image_feature_t),
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
    free(tmp_meta);
    free(tmp_feat);
    free(bak_meta);
    free(bak_feat);
    free(real_meta);
    free(real_feat);
    return result;
}
