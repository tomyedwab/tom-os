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
        TKVProcID read_owner, TKStreamPointer *read_ptr,
        TKVProcID write_owner, TKStreamPointer *write_ptr) {
    TKStreamInfo *info;
    // Round up to the nearest 4k bytes
    int num_pages = (size + 0xfff) >> 12;
    void *read_vaddr, *write_vaddr;
    void *first_buffer;
    int i;

    info = &tk_stream_table[tk_num_streams++];

    info->stream_id = tk_num_streams;
    info->read_owner = read_owner;
    info->read_ptr = read_ptr;
    info->write_owner = write_owner;
    info->write_ptr = write_ptr;
    info->size = size;
    info->num_pages = num_pages;

    // Map the pages into the user memory spaces
    for (i = 0; i < info->num_pages; i++) {
        int j;
        void *addr;
        void *buffer = allocPage();

        if (i == 0) { first_buffer = buffer; }

        // Fill with empty identifier
        for (j = 0; j < 1024; j++) {
            ((unsigned int *)buffer)[j] = TKS_ID_EMPTY;
        }

        addr = procMapHeapPage(read_owner, (unsigned int)buffer);
        if (i == 0) { read_vaddr = addr; }

        addr = procMapHeapPage(write_owner, (unsigned int)buffer);
        if (i == 0) { write_vaddr = addr; }
    }
    // Map the first page again after the last page, so writes off the end can
    // magically just wrap around
    procMapHeapPage(read_owner, (unsigned int)first_buffer);
    procMapHeapPage(write_owner, (unsigned int)first_buffer);

    // Initialize the stream pointers
    streamInitStreams(read_ptr, write_ptr, read_vaddr, write_vaddr, size);

    kprintf("Created stream %d, size %X, %d pages\n", info->stream_id, info->size, info->num_pages);
    kprintf(" -> Mapped reader %d (%X) to address %X\n", read_owner, read_ptr, read_vaddr);
    kprintf(" -> Mapped writer %d (%X) to address %X\n", write_owner, write_ptr, write_vaddr);

    return info->stream_id;
}

void streamSync(TKVProcID proc_id) {
    int stream;
    for (stream = 0; stream < tk_num_streams; ++stream) {
        TKStreamInfo *info = &tk_stream_table[stream];
        if (proc_id == 0 || proc_id == info->read_owner || proc_id == info->write_owner) {
            streamSyncStreams(info->read_ptr, info->write_ptr);
        }
    }
}
