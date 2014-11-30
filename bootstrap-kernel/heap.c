// Dead simple heap allocator returns whole 4k pages
#include "kernel.h"

int current_page;

// Memory map header consists of:
// 1. Total memory assigned to heap = 8 bytes
// 2. Total unassigned memory remaining in heap = 8 bytes
// 3. Up to 10 segments
//    base = 8 bytes, length = 8 bytes, bitmap offset = 4 bytes
//    = 200 bytes
// 4. Bitmap, 2 bits per 256-page page block:
//      (00 = empty, 01 = not full, 11 = full)
//    8 GB of memory = 8*1024*1024*1024*2/4096/256 = 16384 bits = 2048 bytes
//  = 2264 bytes total

#define MAX_HEAP_MEMORY_SEGMENTS 10

typedef struct {
    unsigned long base;
    unsigned long length;
    unsigned int bitmap_offset;
} TKHeapSegmentInfo;

typedef struct {
    long heap_memory;
    long remaining_memory;
    TKHeapSegmentInfo segments[MAX_HEAP_MEMORY_SEGMENTS];
    char memory_bitmap[];
} TKHeapHeader;

// Each 256-page block begins with one page of page information
// 255 pages * 16 bytes = 4080 bytes
// TODO: Track more information so we can maybe someday deallocate memory
typedef struct {
    char type;              // Currently 0 = available, 1 = allocated
    char reserved[15];
} TKHeapPageInfo;

// A header for a page used in a small allocator pool
typedef struct TKSmallAllocatorPage {
    int size;                   // Size of the objects in the pool
    int allocated;              // Number of allocated entries in this page
    TKVProcID owner;            // Process that owns this page
    void *vaddr;                // Virtual address in process memory for this page
    TKSmallAllocatorPage *next; // Pointer to the next page in the pool
} TKSmallAllocatorPage;

TKHeapHeader *heap_header = (TKHeapHeader*)0x090000;

// Iterators that we keep around to continuously traverse the free list
int heap_segment = 0;
int heap_byte = 0;

// Small allocator types
TKSmallAllocatorPage *stream_allocator;

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
        unsigned long base = *(unsigned long*)(0x88010 + 24 * i);
        unsigned long length = *(unsigned long*)(0x88018 + 24 * i);
        long type = *(int*)(0x88020 + 24 * i);

        // Align to 4096-byte boundary
        if ((base & 0x3ff) != 0) {
            base = (base + 0x400) & ~0x3ff;
        }
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

    heap_header->segments[0].bitmap_offset = 0;
    for (i = 0; i <= cur_segment; i++) {
        int idx;
        if (i > 0) {
            // Bitmap size = length / 4096 / 256 / 4 = length >> 22
            heap_header->segments[i].bitmap_offset = heap_header->segments[i-1].bitmap_offset + (heap_header->segments[i-1].length >> 22);
        }
        kprintf("Segment %d: %016X - %016X (%d)\n", i, heap_header->segments[i].base, heap_header->segments[i].base+heap_header->segments[i].length, heap_header->segments[i].bitmap_offset);

        // Clear the bitmap
        for (idx = (heap_header->segments[i].length >> 22) - 1; idx >= 0; --idx) {
            heap_header->memory_bitmap[heap_header->segments[i].bitmap_offset + idx] = 0;
        }
    }

    kprintf("Found %d kbytes of memory (%d kbytes for heap)\n",
            total_memory >> 10, heap_header->heap_memory >> 10);

    // 1 page out of every 256 is used for page group header
    heap_header->remaining_memory = (heap_header->heap_memory * 255) / 256;

    // Start a this page and keep handing out pages going up the address space
    current_page = KERNEL_HEAP_ADDR;
}

void *_allocPageFromGroup(int segment, int page_group, int bitmap_flags) {
    int page;
    TKHeapPageInfo *group_header = (TKHeapPageInfo *)(heap_header->segments[segment].base + page_group * 256 * 4096);

    if (bitmap_flags == 3) {
        return 0; // Group is full
    }
    if (bitmap_flags == 0) {
        // The page group is uninitialized, mark all the pages available
        for (page = 0; page < 255; page++) {
            group_header[page].type = 0; // available
        }

        // Update the allocation bitmap
        heap_header->memory_bitmap[heap_header->segments[segment].bitmap_offset + (page_group / 4)] |= (1 << ((page_group % 4) * 2));
    }

    for (page = 0; page < 255; page++) {
        if (group_header[page].type == 0) {
            void *ret;
            // We have an uninitialized page! Grab it.
            group_header[page].type = 1;
            
            heap_header->remaining_memory -= 4096;

            ret = (void *)(heap_header->segments[segment].base + (page_group * 256 + page + 1) * 4096);

            //kprintf("ALLOC %d:%d:%d (%X)\n", segment, page_group, page, ret);

            return ret;
        }
    }

    // We must be full; update the allocation bitmap
    heap_header->memory_bitmap[heap_header->segments[segment].bitmap_offset + (page_group / 4)] |= (1 << ((page_group % 4) * 2 + 1));
    return 0; // Group is full
}

// TODO: Rename heapAllocPage()
void *allocPage() {
    int heap_segment_count = heap_header->segments[heap_segment].length >> 22;
    while (1) {
        char bitmap_byte;
        void *ret;

        // If there's no segment here, skip to the next one
        if (heap_header->segments[heap_segment].base == 0) {
            heap_segment++;
            if (heap_segment == MAX_HEAP_MEMORY_SEGMENTS) {
                heap_segment = 0;
            }
            heap_segment_count = heap_header->segments[heap_segment].length >> 22;
            continue;
        }

        // If we're past the end of the bitmap, skip to the next segment
        if (heap_byte >= heap_segment_count) {
            heap_segment++;
            if (heap_segment == MAX_HEAP_MEMORY_SEGMENTS) {
                heap_segment = 0;
            }
            heap_segment_count = heap_header->segments[heap_segment].length >> 22;
            continue;
        }

        // Test the bitmap
        bitmap_byte = heap_header->memory_bitmap[heap_header->segments[heap_segment].bitmap_offset + heap_byte];

        // Page group 1
        ret = _allocPageFromGroup(heap_segment, (heap_byte * 4), bitmap_byte & 0x3);
        if (ret) { return ret; }

        // Page group 2
        ret = _allocPageFromGroup(heap_segment, (heap_byte * 4) + 1, (bitmap_byte>>2) & 0x3);
        if (ret) { return ret; }

        // Page group 3
        ret = _allocPageFromGroup(heap_segment, (heap_byte * 4) + 2, (bitmap_byte>>4) & 0x3);
        if (ret) { return ret; }

        // Page group 4
        ret = _allocPageFromGroup(heap_segment, (heap_byte * 4) + 3, (bitmap_byte>>6) & 0x3);
        if (ret) { return ret; }

        // None found; move to next set of page groups
        bitmap_byte++;
    }

    kprintf("Ran out of memory!\n");
    halt();
    return 0;
}

void *heapVirtAllocContiguous(int num_pages) {
    int page;
    void *start;
    for (page = 0; page < num_pages; page++) {
        void *addr = allocPage();
        void *vaddr = procMapHeapPage(1, addr);
        if (page == 0) {
            start = vaddr;
        }
    }
    return start;
}

void *heapSmallAlloc(TKSmallAllocatorPage **allocator_pool, TKVProcID owner, int size) {
    int num_per_page = (4096 - sizeof(TKSmallAllocatorPage)) / size;
    if (num_per_page < 2) {
        // This is not a small allocation!
        return 0;
    }
    while (1) {
        if (*allocator_pool) {
            // Check that size matches
            if ((*allocator_pool)->size != size) {
                return 0;
            }

            // Check if there is available space in the current page
            if ((*allocator_pool)->allocated < num_per_page) {
                void *ret = (void*)&((char*)*allocator_pool)[sizeof(TKSmallAllocatorPage) + (*allocator_pool)->allocated * size];
                (*allocator_pool)->allocated++;
                kprintf("SMALLOC: Allocated %X (%d) in page %X\n", ret, size, *allocator_pool);
                return ret;
            }

            // No space. Advance to next
            allocator_pool = &((*allocator_pool)->next);
        }

        // If there is no next, create one
        if (!*allocator_pool) {
            *allocator_pool = (TKSmallAllocatorPage *)allocPage();
            if (!*allocator_pool) {
                return 0;
            }
            (*allocator_pool)->size = size;
            (*allocator_pool)->allocated = 0;
            (*allocator_pool)->owner = owner;
            (*allocator_pool)->vaddr = procMapHeapPage(owner, *allocator_pool);
            kprintf("SMALLOC: Allocated page for size %d to %d at %X\n", size, owner, (*allocator_pool)->vaddr);
        }
    }
}

void *heapSmallAllocGetVAddr(void *real_addr) {
    TKSmallAllocatorPage *page = (TKSmallAllocatorPage*)(((int)real_addr) & ~0x3ff);
    int offset = (int)real_addr - (int)page;
    return page->vaddr + offset;
}

