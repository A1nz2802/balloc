#include <stddef.h>

typedef struct region {
    size_t size;         // Size of the region (ej. 4096).
    struct region *next; // Pointer to the next region
} region_t;
