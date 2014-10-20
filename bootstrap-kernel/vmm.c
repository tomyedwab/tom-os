#include "kernel.h"

TKVPageDirectory vmmCreateDirectory() {
    int i;
    TKVPageDirectory directory = (TKVPageDirectory)allocPage();
    for (i = 0; i < 1024; i++) {
        directory[i] = 0;
    }

    return directory;
}

TKVPageTable vmmGetOrCreatePageTable(TKVPageDirectory directory, unsigned int prefix, int user) {
    if (directory[prefix] & 1 > 0) {
        if (user != 0) {
            directory[prefix] |= 0x4;
        }
        return (TKVPageTable)(directory[prefix] & 0xfffff000);
    }
    int i;
    // Allocate a table on the heap
    TKVPageTable table = (TKVPageTable)allocPage();

    // Clear table
    for (i = 0; i < 1024; i++) {
        table[i] = 0;
    }

    // Set "present" and "read/write" bits
    directory[prefix] = ((unsigned int)table) & 0xfffff000 | 0x3 | ((user & 0x1) << 2);
    return table;
}

void vmmSetPage(TKVPageTable table, int src, unsigned int dest, int user) {
    // Set "present" and "read/write" bits
    table[src] = dest & 0xfffff000 | 0x3 | ((user & 0x1) << 2);
}

void vmmMapPage(TKVPageDirectory directory, unsigned int src, unsigned int dest, int user) {
    TKVPageTable table = vmmGetOrCreatePageTable(directory, src >> 22, user);
    vmmSetPage(table, (src >> 12) & 0x3ff, dest, user);
}

void vmmSwap(TKVPageDirectory directory) {
    __asm__ __volatile__ (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%ebx\n"
        "or $0x80000000, %%ebx\n"
        "mov %%ebx, %%cr0\n"
        : : "r" (directory) : "%ebx"
        );
}

