#ifndef CLI_PARSE_H
#define CLI_PARSE_H

#include "app.h"

typedef enum cli_parse_status {
    CLI_PARSE_OK,
    CLI_PARSE_HELP,
    CLI_PARSE_ERROR
} cli_parse_status_t;

typedef enum cli_parse_error_code {
    CLI_PARSE_ERROR_USAGE,
    CLI_PARSE_ERROR_TAKES_NO_ARGUMENTS,
    CLI_PARSE_ERROR_INVALID_POSITIVE,
    CLI_PARSE_ERROR_INVALID_THRESHOLD,
    CLI_PARSE_ERROR_INVALID_ANGLE,
    CLI_PARSE_ERROR_INVALID_BRIGHTNESS,
    CLI_PARSE_ERROR_INVALID_CONTRAST,
    CLI_PARSE_ERROR_UNKNOWN_METRIC_WITH_HINT,
    CLI_PARSE_ERROR_UNKNOWN_METRIC,
    CLI_PARSE_ERROR_UNKNOWN_OPTION,
    CLI_PARSE_ERROR_UNKNOWN_COMMAND,
    CLI_PARSE_ERROR_UNKNOWN_FIELD,
    CLI_PARSE_ERROR_UNKNOWN_OPERATOR,
    CLI_PARSE_ERROR_INVALID_FIELD_OPERATOR,
    CLI_PARSE_ERROR_INVALID_NUMERIC_VALUE
} cli_parse_error_code_t;

typedef struct cli_parse_error {
    cli_parse_error_code_t code;
    app_command_kind_t command;
    const char *argument;
    const char *label;
    const char *secondary;
} cli_parse_error_t;

cli_parse_status_t cli_parse(int argc, char *const argv[],
                             app_command_t *command,
                             cli_parse_error_t *error);

#endif
