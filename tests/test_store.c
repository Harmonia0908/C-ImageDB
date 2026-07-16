#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "database.h"

static int failures;

#define CHECK(condition) do {                                                   \
    if (!(condition)) {                                                        \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++;                                                            \
    }                                                                          \
} while (0)

static int make_temp_root(char *path, size_t path_size) {
    int attempt;

    for (attempt = 0; attempt < 100; attempt++) {
        int length = snprintf(path, path_size,
                              "/tmp/cimagedb_store_%ld_%d",
                              (long)getpid(), attempt);
        if (length < 0 || (size_t)length >= path_size)
            return -1;
        if (mkdir(path, 0700) == 0)
            return 0;
        if (errno != EEXIST)
            return -1;
    }
    return -1;
}

static int file_matches(const char *path, const void *expected,
                        size_t expected_size) {
    FILE *fp;
    unsigned char *actual = NULL;
    long file_size;
    int matches = 0;

    fp = fopen(path, "rb");
    if (!fp)
        return 0;
    if (fseek(fp, 0, SEEK_END) != 0)
        goto cleanup;
    file_size = ftell(fp);
    if (file_size < 0 || (size_t)file_size != expected_size)
        goto cleanup;
    rewind(fp);

    if (expected_size > 0) {
        actual = malloc(expected_size);
        if (!actual)
            goto cleanup;
        if (fread(actual, 1, expected_size, fp) != expected_size)
            goto cleanup;
        if (memcmp(actual, expected, expected_size) != 0)
            goto cleanup;
    }
    matches = 1;

cleanup:
    free(actual);
    if (fclose(fp) != 0)
        matches = 0;
    return matches;
}

static int append_byte(const char *path) {
    FILE *fp = fopen(path, "ab");
    int write_failed;

    if (!fp)
        return -1;
    write_failed = fputc(0x5a, fp) == EOF;
    if (fclose(fp) != 0)
        write_failed = 1;
    return write_failed ? -1 : 0;
}

static int write_bytes(const char *path, const void *data, size_t size) {
    FILE *fp = fopen(path, "wb");
    int write_failed = 0;

    if (!fp)
        return -1;
    if (size > 0 && fwrite(data, 1, size, fp) != size)
        write_failed = 1;
    if (fclose(fp) != 0)
        write_failed = 1;
    return write_failed ? -1 : 0;
}

static int path_is_directory(const char *path) {
    struct stat st;

    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void remove_fixture_tree(void) {
    rmdir("data/metadata.tmp");
    rmdir("data/features.tmp");
    rmdir("data/metadata.bak");
    rmdir("data/features.bak");
    rmdir("data/metadata.dat");
    rmdir("data/features.dat");
    rmdir("data/.next_id.tmp");
    rmdir("data/.next_id");
    unlink("data/metadata.tmp");
    unlink("data/features.tmp");
    unlink("data/metadata.bak");
    unlink("data/features.bak");
    unlink("data/metadata.dat");
    unlink("data/features.dat");
    unlink("data/.next_id");
    rmdir("data/images");
    rmdir("data");
    rmdir("output");
}

static void populate_records(image_record_t records[2]) {
    memset(records, 0, 2 * sizeof(*records));

    records[0].id = 7;
    snprintf(records[0].name, sizeof(records[0].name), "alpha.ppm");
    snprintf(records[0].path, sizeof(records[0].path),
             "data/images/7.ppm");
    records[0].width = 3;
    records[0].height = 5;
    records[0].channels = 3;
    records[0].file_size = 1234;
    records[0].import_time = 1700000000L;
    records[0].content_hash = UINT64_C(0x0123456789abcdef);

    records[1].id = 8;
    snprintf(records[1].name, sizeof(records[1].name), "beta.bmp");
    snprintf(records[1].path, sizeof(records[1].path),
             "data/images/8.bmp");
    records[1].width = 11;
    records[1].height = 13;
    records[1].channels = 3;
    records[1].file_size = 5678;
    records[1].import_time = 1700000100L;
    records[1].content_hash = UINT64_C(0xfedcba9876543210);
    records[1].deleted = 1;
}

static void populate_features(image_feature_t features[2]) {
    int i;

    memset(features, 0, 2 * sizeof(*features));
    features[0].image_id = 7;
    features[1].image_id = 8;
    for (i = 0; i < 256; i++) {
        features[0].r_hist[i] = i;
        features[0].g_hist[i] = 255 - i;
        features[0].b_hist[i] = i % 7;
        features[1].r_hist[i] = i % 11;
        features[1].g_hist[i] = i % 13;
        features[1].b_hist[i] = i % 17;
    }
    features[0].avg_r = 12.5;
    features[0].avg_g = 34.5;
    features[0].avg_b = 56.5;
    features[1].avg_r = 78.25;
    features[1].avg_g = 90.25;
    features[1].avg_b = 123.25;
}

static void test_created_layout(void) {
    static const char initial_id[] = "1\n";

    CHECK(path_is_directory("data"));
    CHECK(path_is_directory("data/images"));
    CHECK(path_is_directory("output"));
    CHECK(file_matches("data/metadata.dat", NULL, 0));
    CHECK(file_matches("data/features.dat", NULL, 0));
    CHECK(file_matches("data/.next_id", initial_id,
                       sizeof(initial_id) - 1));
}

static void test_empty_store(void) {
    image_record_t record_sentinel;
    image_feature_t feature_sentinel;
    image_record_t *records = &record_sentinel;
    image_feature_t *features = &feature_sentinel;
    int record_count = -1;
    int feature_count = -1;

    CHECK(db_load_records("data", &records, &record_count) == 0);
    CHECK(records == NULL);
    CHECK(record_count == 0);
    CHECK(db_load_features("data", &features, &feature_count) == 0);
    CHECK(features == NULL);
    CHECK(feature_count == 0);
}

static void test_missing_and_unreadable_files(void) {
    image_record_t record_sentinel;
    image_feature_t feature_sentinel;
    image_record_t *records = &record_sentinel;
    image_feature_t *features = &feature_sentinel;
    int record_count = 41;
    int feature_count = 42;

    CHECK(unlink("data/metadata.dat") == 0);
    CHECK(db_load_records("data", &records, &record_count) == -1);
    CHECK(records == NULL);
    CHECK(record_count == 0);
    CHECK(db_init("data") == 0);

    CHECK(unlink("data/features.dat") == 0);
    CHECK(db_load_features("data", &features, &feature_count) == -1);
    CHECK(features == NULL);
    CHECK(feature_count == 0);
    CHECK(db_init("data") == 0);

    CHECK(unlink("data/metadata.dat") == 0);
    CHECK(mkdir("data/metadata.dat", 0700) == 0);
    records = &record_sentinel;
    record_count = 43;
    CHECK(db_load_records("data", &records, &record_count) == -1);
    CHECK(records == NULL);
    CHECK(record_count == 0);
    CHECK(rmdir("data/metadata.dat") == 0);
    CHECK(db_init("data") == 0);
}

static void test_next_id_persistence_and_failure(void) {
    static const char next_id_two[] = "2\n";

    CHECK(db_next_id("data") == 1);
    CHECK(file_matches("data/.next_id", next_id_two,
                       sizeof(next_id_two) - 1));
    CHECK(mkdir("data/.next_id.tmp", 0700) == 0);
    CHECK(db_next_id("data") == -1);
    CHECK(file_matches("data/.next_id", next_id_two,
                       sizeof(next_id_two) - 1));
    CHECK(rmdir("data/.next_id.tmp") == 0);
    CHECK(db_next_id("data") == 2);
}

static void test_round_trip_and_disk_layout(void) {
    image_record_t expected_records[2];
    image_feature_t expected_features[2];
    image_record_t *actual_records = NULL;
    image_feature_t *actual_features = NULL;
    int record_count = 0;
    int feature_count = 0;

    populate_records(expected_records);
    populate_features(expected_features);

    CHECK(db_write_records("data", expected_records, 2) == 0);
    CHECK(db_write_features("data", expected_features, 2) == 0);
    CHECK(file_matches("data/metadata.dat", expected_records,
                       sizeof(expected_records)));
    CHECK(file_matches("data/features.dat", expected_features,
                       sizeof(expected_features)));

    CHECK(db_load_records("data", &actual_records, &record_count) == 0);
    CHECK(record_count == 2);
    if (actual_records && record_count == 2)
        CHECK(memcmp(actual_records, expected_records,
                     sizeof(expected_records)) == 0);

    CHECK(db_load_features("data", &actual_features, &feature_count) == 0);
    CHECK(feature_count == 2);
    if (actual_features && feature_count == 2)
        CHECK(memcmp(actual_features, expected_features,
                     sizeof(expected_features)) == 0);

    free(actual_records);
    free(actual_features);
}

static void test_repeated_write_and_reinitialization(void) {
    image_record_t records[2];
    image_feature_t features[2];

    populate_records(records);
    populate_features(features);

    CHECK(db_write_records("data", records, 2) == 0);
    CHECK(db_write_records("data", records, 2) == 0);
    CHECK(db_write_features("data", features, 2) == 0);
    CHECK(db_write_features("data", features, 2) == 0);
    CHECK(db_init("data") == 0);
    CHECK(file_matches("data/metadata.dat", records, sizeof(records)));
    CHECK(file_matches("data/features.dat", features, sizeof(features)));
}

static void test_boundary_length_records(void) {
    image_record_t record;
    image_record_t *loaded = NULL;
    int count = 0;
    int prefix_length;

    memset(&record, 0, sizeof(record));
    record.id = 31;
    memset(record.name, 'n', sizeof(record.name) - 1);
    prefix_length = snprintf(record.path, sizeof(record.path),
                             "data/images/%d.", record.id);
    CHECK(prefix_length > 0);
    if (prefix_length > 0 && (size_t)prefix_length < sizeof(record.path) - 1)
        memset(record.path + prefix_length, 'x',
               sizeof(record.path) - (size_t)prefix_length - 1);

    CHECK(db_write_records("data", &record, 1) == 0);
    CHECK(db_load_records("data", &loaded, &count) == 0);
    CHECK(count == 1);
    if (loaded && count == 1)
        CHECK(memcmp(loaded, &record, sizeof(record)) == 0);
    free(loaded);

    memset(record.name, 'n', sizeof(record.name));
    CHECK(db_write_records("data", &record, 1) == -1);
}

static void test_corrupt_record_is_rejected(void) {
    image_record_t record;
    image_record_t sentinel;
    image_record_t *loaded = &sentinel;
    int count = 55;

    memset(&record, 0, sizeof(record));
    record.id = 32;
    memset(record.name, 'x', sizeof(record.name));
    snprintf(record.path, sizeof(record.path), "data/images/32.ppm");
    CHECK(write_bytes("data/metadata.dat", &record, sizeof(record)) == 0);
    CHECK(db_load_records("data", &loaded, &count) == -1);
    CHECK(loaded == NULL);
    CHECK(count == 0);

    memset(&record, 0, sizeof(record));
    record.id = 32;
    snprintf(record.name, sizeof(record.name), "unsafe.ppm");
    snprintf(record.path, sizeof(record.path), "../../unsafe.ppm");
    CHECK(write_bytes("data/metadata.dat", &record, sizeof(record)) == 0);
    loaded = &sentinel;
    count = 56;
    CHECK(db_load_records("data", &loaded, &count) == -1);
    CHECK(loaded == NULL);
    CHECK(count == 0);
}

static void test_truncated_files_reset_outputs(void) {
    image_record_t record_sentinel;
    image_feature_t feature_sentinel;
    image_record_t *records = &record_sentinel;
    image_feature_t *features = &feature_sentinel;
    int record_count = 99;
    int feature_count = 99;

    CHECK(append_byte("data/metadata.dat") == 0);
    CHECK(db_load_records("data", &records, &record_count) == -1);
    CHECK(records == NULL);
    CHECK(record_count == 0);

    CHECK(append_byte("data/features.dat") == 0);
    CHECK(db_load_features("data", &features, &feature_count) == -1);
    CHECK(features == NULL);
    CHECK(feature_count == 0);
}

static void test_pair_replacement(void) {
    image_record_t record_fixture[2];
    image_feature_t feature_fixture[2];
    image_record_t record;
    image_feature_t feature;
    image_record_t *records = NULL;
    image_feature_t *features = NULL;
    int record_count = 0;
    int feature_count = 0;

    populate_records(record_fixture);
    populate_features(feature_fixture);
    record = record_fixture[0];
    feature = feature_fixture[0];
    record.id = 21;
    snprintf(record.path, sizeof(record.path), "data/images/21.ppm");
    feature.image_id = 21;

    CHECK(db_replace_store("data", &record, 1, &feature, 1) == 0);
    CHECK(db_load_records("data", &records, &record_count) == 0);
    CHECK(record_count == 1);
    if (records && record_count == 1)
        CHECK(memcmp(records, &record, sizeof(record)) == 0);
    CHECK(db_load_features("data", &features, &feature_count) == 0);
    CHECK(feature_count == 1);
    if (features && feature_count == 1)
        CHECK(memcmp(features, &feature, sizeof(feature)) == 0);

    free(records);
    free(features);
}

static void test_write_failures_preserve_files(void) {
    image_record_t original_records[2];
    image_feature_t original_features[2];
    image_record_t replacement;
    image_feature_t replacement_feature;

    populate_records(original_records);
    populate_features(original_features);
    replacement = original_records[0];
    replacement.id = 40;
    snprintf(replacement.path, sizeof(replacement.path),
             "data/images/40.ppm");
    replacement_feature = original_features[0];
    replacement_feature.image_id = 40;

    CHECK(db_write_records("data", original_records, 2) == 0);
    CHECK(db_write_features("data", original_features, 2) == 0);

    CHECK(mkdir("data/metadata.tmp", 0700) == 0);
    CHECK(db_write_records("data", &replacement, 1) == -1);
    CHECK(file_matches("data/metadata.dat", original_records,
                       sizeof(original_records)));
    CHECK(rmdir("data/metadata.tmp") == 0);

    CHECK(mkdir("data/features.tmp", 0700) == 0);
    CHECK(db_write_features("data", &replacement_feature, 1) == -1);
    CHECK(file_matches("data/features.dat", original_features,
                       sizeof(original_features)));
    CHECK(rmdir("data/features.tmp") == 0);

    CHECK(mkdir("data/features.bak", 0700) == 0);
    CHECK(db_replace_store("data", &replacement, 1,
                           &replacement_feature, 1) == -1);
    CHECK(file_matches("data/metadata.dat", original_records,
                       sizeof(original_records)));
    CHECK(file_matches("data/features.dat", original_features,
                       sizeof(original_features)));
    CHECK(rmdir("data/features.bak") == 0);
}

int main(void) {
    char old_cwd[4096];
    char temp_root[128];
    int fixture_ready = 0;

    if (!getcwd(old_cwd, sizeof(old_cwd)) ||
        make_temp_root(temp_root, sizeof(temp_root)) != 0) {
        perror("store test setup");
        return 1;
    }
    if (chdir(temp_root) != 0) {
        perror("store test chdir");
        rmdir(temp_root);
        return 1;
    }

    CHECK(db_init("data") == 0);
    fixture_ready = 1;
    test_created_layout();
    test_empty_store();
    test_missing_and_unreadable_files();
    test_next_id_persistence_and_failure();
    test_round_trip_and_disk_layout();
    test_repeated_write_and_reinitialization();
    test_boundary_length_records();
    test_corrupt_record_is_rejected();

    /* Restore valid files before exercising truncation. */
    {
        image_record_t records[2];
        image_feature_t features[2];
        populate_records(records);
        populate_features(features);
        CHECK(db_write_records("data", records, 2) == 0);
        CHECK(db_write_features("data", features, 2) == 0);
    }
    test_truncated_files_reset_outputs();

    /* Restore valid files before exercising paired replacement. */
    {
        image_record_t records[2];
        image_feature_t features[2];
        populate_records(records);
        populate_features(features);
        CHECK(db_write_records("data", records, 2) == 0);
        CHECK(db_write_features("data", features, 2) == 0);
    }
    test_pair_replacement();
    test_write_failures_preserve_files();

    if (fixture_ready)
        remove_fixture_tree();
    if (chdir(old_cwd) != 0) {
        perror("store test restore cwd");
        return 1;
    }
    rmdir(temp_root);

    if (failures != 0) {
        fprintf(stderr, "%d store test(s) failed\n", failures);
        return 1;
    }
    puts("store unit tests: PASS");
    return 0;
}
