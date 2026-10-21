#ifndef ALPACOS_KSTRING_H
#define ALPACOS_KSTRING_H

#include <stdbool.h>
#include <stddef.h>

size_t kstrlen(const char *s);
bool kstreq(const char *a, const char *b);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
void *memset(void *dest, int value, size_t count);
int memcmp(const void *a, const void *b, size_t count);

#endif
