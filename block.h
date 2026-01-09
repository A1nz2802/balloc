#include <stdbool.h>
#include <stddef.h>

typedef struct block {
    size_t size;        // How many bytes of data has this block (payload).
    bool is_free;       // Is free or not?
    struct block *next; // Pointer to the next block.
    struct block *prev; // Pointer to the previous block.
} block_t;
