#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define da_append(da, item)                                                    \
    do {                                                                       \
        if ((da)->count >= (da)->capacity) {                                   \
            (da)->capacity = (da)->capacity == 0 ? 1 : (da)->capacity * 2;     \
            (da)->items =                                                      \
                realloc((da)->items, (da)->capacity * sizeof(*(da)->items));   \
            if ((da)->items == NULL) {                                         \
                fprintf(stderr, "out of memory\n");                            \
                exit(0);                                                       \
            }                                                                  \
        }                                                                      \
                                                                               \
        (da)->items[(da)->count++] = (item);                                   \
    } while (0)

#define da_free(da) free((da).items)
