#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Region Header: Represents a large chunk of memory mapped from the OS.
 */
typedef struct region {
    size_t size;         // Total size of the mapped region
    struct region *next; // Pointer to the next mapped region (for tracking)
} region_t;

/**
 * Block Header: Represents a chunk of memory within a region.
 * Used for the doubly linked list of memory blocks.
 */
typedef struct block {
    size_t size;        // Size of the payload
    bool is_free;       // Allocation status
    struct block *next; // Pointer to the next block in the list
    struct block *prev; // Pointer to the previous block in the list
} block_t;

// --- Public API ---

void *balloc(size_t size);
void free(void *payload);

// Debug / Testing helper
void verify_heap();

#endif // ALLOCATOR_H
