#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "cli_parse.h"

static void test_no_command_requests_help(void) {
    char *argv[] = {(char *)"imagedb"};
    app_command_t command;
    cli_parse_error_t error;

    assert(cli_parse(1, argv, &command, &error) == CLI_PARSE_HELP);
    assert(command.kind == APP_COMMAND_HELP);
}

static void test_import_keeps_borrowed_path(void) {
    char *argv[] = {(char *)"imagedb", (char *)"import",
                    (char *)"samples/sample1.ppm"};
    app_command_t command;
    cli_parse_error_t error;

    assert(cli_parse(3, argv, &command, &error) == CLI_PARSE_OK);
    assert(command.kind == APP_COMMAND_IMPORT);
    assert(strcmp(command.args.import_file.path, "samples/sample1.ppm") == 0);
}

static void test_search_parses_default_and_explicit_metric(void) {
    char *default_argv[] = {(char *)"imagedb", (char *)"search",
                            (char *)"7", (char *)"3"};
    char *metric_argv[] = {(char *)"imagedb", (char *)"search",
                           (char *)"7", (char *)"3", (char *)"--metric",
                           (char *)"l2"};
    app_command_t command;
    cli_parse_error_t error;

    assert(cli_parse(4, default_argv, &command, &error) == CLI_PARSE_OK);
    assert(command.args.search.id == 7);
    assert(command.args.search.top_k == 3);
    assert(command.args.search.metric == METRIC_INTERSECTION);

    assert(cli_parse(6, metric_argv, &command, &error) == CLI_PARSE_OK);
    assert(command.args.search.metric == METRIC_L2);
}

static void test_adjust_parses_signed_brightness(void) {
    char *argv[] = {(char *)"imagedb", (char *)"adjust", (char *)"2",
                    (char *)"-15", (char *)"1.25", (char *)"out.ppm"};
    app_command_t command;
    cli_parse_error_t error;

    assert(cli_parse(6, argv, &command, &error) == CLI_PARSE_OK);
    assert(command.kind == APP_COMMAND_ADJUST);
    assert(command.args.adjust.brightness == -15);
    assert(command.args.adjust.contrast == 1.25);
}

static void test_query_is_typed_by_parser(void) {
    char *argv[] = {(char *)"imagedb", (char *)"query", (char *)"width",
                    (char *)"ge", (char *)"128"};
    app_command_t command;
    cli_parse_error_t error;

    assert(cli_parse(5, argv, &command, &error) == CLI_PARSE_OK);
    assert(command.kind == APP_COMMAND_QUERY);
    assert(command.args.query.field == APP_QUERY_WIDTH);
    assert(command.args.query.op == APP_QUERY_GE);
    assert(command.args.query.numeric_value == 128);
}

static void test_parse_error_is_structured(void) {
    char *argv[] = {(char *)"imagedb", (char *)"info", (char *)"abc"};
    app_command_t command;
    cli_parse_error_t error;

    assert(cli_parse(3, argv, &command, &error) == CLI_PARSE_ERROR);
    assert(error.code == CLI_PARSE_ERROR_INVALID_POSITIVE);
    assert(strcmp(error.argument, "abc") == 0);
    assert(strcmp(error.label, "ID") == 0);
}

int main(void) {
    test_no_command_requests_help();
    test_import_keeps_borrowed_path();
    test_search_parses_default_and_explicit_metric();
    test_adjust_parses_signed_brightness();
    test_query_is_typed_by_parser();
    test_parse_error_is_structured();
    puts("cli parser unit tests: PASS");
    return 0;
}
