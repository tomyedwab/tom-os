#include "kernel.h"

int tk_num_streams;
TKStreamInfo *tk_stream_table;

void streamInit() {
    // Create a table of streams
    tk_stream_table = (TKStreamInfo *)allocPage();

    tk_num_streams = 0;
}

TKStreamID streamCreate(unsigned int size) {
    TKStreamInfo *info;
    // Round up to the nearest 4k bytes
    int num_pages = (size + 0xfff) >> 12;
    int i;

    info = &tk_stream_table[tk_num_streams++];

    info->stream_id = tk_num_streams;
    info->read_owner = 0;
    info->write_owner = 0;
    info->size = size;
    info->num_pages = num_pages;
    info->buffer_ptr = heapAllocContiguous(num_pages);

    // Fill with empty identifier
    for (i = 0; i < size / 4; i++) {
        ((unsigned int *)info->buffer_ptr)[i] = ID_EMPTY;
    }

    kprintf("Created stream %d at %X, size %X, %d pages\n", info->stream_id, info->buffer_ptr, info->size, info->num_pages);

    return info->stream_id;
}

void streamMapReader(TKStreamID id, TKVProcID owner, unsigned int v_addr) {
    // TODO: Verify id is valid
    int i;
    TKStreamInfo *info = &tk_stream_table[id-1];
    info->read_owner = owner;
    info->read_vaddr = (void*)v_addr;
    for (i = 0; i < info->num_pages; i++) {
        procMapPage(owner, v_addr + (i << 12), (unsigned int)info->buffer_ptr + (i << 12));
    }
}

void streamMapWriter(TKStreamID id, TKVProcID owner, unsigned int v_addr) {
    // TODO: Verify id is valid
    int i;
    TKStreamInfo *info = &tk_stream_table[id-1];
    info->write_owner = owner;
    info->write_vaddr = (void*)v_addr;
    for (i = 0; i < info->num_pages; i++) {
        procMapPage(owner, v_addr + (i << 12), (unsigned int)info->buffer_ptr + (i << 12));
    }
}

void streamCreatePointer(TKStreamID id, TKStreamPointer *pointer) {
    // TODO: Verify id is valid
    TKStreamInfo *info = &tk_stream_table[id-1];
    pointer->buffer_ptr = info->buffer_ptr;
    pointer->buffer_size = info->size;
    pointer->cur_ptr = info->buffer_ptr;
}

