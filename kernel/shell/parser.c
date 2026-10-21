#include <alpacos/parser.h>

parse_result_t shell_parse(char *line, char **argv, size_t capacity, size_t *argc)
{
    *argc = 0;
    char *cursor = line;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        if (*argc == capacity) {
            return PARSE_TOO_MANY_ARGS;
        }
        argv[(*argc)++] = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            ++cursor;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return *argc == 0u ? PARSE_EMPTY : PARSE_OK;
}
