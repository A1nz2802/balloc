#include "allocator.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define MIN_ALLOC_SIZE 8 // Minimum bytes requited to justify a split

// Global head of the region list (to track all mmapped areas)
static region_t *global_regions_head = NULL;

// Global head of the block list (start of our free/used blocks)
static block_t *first_block = NULL;

/**
 * Aligns the given size to the nearest multiple of 8 bytes.
 *
 * This ensures that all returned pointers are aligned to 8-byte boundaries,
 * which is critical for performance and hardware requirements on 64-bit systems.
 *
 * Example: align(10) -> 16, align(8) -> 8, align(20) -> 24.
 */
static size_t align(size_t n) {
    return (n + 7) & ~7;
}

/**
 * Request a new memory region from the Operating System using mmap.
 *
 * This function handles the heap expansion when the allocator runs out of space.
 * It calculates the required number of pages, requests raw memory, and initializes
 * the necessary Region and Block headers.
 *
 * Key Logic:
 * 1. Calculates total size needed (User Request + Headers).
 * 2. Rounds up to the nearest Page Size multiple (e.g., 4096 bytes).
 * 3. Links the new block to the end of the existing list (Heap Extension).
 *
 * @param last_block Pointer to the tail of the current block list.
 * @param size The payload size requested by the user.
 * @return Pointer to the newly initialized block within the new region.
 */
static block_t *map_new_region(block_t *last_block, size_t size) {
    // 1. Calculate required size including headers
    size_t total_req_size = size + sizeof(region_t) + sizeof(block_t);

    // 2. Round up to the nearest multiple of PAGE_SIZE
    size_t num_pages = (total_req_size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t region_size = num_pages * PAGE_SIZE;

    // 3. Request memory from OS
    void *addr = mmap(
        NULL,
        region_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }

    // 4. Initialize Region Header
    region_t *region = (region_t *)addr;
    region->size = region_size;
    region->next = global_regions_head;
    global_regions_head = region; // Push to front of region list

    // 5. Initialize the Block Header inside this region
    block_t *new_block = (block_t *)((void *)region + sizeof(region_t));
    new_block->size = region_size - sizeof(region_t) - sizeof(block_t);
    new_block->is_free = true;
    new_block->next = NULL;
    new_block->prev = last_block; // Link back to the end of the old list

    // 6. Link the previous tail to this new block (The Expansion Logic)
    if (last_block != NULL) {
        last_block->next = new_block;
    } else {
        // If this is the very first region
        first_block = new_block;
    }

    return new_block;
}

/**
 * Splits a large free block into two smaller blocks to reduce internal fragmentation.
 *
 * 1. The first block (current) is shrunk to the requested size and returned to the user.
 * 2. The second block (new) is created from the remaining space and marked as free.
 *
 * @param current_block The large free block found by the First-Fit algorithm.
 * @param requested_size The exact size requested by the user.
 */
static void split_block(block_t *current_block, size_t requested_size) {
    // calculate pointer to the new block
    block_t *new_block = (block_t *)((void *)current_block + sizeof(block_t) + requested_size);

    // calculate the payload size available for this new split block
    size_t remaining_size = current_block->size - requested_size - sizeof(block_t);

    // initialize metadata for the new free block
    new_block->size = remaining_size;
    new_block->is_free = true;
    new_block->next = current_block->next;
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

// --- Public API ---

/**
 * Allocates a block of memory of at least 'size' bytes.
 *
 * Strategy: First-Fit.
 * 1. Iterates through the linked list to find the first free block that fits.
 * 2. If the block is significantly larger, it splits it (Block Splitting).
 * 3. If no block is found, it requests more memory from the OS (Heap Expansion).
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated payload, or NULL if system is out of memory.
 */
void *balloc(size_t size) {
    if (size <= 0)
        return NULL;

    size_t aligned_size = align(size);

    // Initialize first region if heap is empty
    if (first_block == NULL) {
        if (map_new_region(NULL, aligned_size) == NULL)
            return NULL;
    }

    block_t *current = first_block;
    block_t *last = NULL;

    // 1. Search for a free block (First Fit)
    while (current != NULL) {
        if (current->is_free && current->size >= aligned_size) {

            // Found one, check if we can split
            size_t remaining = current->size - aligned_size;

            // If reamaining space large enough for a new Header + Min Payload?
            if (remaining >= sizeof(block_t) + MIN_ALLOC_SIZE) {
                split_block(current, aligned_size);
            } else {
                // Not enough space to split effectively, just claim the whole block
                current->is_free = false;
            }

            return (void *)current + sizeof(block_t);
        }

        last = current; // Keep track of the last block seen
        current = current->next;
    }

    // 2. No space found? Expand Heap.
    // 'last' points to the tail of the list. We append the new region there.
    block_t *new_huge_block = map_new_region(last, aligned_size);

    if (new_huge_block == NULL) {
        return NULL; // OS is out of memory
    }

    // 3. Use the newly allocated block
    // Recursively call split (or just manual logic) since it's fresh and huge
    size_t remaining = new_huge_block->size - aligned_size;

    if (remaining >= sizeof(block_t) + MIN_ALLOC_SIZE) {
        split_block(new_huge_block, aligned_size);
    } else {
        new_huge_block->is_free = false;
    }

    return (void *)new_huge_block + sizeof(block_t);
}

/**
 * Frees a previously allocated memory block and merges it with neighbors (Coalescing).
 *
 * Logic:
 * 1. Marks the block as free.
 * 2. Forward Coalescing: Checks if the next block is free and merges with it.
 * 3. Backward Coalescing: Checks if the previous block is free and merges into it.
 *
 * This merging strategy prevents external fragmentation (many small free holes).
 *
 * @param payload Pointer to the memory payload to be freed.
 */
void free(void *payload) {
    // Safety check
    if (payload == NULL) {
        return;
    }

    // Get the block header
    // We move backwards from the payload pointer to find the block start
    block_t *current_block = (block_t *)((void *)payload - sizeof(block_t));

    // Mark as free
    current_block->is_free = true;

    // Get next block (neighbor)
    block_t *next_block = current_block->next;

    // Check if next block exists AND is free
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

    // Get previous block (backward neighbor)
    block_t *prev_block = current_block->prev;

    // Check if the previous block exists AND is free
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

/**
 * Debug helper: Verifies the integrity of the heap structure.
 *
 * Traverses the entire doubly linked list to ensure internal consistency:
 * 1. Link Integrity: checks if current->next->prev points back to current.
 * 2. Address Monotonicity: checks if the next block is physically after the current one.
 *
 * If corruption is detected, the program aborts with an assertion failure.
 */
void verify_heap() {
    block_t *curr = first_block;

    while (curr != NULL) {

        if (curr->next != NULL) {
            assert(curr->next->prev == curr && "Broken Link detected!");
            assert((void *)curr->next > (void *)curr && "Address corruption detected!");
        }

        curr = curr->next;
    }
}
