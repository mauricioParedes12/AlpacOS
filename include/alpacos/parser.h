#ifndef ALPACOS_PARSER_H
#define ALPACOS_PARSER_H

#include <stddef.h>

#define SHELL_LINE_CAPACITY 128u
#define SHELL_MAX_ARGS 16u

typedef enum {
    PARSE_OK,
    PARSE_EMPTY,
    PARSE_TOO_MANY_ARGS
} parse_result_t;

/* In-place split on spaces/tabs. argv points into line; no allocation. */
parse_result_t shell_parse(char *line, char **argv, size_t capacity, size_t *argc);

#endif
