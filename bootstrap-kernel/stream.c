#include "kernel.h"

int tk_num_streams;
TKStreamInfo *tk_stream_table;

void streamInit() {
    // Create a table of streams
    tk_stream_table = (TKStreamInfo *)allocPage();

    tk_num_streams = 0;
}

/**
 * Create a new stream between processes:
 *   1) Create a ring buffer to hold the data
 *   2) Map the buffer to read_vaddr and write_vaddr in the respective process
 *      memory spaces
 *   3) Initialize the stream pointers residing in shared memory
 */
TKStreamID streamCreate(
        unsigned int size,
        TKVProcID read_owner, unsigned int read_vaddr, TKStreamPointer *read_ptr,
        TKVProcID write_owner, unsigned int write_vaddr, TKStreamPointer *write_ptr) {
    TKStreamInfo *info;
    // Round up to the nearest 4k bytes
    int num_pages = (size + 0xfff) >> 12;
    int i;

    info = &tk_stream_table[tk_num_streams++];

    info->stream_id = tk_num_streams;
    info->read_owner = read_owner;
    info->write_owner = write_owner;
    info->size = size;
    info->num_pages = num_pages;
    info->buffer_ptr = heapAllocContiguous(num_pages);

    // Fill with empty identifier
    for (i = 0; i < size / 4; i++) {
        ((unsigned int *)info->buffer_ptr)[i] = TKS_ID_EMPTY;
    }

    // Map the pages into the user memory spaces
    for (i = 0; i < info->num_pages; i++) {
        procMapPage(read_owner, read_vaddr + (i << 12), (unsigned int)info->buffer_ptr + (i << 12));
        procMapPage(write_owner, write_vaddr + (i << 12), (unsigned int)info->buffer_ptr + (i << 12));
    }
    // Map the first page again after the last page, so writes off the end can
    // magically just wrap around
    procMapPage(read_owner, read_vaddr + (i << 12), (unsigned int)info->buffer_ptr);
    procMapPage(write_owner, write_vaddr + (i << 12), (unsigned int)info->buffer_ptr);

    // Initialize the stream pointers
    streamInitStreams(read_ptr, write_ptr, read_vaddr, write_vaddr, size);

    kprintf("Created stream %d at %X, size %X, %d pages\n", info->stream_id, info->buffer_ptr, info->size, info->num_pages);
    kprintf(" -> Mapped reader %d to address %X\n", read_owner, read_vaddr);
    kprintf(" -> Mapped writer %d to address %X\n", write_owner, write_vaddr);

    return info->stream_id;
}

