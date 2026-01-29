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

    // Link back to the current block (backward Link)
    new_block->prev = current_block;

    // update the current block metadata
    current_block->size = requested_size;
    current_block->is_free = false;
    current_block->next = new_block;

    // Update the reverse pointer of the next block (if it exists)
    // The block that used to follow 'current_block' must now point back to 'new_block'
    if (new_block->next != NULL) {
        new_block->next->prev = new_block;
    }
}

void free(void *payload) {
    // safety check
    if (payload == NULL) {
        return;
    }

    // get the block header
    // we move backwards from the payload pointer to find the block start
    block_t *current_block = (block_t *)((void *)payload - sizeof(block_t));

    // mark as free
    current_block->is_free = true;

    // get next block (neighbor)
    block_t *next_block = current_block->next;

    // check if next block exists AND is free
    if (next_block != NULL && next_block->is_free) {

        // 1. Update Size (Absorb the neigbor)
        // New Size = My SIZE + Neighbor's Size + Neighbor's Header
        current_block->size += next_block->size + sizeof(block_t);

        // 2. Update Pointers (The bridge)
        // Skip over the 'next block' because its now part of the 'current_block'
        current_block->next = next_block->next;

        // 3. Update Reverse Pointer (If aplicable)
        // If there is a block AFTER the one we just ate, let i t know we are its new neighbor
        if (current_block->next != NULL) {
            current_block->next->prev = current_block;
        }
    }

    // get previous block (backward neighbor)
    block_t *prev_block = current_block->prev;

    // check if the previous block exists AND is free
    if (prev_block != NULL && prev_block->is_free) {

        // 1. Update Size
        prev_block->size += current_block->size + sizeof(block_t);

        // 2. Update Pointers
        prev_block->next = current_block->next;

        // 3. Update Reverse Pointer
        if (current_block->next != NULL) {
            current_block->next->prev = prev_block;
        }
    }
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
    balloc(10);

    return 0;
}
