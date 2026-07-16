#ifndef REPORT_H
#define REPORT_H

typedef enum report_status {
    REPORT_STATUS_OK = 0,
    REPORT_STATUS_OUTPUT_DIR_MISSING,
    REPORT_STATUS_PATH_REQUIRED,
    REPORT_STATUS_PATH_TOO_LONG,
    REPORT_STATUS_OPEN_FAILED,
    REPORT_STATUS_FINISH_FAILED
} report_status_t;

report_status_t generate_html_report_status(const char *output_dir,
                                            const char *report_path);
int generate_html_report(const char *output_dir, const char *report_path);

#endif
