#ifndef VERIFY_H
#define VERIFY_H

typedef struct verify_summary {
    int total_records;
    int missing_files;
    int missing_histograms;
    int duplicate_ids;
    int duplicate_paths;
    int invalid_records;
    int dimension_mismatches;
    int metadata_missing;
    int feature_store_missing;
    int status_failed;
} verify_summary_t;

typedef struct repair_summary {
    int removed_records;
    int regenerated_histograms;
    int fixed_dimensions;
    int remaining_issues;
} repair_summary_t;

int verify_database(const char *data_dir, verify_summary_t *summary);
int repair_database(const char *data_dir, repair_summary_t *summary);

void verify_print_summary(const verify_summary_t *summary);
void repair_print_summary(const repair_summary_t *summary);

#endif
