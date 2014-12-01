#include "kernel.h"
#include <tomfs.h>

typedef struct BlockCacheEntry {
    unsigned int block;
    char *buffer;
} BlockCacheEntry;

// TODO: Make this an LRU, don't stomp memory in case this gets past the size
// of the allocated size
BlockCacheEntry *block_cache;
int block_cache_size;

int read_fn(struct TFS *fs, char *buf, unsigned int block) {
    int i;
    // Check the cache
    for (i = 0; i < block_cache_size; i++) {
        if (block_cache[i].block == block) {
            char *buffer = block_cache[i].buffer;
            //kprintf("Found cached block %d\n", block);
            for (i = 0; i < 4096; i++) {
                buf[i] = buffer[i];
            }
            return 0;
        }
    }
    // 8 sectors per block in our filesystem
    if (loadFromDisk(34 + (block << 3), 8, buf) == 1) {
        char *buffer = allocPage();
        for (i = 0; i < 4096; i++) {
            buffer[i] = buf[i];
        }
        block_cache[block_cache_size].block = block;
        block_cache[block_cache_size].buffer = buffer;
        block_cache_size++;
        //kprintf("Read block %d\n", block);
        return 0;
    }
    kprintf("FS: Failed to read block %d.\n", block);
    return -1;
}

int write_fn(struct TFS *fs, char *buf, unsigned int block) {
    int i;
    if (writeToDisk(34 + (block << 3), 8, buf) != 1) {
        kprintf("FS: Failed to write block %d.\n", block);
        return -1;
    }
    // Update the cache
    for (i = 0; i < block_cache_size; i++) {
        if (block_cache[i].block == block) {
            int j;
            char *buffer = block_cache[i].buffer;
            for (j = 0; j < 4096; j++) {
                buffer[j] = buf[j];
            }
            break;
        }
    }
    return 0;
}

TFS gTFS;

void initFilesystem() {
    int i;
    FileHandle *handle_storage = (FileHandle*)heapVirtAllocContiguous(4);

    block_cache = allocPage();
    for (i = 0; i < block_cache_size; i++) {
        block_cache[i].block = 0xFFFFFFFF;
    }

    gTFS.read_fn = read_fn;
    gTFS.write_fn = write_fn;

    // We really don't want a write function at this moment
    tfsInit(&gTFS, handle_storage, 4*4096 / TFS_FILE_HANDLE_SIZE);

    if (tfsOpenFilesystem(&gTFS) != 0) {
        kprintf("Failed to open filesystem!\n");
        halt();
    }
}
