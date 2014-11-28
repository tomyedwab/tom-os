#include "kernel.h"
#include "tomfs.h"

#define LOADER_VADDR_BASE 0x2000000

typedef struct {
    unsigned int name;
    unsigned int type;
    unsigned int flags;
    unsigned int addr;
    unsigned int offset;
    unsigned int size;
    unsigned int link;
    unsigned int info;
    unsigned int addralign;
    unsigned int entsize;
} ELFSection;

typedef struct {
    unsigned int type;
    unsigned int offset;
    unsigned int v_addr;
    unsigned int p_addr;
    unsigned int filesz;
    unsigned int memsz;
    unsigned int flags;
    unsigned int align;
} ELFProgramHeader;

typedef struct {
    unsigned char magic[4];
    unsigned char byte_width;
    unsigned char endian;
    unsigned char version;
    unsigned char abi;
    unsigned char abi_version;
    unsigned char unused_1[7];
    unsigned short type;
    unsigned short machine;
    unsigned int version2;
    void (*entry)();
    unsigned int phoff;
    unsigned int shoff;
    unsigned int flags;
    unsigned short ehsize;
    unsigned short phentsize;
    unsigned short phnum;
    unsigned short shentsize;
    unsigned short shnum;
    unsigned short shstrndx;
} ELFHeader;

int loadELF(const char *path, const char *file_name) {
    int i, j, ret;
    ELFHeader *header;
    ELFSection *stringTable;
    const char *strings;
    TKVProcID proc_id;
    FileHandle *dir, *file;
    char *buffer;
    int mode, block_idx, size;

    if ((dir = tfsOpenPath(&gTFS, path)) == NULL) {
        kprintf("Could not find path %s!\n", path);
        return -1;
    }
    if (tfsFindEntry(&gTFS, dir, file_name, &mode, &block_idx, &size) != 0) {
        kprintf("Could not find file %s!\n", file_name);
        tfsCloseHandle(dir);
        return -1;
    }
    buffer = heapAllocContiguous((size + 4095) / 4096);
    header = (ELFHeader *)buffer;
    tfsCloseHandle(dir);

    if ((file = tfsOpenFile(&gTFS, path, file_name)) == NULL) {
        kprintf("Could not open file %s!", file_name);
        return -1;
    }

    if (tfsReadFile(&gTFS, file, buffer, size, 0) < size) {
        kprintf("Could not read file %s!", file_name);
        return -1;
    }

    tfsCloseHandle(file);
    
    if (header->magic[0] != 0x7F || header->magic[1] != 'E' ||
        header->magic[2] != 'L' || header->magic[3] != 'F') {
        printStr("Magic number invalid!");
        return -1;
    }

    proc_id = procInitUser(header->entry);

    //kprintf("String table: %04x\n", header->shstrndx);
    stringTable = (ELFSection*)&buffer[header->shoff + header->shstrndx * header->shentsize];
    strings = &buffer[stringTable->offset];

    for (i = 0; i < header->phnum - 1; i++) {
        void *phMem;
        ELFProgramHeader *pheader = (ELFProgramHeader*)&buffer[header->phoff + i * header->phentsize];

        /*
        kprintf("Header %d: %d\n", i, pheader->type);
        kprintf("  vaddr %X paddr %X size %X / %X\n", pheader->v_addr, pheader->p_addr, pheader->filesz, pheader->memsz);
        */

        // Allocate a page at the requested vaddr
        phMem = allocPage();
        // Map it to the process's memory space
        procMapPage(proc_id, (unsigned int)pheader->v_addr, (unsigned int)phMem);
        // Map to kernel memory space as well
        procMapPage(1, LOADER_VADDR_BASE + (i<<12), (unsigned int)phMem);
        //kprintf("Mapped %X to %x, Kernel: %X\n", pheader->v_addr, (unsigned int)phMem, LOADER_VADDR_BASE + (i<<12));
    }

    for (i = 0; i < header->shnum - 1; i++) {
        ELFSection *section = (ELFSection*)&buffer[header->shoff + i * header->shentsize];
        unsigned int kernel_addr = 0;

        if (!section->addr) {
            continue;
        }

        for (j = 0; j < header->phnum - 1; j++) {
            ELFProgramHeader *pheader = (ELFProgramHeader*)&buffer[header->phoff + j * header->phentsize];
            if ((section->addr & 0xfffff000) == (pheader->v_addr & 0xfffff000)) {
                kernel_addr = (section->addr & 0xfff) + LOADER_VADDR_BASE + (j<<12);
            }
        }
        /*
        kprintf("Section %d: %s\n", i, &strings[section->name]);
        kprintf("  addr %X  kernel %X offset %X size %X\n", section->addr, kernel_addr, section->offset, section->size);
        */

        if (kernel_addr == 0) {
            kprintf("PROC ERROR: Cannot find page for address: %X\n", section->addr);
            return;
        }

        memcpy((void *)kernel_addr, (void *)&buffer[section->offset], section->size);
    }
    
    kprintf("Running ELF... %X\n", header->entry);

    // Switch to the process memory space & immediately jump
    procStart(proc_id, header->entry);
    return 0;
}
