// Dead simple heap allocator returns whole 4k pages
#include "kernel.h"

int current_page;

// Memory map consists of:
// Up to 10 segments
//   base = 8 bytes, length = 8 bytes
// = 160 bytes
// Bitmap, 1 bit per 256-page block
// 16 GB of memory = 16*1024*1024*1024/4096/256 = 16384 bits = 2048 bytes
// = 2208 bytes total

#define MAX_HEAP_MEMORY_SEGMENTS 10

typedef struct {
    long base;
    long length;
} TKHeapSegmentInfo;

typedef struct {
    long heap_memory;
    long remaining_memory;
    TKHeapSegmentInfo segments[MAX_HEAP_MEMORY_SEGMENTS];
    char memory_bitmap[];
} TKHeapHeader;

TKHeapHeader *heap_header = (TKHeapHeader*)0x090000;

void initHeap() {
    int i, mmap_length = *((int*)0x88000);
    int total_memory = 0;
    int cur_segment = 0;

    heap_header->heap_memory = 0;
    for (i = 0; i < 10; i++) {
        heap_header->segments[i].base = 0;
        heap_header->segments[i].length = 0;
    }

    kprintf("Memory map:\n");
    for (i = 0; i < mmap_length; i++) {
        long base = *(long*)(0x88010 + 24 * i);
        long length = *(long*)(0x88018 + 24 * i);
        long type = *(int*)(0x88020 + 24 * i);
        kprintf("%016X - %016X %08X\n", base, base+length-1, type);

        if (type == 1 || type == 3) {
            // Usable memory!
            total_memory += length;
            if (base >= 0x100000) {
                if (heap_header->segments[cur_segment].base == 0) {
                    heap_header->segments[cur_segment].base = base;
                    heap_header->segments[cur_segment].length = length;
                } else if (heap_header->segments[cur_segment].base + heap_header->segments[cur_segment].length == base) {
                    heap_header->segments[cur_segment].length += length;
                } else if (cur_segment < MAX_HEAP_MEMORY_SEGMENTS - 1) {
                    cur_segment++;
                    heap_header->segments[cur_segment].base = base;
                    heap_header->segments[cur_segment].length = length;
                } else {
                    // Too many segments. Give up.
                    break;
                }
                heap_header->heap_memory += length;
            }
        }
    }

    for (i = 0; i <= cur_segment; i++) {
        kprintf("Segment %d: %016X - %016X\n", i, heap_header->segments[i].base, heap_header->segments[i].length);
    }

    kprintf("Found %d kbytes of memory (%d kbytes for heap)\n",
            total_memory >> 10, heap_header->heap_memory >> 10);

    heap_header->remaining_memory = heap_header->heap_memory;

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
