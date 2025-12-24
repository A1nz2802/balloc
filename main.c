#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define HEAP_SIZE 4096

void *heap_start = NULL;
void *heap_top = NULL;

void *bad_malloc(size_t size) {
    void *heap_end = (char *)heap_start + HEAP_SIZE;

    if ((char *)heap_top + size > (char *)heap_end) {
        printf("Heap overflow :P.\n");
        return NULL;
    }

    void *result = heap_top;
    heap_top = (char *)heap_top + size;

    return result;
}

void init_heap() {
    void *addr = mmap(
        NULL,
        HEAP_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);

    if (addr == MAP_FAILED) {
        printf("mmap failed to allocate initial heap memory.\n");
        exit(1);
    }

    heap_start = addr;
    heap_top = addr;
}

void debug_heap() {
    printf("Top: %p\n", heap_top);

    size_t used = (char *)heap_top - (char *)heap_start;
    size_t free = HEAP_SIZE - used;

    printf("Used: %zu bytes\n", used);
    printf("Free: %zu bytes\n\n", free);
}

int main() {
    init_heap();

    printf("--- Heap Status ---\n");
    printf("Start: %p\n", heap_start);
    printf("End: %p\n\n", (char *)heap_start + HEAP_SIZE);

    printf("Requesting 10 bytes ...\n");
    void *ptr = bad_malloc(10);
    if (ptr == NULL) {
        printf("Failed to allocate memory.\n");
    };
    debug_heap();

    printf("Requesting 5 bytes ...\n");
    void *ptr2 = bad_malloc(5);
    if (ptr2 == NULL) {
        printf("Failed to allocate memory.\n");
    };
    debug_heap();

    printf("Requesting 100000 bytes ...\n");
    void *ptr3 = bad_malloc(100000);
    if (ptr3 == NULL) {
        printf("Failed to allocate memory.\n");
    };
    debug_heap();
}
