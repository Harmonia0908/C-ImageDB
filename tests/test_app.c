#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app.h"
#include "report.h"

static int make_test_directory(char *path_template) {
    int fd = mkstemp(path_template);
    if (fd < 0)
        return -1;
    close(fd);
    if (unlink(path_template) != 0)
        return -1;
    return mkdir(path_template, 0700);
}

static app_status_t execute_without_terminal_output(
    const app_context_t *context, const app_command_t *command,
    app_result_t *result) {
    FILE *captured_stdout = tmpfile();
    FILE *captured_stderr = tmpfile();
    int saved_stdout;
    int saved_stderr;
    long stdout_size;
    long stderr_size;
    app_status_t status;

    assert(captured_stdout != NULL);
    assert(captured_stderr != NULL);
    fflush(stdout);
    fflush(stderr);
    saved_stdout = dup(STDOUT_FILENO);
    saved_stderr = dup(STDERR_FILENO);
    assert(saved_stdout >= 0);
    assert(saved_stderr >= 0);
    assert(dup2(fileno(captured_stdout), STDOUT_FILENO) >= 0);
    assert(dup2(fileno(captured_stderr), STDERR_FILENO) >= 0);

    status = app_execute(context, command, result);
    fflush(stdout);
    fflush(stderr);
    stdout_size = ftell(captured_stdout);
    stderr_size = ftell(captured_stderr);

    assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
    assert(dup2(saved_stderr, STDERR_FILENO) >= 0);
    close(saved_stdout);
    close(saved_stderr);
    fclose(captured_stdout);
    fclose(captured_stderr);
    assert(stdout_size == 0);
    assert(stderr_size == 0);
    return status;
}

static void remove_test_store(const char *dir) {
    char path[MAX_PATH_LEN];
    const char *files[] = {
        "/images/1.ppm", "/metadata.dat", "/features.dat", "/.next_id"
    };
    size_t i;

    for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        snprintf(path, sizeof(path), "%s%s", dir, files[i]);
        unlink(path);
    }
    snprintf(path, sizeof(path), "%s/images", dir);
    rmdir(path);
    rmdir(dir);
}

static void test_store_workflow_is_callable_without_cli(void) {
    char directory[] = "/tmp/cimagedb-app-XXXXXX";
    app_context_t context;
    app_command_t command;
    app_result_t result;

    assert(make_test_directory(directory) == 0);
    context.data_dir = directory;

    memset(&command, 0, sizeof(command));
    command.kind = APP_COMMAND_INIT;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_LIST;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(result.data.records.count == 0);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_IMPORT;
    command.args.import_file.path = "samples/sample1.ppm";
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(result.data.record.id == 1);
    assert(strcmp(result.data.record.name, "sample1.ppm") == 0);
    assert(result.data.record.width == 64);
    assert(result.data.record.height == 64);
    app_result_destroy(&result);

    assert(app_execute(&context, &command, &result) == APP_STATUS_DUPLICATE_IMAGE);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_INFO;
    command.args.id.id = 1;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(result.data.record.id == 1);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_SEARCH;
    command.args.search.id = 1;
    command.args.search.top_k = 3;
    command.args.search.metric = METRIC_INTERSECTION;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(result.data.search.count == 0);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_STATS;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(result.data.stats.total_records == 1);
    assert(result.data.stats.active_records == 1);
    assert(result.data.stats.feature_records == 1);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_VERIFY;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(result.data.verify.status_failed == 0);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_DELETE;
    command.args.id.id = 1;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(strcmp(result.data.record.name, "sample1.ppm") == 0);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_INFO;
    command.args.id.id = 1;
    assert(app_execute(&context, &command, &result) ==
           APP_STATUS_RECORD_NOT_FOUND);
    app_result_destroy(&result);

    remove_test_store(directory);
}

static void test_processing_returns_an_image_instead_of_writing_output(void) {
    char directory[] = "/tmp/cimagedb-app-image-XXXXXX";
    app_context_t context;
    app_command_t command;
    app_result_t result;
    const char *output_path = "/tmp/cimagedb-app-should-not-exist.ppm";

    unlink(output_path);
    assert(make_test_directory(directory) == 0);
    context.data_dir = directory;

    memset(&command, 0, sizeof(command));
    command.kind = APP_COMMAND_INIT;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_IMPORT;
    command.args.import_file.path = "samples/sample1.ppm";
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_GRAY;
    command.args.image_output.id = 1;
    command.args.image_output.output_path = output_path;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(result.data.image.image != NULL);
    assert(result.data.image.image->width == 64);
    assert(access(output_path, F_OK) != 0);
    app_result_destroy(&result);

    remove_test_store(directory);
}

static void test_query_returns_structured_records(void) {
    char directory[] = "/tmp/cimagedb-app-query-XXXXXX";
    app_context_t context;
    app_command_t command;
    app_result_t result;

    assert(make_test_directory(directory) == 0);
    context.data_dir = directory;
    memset(&command, 0, sizeof(command));

    command.kind = APP_COMMAND_INIT;
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    app_result_destroy(&result);
    command.kind = APP_COMMAND_IMPORT;
    command.args.import_file.path = "samples/sample1.ppm";
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_QUERY;
    command.args.query.field = APP_QUERY_WIDTH;
    command.args.query.op = APP_QUERY_GE;
    command.args.query.numeric_value = 64;
    command.args.query.text_value = "64";
    assert(app_execute(&context, &command, &result) == APP_STATUS_OK);
    assert(result.data.records.count == 1);
    assert(result.data.records.items[0].id == 1);
    app_result_destroy(&result);

    command.args.query.field = APP_QUERY_NAME;
    command.args.query.op = APP_QUERY_GT;
    command.args.query.text_value = "sample1.ppm";
    assert(app_execute(&context, &command, &result) ==
           APP_STATUS_INVALID_ARGUMENT);
    app_result_destroy(&result);

    remove_test_store(directory);
}

static void test_errors_are_structured_and_do_not_print(void) {
    char directory[] = "/tmp/cimagedb-app-errors-XXXXXX";
    app_context_t context;
    app_command_t command;
    app_result_t result;

    assert(make_test_directory(directory) == 0);
    context.data_dir = directory;
    memset(&command, 0, sizeof(command));

    command.kind = APP_COMMAND_INIT;
    assert(execute_without_terminal_output(&context, &command, &result) ==
           APP_STATUS_OK);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_INFO;
    command.args.id.id = 99;
    assert(execute_without_terminal_output(&context, &command, &result) ==
           APP_STATUS_RECORD_NOT_FOUND);
    assert(result.detail_value == 99);
    app_result_destroy(&result);

    command.kind = APP_COMMAND_REPORT;
    command.args.report.output_dir = "/path/that/does/not/exist";
    command.args.report.report_path = "report.html";
    assert(execute_without_terminal_output(&context, &command, &result) ==
           APP_STATUS_REPORT_FAILED);
    assert(result.auxiliary_status == REPORT_STATUS_OUTPUT_DIR_MISSING);
    app_result_destroy(&result);

    remove_test_store(directory);
}

int main(void) {
    test_store_workflow_is_callable_without_cli();
    test_processing_returns_an_image_instead_of_writing_output();
    test_query_returns_structured_records();
    test_errors_are_structured_and_do_not_print();
    puts("app integration tests: PASS");
    return 0;
}
