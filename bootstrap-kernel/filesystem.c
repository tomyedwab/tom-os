#include "kernel.h"
#include <tomfs.h>

int read_fn(struct TFS *fs, char *buf, unsigned int block) {
    // TODO: Cache these
    // 8 sectors per block in our filesystem
    kprintf("Reading block %d\n", block);
    if (loadFromDisk(34 + (block << 3), 8, buf) == 1) {
        unsigned int *words = buf;
        kprintf("Read block %d\n", block);
        return 0;
    }
    kprintf("Failed to read block %d.\n", block);
    return -1;
}

TFS gTFS;

void initFilesystem() {
    FileHandle *handle_storage = (FileHandle*)heapAllocContiguous(4);

    gTFS.read_fn = read_fn;

    // We really don't want a write function at this moment
    tfsInit(&gTFS, handle_storage, 4*4096 / TFS_FILE_HANDLE_SIZE);

    if (tfsOpenFilesystem(&gTFS) != 0) {
        kprintf("Failed to open filesystem!\n");
        halt();
    }
}
