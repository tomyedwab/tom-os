#include <tomfs.h>

void initScreen();

#define BLOCK_CACHE_ADDR 0x204000

// block_cache[idx] = the filesystem block index cached at address
// BLOCK_CACHE_ADDR + 4096*(idx+1)
unsigned int *block_cache;
int block_cache_size;

void sleep(unsigned int count) {
    int i, x;
    for (i = 0; i < count * 100; i++) {
        x += 123;
    }
}

int read_fn(struct TFS *fs, char *buf, unsigned int block) {
    int i;
    // Check the cache
    for (i = 0; i < block_cache_size; i++) {
        if (block_cache[i] == block) {
            char *cached_block = ((char*)BLOCK_CACHE_ADDR) + 4096 * (i+1);
            for (i = 0; i < 4096; i++) {
                buf[i] = cached_block[i];
            }
            return 0;
        }
    }
    // 8 sectors per block in our filesystem
    if (loadFromDisk(34 + (block << 3), 8, buf) == 1) {
        char *cached_block = ((char*)BLOCK_CACHE_ADDR) + 4096 * (block_cache_size+1);
        for (i = 0; i < 4096; i++) {
            cached_block[i] = buf[i];
        }
        block_cache[block_cache_size++] = block;
        return 0;
    }
    return -1;
}

void load_kernel() {
    // The job of the second stage bootloader is to find the kernel image,
    // load it into memory at address 0x008000, switch to protected mode,
    // and then jump to the kernel.
    TFS tfs;
    char handle_storage[8*TFS_FILE_HANDLE_SIZE];
    FileHandle *file;

    initScreen();
    printStr("Bootloader stage 2 loaded.\n");

    block_cache = (unsigned int*)BLOCK_CACHE_ADDR;
    block_cache_size = 0;

    tfs.read_fn = read_fn;
    // We really don't want a write function at this moment
    tfsInit(&tfs, (FileHandle*)handle_storage, 8);

    if (tfsOpenFilesystem(&tfs) != 0) {
        printStr("Failed to open filesystem!\n");
        while (1) {};
    }

    file = tfsOpenFile(&tfs, "", "kernel");
    if (file == NULL) {
        printStr("Failed to open kernel!\n");
        while (1) {};
    }

    printStr("Loading kernel...\n");
    // Read up to 480kb to the kernel base address 0x10000
    if (tfsReadFile(&tfs, file, (char*)0x10000, 480*1024, 0) <= 0) {
        printStr("Failed to read kernel!\n");
        while (1) {};
    }

    printStr("Starting kernel...\n");
}

