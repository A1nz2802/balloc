#include "allocator.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void test_large_allocation() {
    printf("Running Large Allocation Test (> 4096 bytes)...\n");

    // Request 10KB (More than 2 pages)
    void *ptr = balloc(10240);
    assert(ptr != NULL);

    verify_heap();

    free(ptr);
    verify_heap();

    printf("PASS ✅: Large allocation handled correctly.\n\n");
}

void test_heap_expansion() {
    printf("Running Heap Expansion Test...\n");

    // Fill the first page
    void *p1 = balloc(2000);
    void *p2 = balloc(2000); // Usually fills the page

    // Force expansion (Requesting more memory while previous is used)
    void *p3 = balloc(1000);

    assert(p1 && p2 && p3);
    verify_heap();

    free(p1);
    free(p2);
    free(p3);

    printf("PASS ✅: Heap expanded successfully.\n\n");
}

void test_reuse() {
    printf("Running Reuse Test (First-Fit Strategy)...\n");

    // 1. Allocate a block
    void *ptr1 = balloc(128);
    assert(ptr1 != NULL);

    // Save the address to compare later
    void *original_addr = ptr1;

    // 2. Free it
    free(ptr1);
    verify_heap();

    // 3. Request the same size again
    // The allocator should reuse the block we just freed.
    void *ptr2 = balloc(128);

    assert(ptr2 == original_addr && "Allocator did NOT reuse the freed block!");

    free(ptr2);
    verify_heap();

    printf("PASS ✅: Memory block reused successfully.\n\n");
}

void test_alignment() {
    printf("Running Alignment Test...\n");

    // Allocate weird sizes
    void *p1 = balloc(1);
    void *p2 = balloc(3);
    void *p3 = balloc(7);
    void *p4 = balloc(13);

    // Verify addresses are aligned to 8 bytes (assuming 64-bit architecture)
    // We cast to uintptr_t to perform bitwise operations on addresses
    assert(((uintptr_t)p1 % 8 == 0) && "Pointer p1 not aligned!");
    assert(((uintptr_t)p2 % 8 == 0) && "Pointer p2 not aligned!");
    assert(((uintptr_t)p3 % 8 == 0) && "Pointer p3 not aligned!");
    assert(((uintptr_t)p4 % 8 == 0) && "Pointer p4 not aligned!");

    free(p1);
    free(p2);
    free(p3);
    free(p4);

    verify_heap();
    printf("PASS ✅: All pointers correctly aligned to 8 bytes.\n\n");
}

void test_fragmentation_stress() {
    printf("Running Stress/Fragmentation Test...\n");

    const int NUM_PTRS = 50;
    void *ptrs[NUM_PTRS];

    // 1. Allocate many small blocks
    for (int i = 0; i < NUM_PTRS; i++) {
        ptrs[i] = balloc(i * 4 + 8); // Allocating 8, 12, 16...
    }
    verify_heap();

    // 2. Free every OTHER block (create Swiss Cheese fragmentation)
    // Frees indices: 0, 2, 4, 6...
    for (int i = 0; i < NUM_PTRS; i += 2) {
        free(ptrs[i]);
        ptrs[i] = NULL;
    }
    verify_heap();

    // 3. Allocate again to see if it fits in the holes
    for (int i = 0; i < NUM_PTRS; i += 2) {
        ptrs[i] = balloc(i * 4 + 8);
    }
    verify_heap();

    // 4. Free everything
    for (int i = 0; i < NUM_PTRS; i++) {
        if (ptrs[i] != NULL) {
            free(ptrs[i]);
        }
    }
    verify_heap();

    printf("PASS ✅: Stress test survived fragmentation.\n");
}

int main() {
    balloc(10);
    balloc(20);
    balloc(10);

    test_large_allocation();
    test_heap_expansion();

    test_reuse();
    test_alignment();
    test_fragmentation_stress();

    printf("\nAll tests passed! ✅\n");
    return 0;
}
