// Dead simple heap allocator returns whole 4k pages
#include "kernel.h"

int current_page;

void initHeap() {
    // Start a this page and keep handing out pages going up the address space
    current_page = KERNEL_HEAP_ADDR;
}

void *allocPage() {
    int i;
    int page = current_page;
    for (i = 0; i < 4096; i++)
        ((char*)page)[i] = 0;
    current_page += 0x1000;
    return (void *)page;
}

void *heapAllocContiguous(int num_pages) {
    int page = current_page;
    current_page += num_pages << 12;
    return (void *)page;
}
