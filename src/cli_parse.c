#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cli_parse.h"

static int parse_positive_int(const char *text, int *value) {
    char *end;
    long parsed;

    if (!text || !*text)
        return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        parsed <= 0 || parsed > (long)INT_MAX)
        return 0;
    *value = (int)parsed;
    return 1;
}

static int parse_int_range(const char *text, int minimum, int maximum,
                           int *value) {
    char *end;
    long parsed;

    if (!text || !*text)
        return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        parsed < (long)minimum || parsed > (long)maximum)
        return 0;
    *value = (int)parsed;
    return 1;
}

static cli_parse_status_t fail(cli_parse_error_t *error,
                               cli_parse_error_code_t code,
                               app_command_kind_t command,
                               const char *argument,
                               const char *label,
                               const char *secondary) {
    error->code = code;
    error->command = command;
    error->argument = argument;
    error->label = label;
    error->secondary = secondary;
    return CLI_PARSE_ERROR;
}

static int parse_metric(const char *text, search_metric_t *metric) {
    if (strcmp(text, "l1") == 0)
        *metric = METRIC_L1;
    else if (strcmp(text, "l2") == 0)
        *metric = METRIC_L2;
    else if (strcmp(text, "intersection") == 0)
        *metric = METRIC_INTERSECTION;
    else
        return 0;
    return 1;
}

static int parse_query_field(const char *text, app_query_field_t *field) {
    if (strcmp(text, "id") == 0) *field = APP_QUERY_ID;
    else if (strcmp(text, "name") == 0) *field = APP_QUERY_NAME;
    else if (strcmp(text, "width") == 0) *field = APP_QUERY_WIDTH;
    else if (strcmp(text, "height") == 0) *field = APP_QUERY_HEIGHT;
    else if (strcmp(text, "format") == 0) *field = APP_QUERY_FORMAT;
    else if (strcmp(text, "size") == 0) *field = APP_QUERY_SIZE;
    else return 0;
    return 1;
}

static int parse_query_operator(const char *text, app_query_operator_t *op) {
    if (strcmp(text, "eq") == 0) *op = APP_QUERY_EQ;
    else if (strcmp(text, "ne") == 0) *op = APP_QUERY_NE;
    else if (strcmp(text, "gt") == 0) *op = APP_QUERY_GT;
    else if (strcmp(text, "ge") == 0) *op = APP_QUERY_GE;
    else if (strcmp(text, "lt") == 0) *op = APP_QUERY_LT;
    else if (strcmp(text, "le") == 0) *op = APP_QUERY_LE;
    else if (strcmp(text, "contains") == 0) *op = APP_QUERY_CONTAINS;
    else return 0;
    return 1;
}

static int query_field_operator_valid(app_query_field_t field,
                                      app_query_operator_t op) {
    if (field == APP_QUERY_NAME)
        return op == APP_QUERY_EQ || op == APP_QUERY_NE ||
               op == APP_QUERY_CONTAINS;
    if (field == APP_QUERY_FORMAT)
        return op == APP_QUERY_EQ || op == APP_QUERY_NE;
    return op >= APP_QUERY_EQ && op <= APP_QUERY_LE;
}

static int query_field_numeric(app_query_field_t field) {
    return field == APP_QUERY_ID || field == APP_QUERY_WIDTH ||
           field == APP_QUERY_HEIGHT || field == APP_QUERY_SIZE;
}

static cli_parse_status_t parse_id_output(int argc, char *const argv[],
                                          app_command_t *command,
                                          cli_parse_error_t *error,
                                          app_command_kind_t kind) {
    int id;
    if (argc != 4)
        return fail(error, CLI_PARSE_ERROR_USAGE, kind, NULL, NULL, NULL);
    if (!parse_positive_int(argv[2], &id))
        return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE, kind,
                    argv[2], "ID", NULL);
    command->kind = kind;
    command->args.image_output.id = id;
    command->args.image_output.output_path = argv[3];
    return CLI_PARSE_OK;
}

static cli_parse_status_t parse_search_output(int argc, char *const argv[],
                                              app_command_t *command,
                                              cli_parse_error_t *error,
                                              app_command_kind_t kind) {
    search_metric_t metric = METRIC_INTERSECTION;
    int id;
    int top_k;

    if (argc < 5 || argc > 7)
        return fail(error, CLI_PARSE_ERROR_USAGE, kind, NULL, NULL, NULL);
    if (!parse_positive_int(argv[2], &id))
        return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE, kind,
                    argv[2], "ID", NULL);
    if (!parse_positive_int(argv[3], &top_k))
        return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE, kind,
                    argv[3], "k", NULL);
    if (argc >= 6) {
        if (argc != 7 || strcmp(argv[5], "--metric") != 0)
            return fail(error, CLI_PARSE_ERROR_USAGE, kind, NULL, NULL, NULL);
        if (!parse_metric(argv[6], &metric))
            return fail(error, CLI_PARSE_ERROR_UNKNOWN_METRIC, kind,
                        argv[6], NULL, NULL);
    }
    command->kind = kind;
    command->args.search_output.id = id;
    command->args.search_output.top_k = top_k;
    command->args.search_output.output_path = argv[4];
    command->args.search_output.metric = metric;
    return CLI_PARSE_OK;
}

cli_parse_status_t cli_parse(int argc, char *const argv[],
                             app_command_t *command,
                             cli_parse_error_t *error) {
    const char *name;
    int id;

    if (!command || !error || argc < 1 || !argv)
        return CLI_PARSE_ERROR;
    memset(command, 0, sizeof(*command));
    memset(error, 0, sizeof(*error));

    if (argc < 2) {
        command->kind = APP_COMMAND_HELP;
        return CLI_PARSE_HELP;
    }
    name = argv[1];

    if (strcmp(name, "help") == 0) {
        command->kind = APP_COMMAND_HELP;
        if (argc != 2)
            return fail(error, CLI_PARSE_ERROR_TAKES_NO_ARGUMENTS,
                        APP_COMMAND_HELP, NULL, NULL, NULL);
        return CLI_PARSE_HELP;
    }
    if (strcmp(name, "init") == 0 || strcmp(name, "list") == 0 ||
        strcmp(name, "stats") == 0 || strcmp(name, "compact") == 0 ||
        strcmp(name, "verify") == 0 || strcmp(name, "repair") == 0) {
        if (strcmp(name, "init") == 0) command->kind = APP_COMMAND_INIT;
        else if (strcmp(name, "list") == 0) command->kind = APP_COMMAND_LIST;
        else if (strcmp(name, "stats") == 0) command->kind = APP_COMMAND_STATS;
        else if (strcmp(name, "compact") == 0) command->kind = APP_COMMAND_COMPACT;
        else if (strcmp(name, "verify") == 0) command->kind = APP_COMMAND_VERIFY;
        else command->kind = APP_COMMAND_REPAIR;
        if (argc != 2)
            return fail(error, CLI_PARSE_ERROR_TAKES_NO_ARGUMENTS,
                        command->kind, NULL, NULL, NULL);
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "import") == 0) {
        command->kind = APP_COMMAND_IMPORT;
        if (argc != 3)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        command->args.import_file.path = argv[2];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "info") == 0 || strcmp(name, "delete") == 0 ||
        strcmp(name, "hist") == 0) {
        if (strcmp(name, "info") == 0) command->kind = APP_COMMAND_INFO;
        else if (strcmp(name, "delete") == 0) command->kind = APP_COMMAND_DELETE;
        else command->kind = APP_COMMAND_HIST;
        if (argc != 3)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[2], &id))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[2], "ID", NULL);
        command->args.id.id = id;
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "gray") == 0)
        return parse_id_output(argc, argv, command, error, APP_COMMAND_GRAY);
    if (strcmp(name, "blur") == 0)
        return parse_id_output(argc, argv, command, error, APP_COMMAND_BLUR);
    if (strcmp(name, "edge") == 0)
        return parse_id_output(argc, argv, command, error, APP_COMMAND_EDGE);
    if (strcmp(name, "equalize") == 0)
        return parse_id_output(argc, argv, command, error, APP_COMMAND_EQUALIZE);
    if (strcmp(name, "gaussian") == 0)
        return parse_id_output(argc, argv, command, error, APP_COMMAND_GAUSSIAN);
    if (strcmp(name, "hist-image") == 0)
        return parse_id_output(argc, argv, command, error, APP_COMMAND_HIST_IMAGE);

    if (strcmp(name, "binary") == 0) {
        int threshold;
        command->kind = APP_COMMAND_BINARY;
        if (argc != 5)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[2], &id))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[2], "ID", NULL);
        if (!parse_int_range(argv[3], 0, 255, &threshold))
            return fail(error, CLI_PARSE_ERROR_INVALID_THRESHOLD,
                        command->kind, argv[3], NULL, NULL);
        command->args.binary.id = id;
        command->args.binary.threshold = threshold;
        command->args.binary.output_path = argv[4];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "search") == 0) {
        int top_k;
        search_metric_t metric = METRIC_INTERSECTION;
        command->kind = APP_COMMAND_SEARCH;
        if (argc < 4 || argc > 6)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[2], &id))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[2], "ID", NULL);
        if (!parse_positive_int(argv[3], &top_k))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[3], "k", NULL);
        if (argc >= 5) {
            if (argc != 6 || strcmp(argv[4], "--metric") != 0)
                return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                            NULL, NULL, NULL);
            if (!parse_metric(argv[5], &metric))
                return fail(error, CLI_PARSE_ERROR_UNKNOWN_METRIC_WITH_HINT,
                            command->kind, argv[5], NULL, NULL);
        }
        command->args.search.id = id;
        command->args.search.top_k = top_k;
        command->args.search.metric = metric;
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "search-similar") == 0) {
        int top_k;
        command->kind = APP_COMMAND_SEARCH_SIMILAR;
        if (argc != 5 || strcmp(argv[3], "--topk") != 0)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[4], &top_k))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[4], "topk", NULL);
        command->args.search_similar.query_path = argv[2];
        command->args.search_similar.top_k = top_k;
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "resize") == 0 || strcmp(name, "resize-bilinear") == 0) {
        int width;
        int height;
        command->kind = strcmp(name, "resize") == 0
                            ? APP_COMMAND_RESIZE : APP_COMMAND_RESIZE_BILINEAR;
        if (argc != 6)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[2], &id))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[2], "ID", NULL);
        if (!parse_positive_int(argv[3], &width))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[3], "new_width", NULL);
        if (!parse_positive_int(argv[4], &height))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[4], "new_height", NULL);
        command->args.resize.id = id;
        command->args.resize.width = width;
        command->args.resize.height = height;
        command->args.resize.output_path = argv[5];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "rotate") == 0) {
        int degrees;
        command->kind = APP_COMMAND_ROTATE;
        if (argc != 5)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[2], &id))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[2], "ID", NULL);
        if (!parse_int_range(argv[3], 0, 360, &degrees))
            return fail(error, CLI_PARSE_ERROR_INVALID_ANGLE,
                        command->kind, argv[3], NULL, NULL);
        command->args.rotate.id = id;
        command->args.rotate.degrees = degrees;
        command->args.rotate.output_path = argv[4];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "median") == 0) {
        int kernel_size;
        command->kind = APP_COMMAND_MEDIAN;
        if (argc != 5)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[2], &id))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[2], "ID", NULL);
        if (!parse_positive_int(argv[3], &kernel_size))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[3], "kernel_size", NULL);
        command->args.median.id = id;
        command->args.median.kernel_size = kernel_size;
        command->args.median.output_path = argv[4];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "adjust") == 0) {
        char *end;
        long brightness;
        double contrast;
        command->kind = APP_COMMAND_ADJUST;
        if (argc != 6)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[2], &id))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[2], "ID", NULL);
        errno = 0;
        brightness = strtol(argv[3], &end, 10);
        if (errno == ERANGE || end == argv[3] || *end != '\0' ||
            brightness < (long)INT_MIN || brightness > (long)INT_MAX)
            return fail(error, CLI_PARSE_ERROR_INVALID_BRIGHTNESS,
                        command->kind, argv[3], NULL, NULL);
        errno = 0;
        contrast = strtod(argv[4], &end);
        if (errno == ERANGE || end == argv[4] || *end != '\0' ||
            !isfinite(contrast) || contrast <= 0.0 || contrast > 10.0)
            return fail(error, CLI_PARSE_ERROR_INVALID_CONTRAST,
                        command->kind, argv[4], NULL, NULL);
        command->args.adjust.id = id;
        command->args.adjust.brightness = (int)brightness;
        command->args.adjust.contrast = contrast;
        command->args.adjust.output_path = argv[5];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "find-name") == 0) {
        command->kind = APP_COMMAND_FIND_NAME;
        if (argc != 3)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        command->args.find_name.keyword = argv[2];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "query") == 0) {
        char *end;
        long value = 0;
        command->kind = APP_COMMAND_QUERY;
        if (argc != 5)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_query_field(argv[2], &command->args.query.field))
            return fail(error, CLI_PARSE_ERROR_UNKNOWN_FIELD, command->kind,
                        argv[2], NULL, NULL);
        if (!parse_query_operator(argv[3], &command->args.query.op))
            return fail(error, CLI_PARSE_ERROR_UNKNOWN_OPERATOR, command->kind,
                        argv[3], NULL, NULL);
        if (!query_field_operator_valid(command->args.query.field,
                                        command->args.query.op))
            return fail(error, CLI_PARSE_ERROR_INVALID_FIELD_OPERATOR,
                        command->kind, argv[3], NULL, argv[2]);
        if (query_field_numeric(command->args.query.field)) {
            errno = 0;
            value = strtol(argv[4], &end, 10);
            if (errno == ERANGE || end == argv[4] || *end != '\0')
                return fail(error, CLI_PARSE_ERROR_INVALID_NUMERIC_VALUE,
                            command->kind, argv[4], NULL, argv[2]);
        }
        command->args.query.numeric_value = value;
        command->args.query.text_value = argv[4];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "export") == 0) {
        command->kind = APP_COMMAND_EXPORT;
        if (argc != 3)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        command->args.export_file.output_path = argv[2];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "report") == 0) {
        command->kind = APP_COMMAND_REPORT;
        if (argc != 4)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        command->args.report.output_dir = argv[2];
        command->args.report.report_path = argv[3];
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "hist-export") == 0) {
        int normalized = 0;
        command->kind = APP_COMMAND_HIST_EXPORT;
        if (argc < 4 || argc > 5)
            return fail(error, CLI_PARSE_ERROR_USAGE, command->kind,
                        NULL, NULL, NULL);
        if (!parse_positive_int(argv[2], &id))
            return fail(error, CLI_PARSE_ERROR_INVALID_POSITIVE,
                        command->kind, argv[2], "ID", NULL);
        if (argc == 5) {
            if (strcmp(argv[4], "--normalized") != 0)
                return fail(error, CLI_PARSE_ERROR_UNKNOWN_OPTION,
                            command->kind, argv[4], NULL, NULL);
            normalized = 1;
        }
        command->args.hist_export.id = id;
        command->args.hist_export.output_path = argv[3];
        command->args.hist_export.normalized = normalized;
        return CLI_PARSE_OK;
    }
    if (strcmp(name, "search-export") == 0)
        return parse_search_output(argc, argv, command, error,
                                   APP_COMMAND_SEARCH_EXPORT);
    if (strcmp(name, "search-contact") == 0)
        return parse_search_output(argc, argv, command, error,
                                   APP_COMMAND_SEARCH_CONTACT);

    return fail(error, CLI_PARSE_ERROR_UNKNOWN_COMMAND, APP_COMMAND_HELP,
                name, NULL, NULL);
}
