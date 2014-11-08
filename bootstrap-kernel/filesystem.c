#include "kernel.h"
#include <tomfs.h>

typedef struct BlockCacheEntry {
    unsigned int block;
    char *buffer;
} BlockCacheEntry;

BlockCacheEntry *block_cache;
int block_cache_size;

int read_fn(struct TFS *fs, char *buf, unsigned int block) {
    int i;
    // Check the cache
    for (i = 0; i < block_cache_size; i++) {
        if (block_cache[i].block == block) {
            char *buffer = block_cache[i].buffer;
            kprintf("Found cached block %d\n", block);
            for (i = 0; i < 4096; i++) {
                buf[i] = buffer[i];
            }
            return 0;
        }
    }
    // 8 sectors per block in our filesystem
    kprintf("Reading block %d\n", block);
    if (loadFromDisk(34 + (block << 3), 8, buf) == 1) {
        char *buffer = allocPage();
        for (i = 0; i < 4096; i++) {
            buffer[i] = buf[i];
        }
        block_cache[block_cache_size].block = block;
        block_cache[block_cache_size].buffer = buffer;
        block_cache_size++;
        kprintf("Read block %d\n", block);
        return 0;
    }
    kprintf("Failed to read block %d.\n", block);
    return -1;
}

TFS gTFS;

void initFilesystem() {
    FileHandle *handle_storage = (FileHandle*)heapAllocContiguous(4);

    block_cache = allocPage();

    gTFS.read_fn = read_fn;

    // We really don't want a write function at this moment
    tfsInit(&gTFS, handle_storage, 4*4096 / TFS_FILE_HANDLE_SIZE);

    if (tfsOpenFilesystem(&gTFS) != 0) {
        kprintf("Failed to open filesystem!\n");
        halt();
    }
}
