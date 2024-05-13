#include <stddef.h>

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} DynamicString;
