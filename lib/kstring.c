#include <alpacos/kstring.h>
#include <stdint.h>

size_t kstrlen(const char *s)
{
    size_t length = 0;
    while (s[length] != '\0') {
        ++length;
    }
    return length;
}

bool kstreq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    for (size_t i = 0; i < count; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t count)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    if ((uintptr_t)d < (uintptr_t)s) {
        for (size_t i = 0; i < count; ++i) {
            d[i] = s[i];
        }
    } else {
        while (count > 0u) {
            --count;
            d[count] = s[count];
        }
    }
    return dest;
}

void *memset(void *dest, int value, size_t count)
{
    unsigned char *d = dest;
    for (size_t i = 0; i < count; ++i) {
        d[i] = (unsigned char)value;
    }
    return dest;
}

int memcmp(const void *a, const void *b, size_t count)
{
    const unsigned char *left = a;
    const unsigned char *right = b;
    for (size_t i = 0; i < count; ++i) {
        if (left[i] != right[i]) {
            return (int)left[i] - (int)right[i];
        }
    }
    return 0;
}
