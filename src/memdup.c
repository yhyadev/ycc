#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "memdup.h"

void *memdup(void *p, size_t n) {
    void *d = malloc(n);

    if (d == NULL) {
        printf("out of memory\n");
        exit(1);
    }

    memcpy(d, p, n);

    return d;
}
