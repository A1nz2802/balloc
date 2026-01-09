#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "block.h"
#include "region.h"

#define PAGE_SIZE 4096

static region_t *global_region = NULL;

void init_region() {
    // request raw memory
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

    // initialize first block
    //
    // calculate where is the block start (just after the region)
    void *block_addr = (void *)global_region + sizeof(region_t);
    block_t *first_block = (block_t *)block_addr;

    first_block->size = PAGE_SIZE - sizeof(region_t) - sizeof(block_t);
    first_block->is_free = true;
    first_block->next = NULL;

    printf("Global Region start address: %p\n", (void *)global_region);
    printf("Global Region end address: %p\n\n", (void *)global_region + PAGE_SIZE);

    printf("Header Region start address: %p\n", (void *)global_region);
    printf("Header Region size in bytes: %lu\n", sizeof(global_region->size));
    printf("Header Region next in bytes: %zu\n", sizeof(region_t));
    printf("Header Region end address: %p\n", (void *)global_region + sizeof(region_t));
}

void *bad_malloc(size_t size) {
    if (size <= 0) {
        return NULL;
    }

    if (global_region == NULL) {
        init_region();
    }

    block_t *current = (block_t *)((void *)global_region + sizeof(region_t));

    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            current->is_free = false;

            return (void *)current + sizeof(block_t);
        }

        current = current->next;
    }

    printf("Out of memory!\n");
    return NULL;
}

int main() {
    init_region();

    bad_malloc(10);
    // bad_malloc(5);

    return 0;
}
