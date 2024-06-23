#pragma once

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#define da_append(da, item)                                                    \
    do {                                                                       \
        if ((da)->count >= (da)->capacity) {                                   \
            (da)->capacity = (da)->capacity == 0 ? 2 : (da)->capacity * 2;     \
            (da)->items =                                                      \
                realloc((da)->items, (da)->capacity * sizeof(*(da)->items));   \
                                                                               \
            if ((da)->items == NULL) {                                         \
                printf("out of memory\n");                                     \
                exit(1);                                                       \
            }                                                                  \
        }                                                                      \
                                                                               \
        (da)->items[(da)->count++] = (item);                                   \
    } while (0)

#define da_free(da) free((da).items)
