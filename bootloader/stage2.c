#include <tomfs.h>

void initScreen();
int kprintf(const char *fmt, ...);

void sleep(unsigned int count) {
    int i;
    for (i = 0; i < count * 1000; i++) {
        i <<= 1;
        i >>= 1;
    }
}

int read_fn(struct TFS *fs, char *buf, unsigned int block) {
    // TODO: Cache these
    // 8 sectors per block in our filesystem
    if (loadFromDisk(34 + (block << 3), 8, buf) == 1) {
        unsigned int *words = buf;
        kprintf("Read block %d\n", block);
        return 0;
    }
    kprintf("Failed to read block %d.\n", block);
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
    kprintf("Bootloader stage 2 loaded.\n");

    tfs.read_fn = read_fn;
    // We really don't want a write function at this moment
    tfsInit(&tfs, (FileHandle*)handle_storage, 8);

    if (tfsOpenFilesystem(&tfs) != 0) {
        kprintf("Failed to open filesystem!\n");
        while (1) {};
    }

    file = tfsOpenFile(&tfs, "", "kernel");
    if (file == NULL) {
        kprintf("Failed to open kernel!\n");
        while (1) {};
    }

    kprintf("Loading kernel...\n");
    // Read up to 480kb to the kernel base address 0x10000
    if (tfsReadFile(&tfs, file, (char*)0x10000, 480*1024, 0) <= 0) {
        kprintf("Failed to read kernel!\n");
        while (1) {};
    }
}

