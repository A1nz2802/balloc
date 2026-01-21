#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "block.h"
#include "region.h"

#define PAGE_SIZE 4096
#define MIN_ALLOC_SIZE 8 // minimum bytes required to justify a split

static region_t *global_region = NULL;

void init_region() {
    // request raw memory from the kernel
    void *addr = mmap(
        NULL,
        PAGE_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);

    if (addr == MAP_FAILED) {
        printf("mmap failed to allocate initial heap memory.\n");
        exit(1);
    }

    // initialize region header
    global_region = (region_t *)addr;
    global_region->size = PAGE_SIZE;
    global_region->next = NULL;

    // initialize first block free block after the region header
    // use void* arithmetic to ensure exact byte offsets
    block_t *first_block = (block_t *)((void *)global_region + sizeof(region_t));

    // calculate usable size: Total Page - Region Header - Block Header
    first_block->size = PAGE_SIZE - sizeof(region_t) - sizeof(block_t);
    first_block->is_free = true;
    first_block->next = NULL;
}

size_t align(size_t n) {
    return (n + 7) & ~7;
}

/**
 * Splits a large free block into two:
 * 1. The allocaed block (requested size).
 * 2. A new free block (remaining size).
 */
void split_block(block_t *current_block, size_t requested_size) {
    // calculate pointer to the new block
    block_t *new_block = (block_t *)((void *)current_block + sizeof(block_t) + requested_size);

    // calculate the payload size available for this new split block
    size_t remaining_size = current_block->size - requested_size - sizeof(block_t);

    // initialize metadata for the new free block
    new_block->size = remaining_size;
    new_block->is_free = true;
    new_block->next = current_block->next;

    // update the current block
    current_block->size = requested_size;
    current_block->is_free = false;
    current_block->next = new_block;
}

void *balloc(size_t size) {
    if (size <= 0) {
        return NULL;
    }

    // initialize region on first call only
    if (global_region == NULL) {
        init_region();
    }

    size_t aligned_size = align(size);

    // compute pointer to the first block (skip region header)
    block_t *block = (block_t *)((void *)global_region + sizeof(region_t));

    // linear search (First-Fit Strategy)
    while (block != NULL) {
        if (block->is_free && block->size >= aligned_size) {

            size_t remaining_space = block->size - aligned_size;

            // if remaining space large enough for a new Header + Min Payload?
            if (remaining_space >= sizeof(block_t) + MIN_ALLOC_SIZE) {
                split_block(block, aligned_size);
            } else {
                // not enough space to split effectively, just claim the whole block
                block->is_free = false;
            }

            return (void *)block + sizeof(block_t);
        }

        block = block->next;
    }

    printf("Out of memory!\n");
    return NULL;
}

int main() {

    balloc(10);
    balloc(20);

    printf("Some");

    return 0;
}
