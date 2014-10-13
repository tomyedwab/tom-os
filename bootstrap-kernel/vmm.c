#include "kernel.h"

TKVPageDirectory vmmCreateDirectory() {
    int i;
    TKVPageDirectory directory = (TKVPageDirectory)allocPage();
    for (i = 0; i < 1024; i++) {
        directory[i] = 0;
    }

    return directory;
}

TKVPageTable vmmGetOrCreatePageTable(TKVPageDirectory directory, int prefix) {
    if (directory[prefix] & 1 > 0) {
        return (TKVPageTable)(directory[prefix] & 0xfffff000);
    }
    int i;
    // Allocate a table on the heap
    TKVPageTable table = (TKVPageTable)allocPage();

    // Clear table
    for (i = 0; i < 1024; i++) {
        table[i] = 0;
    }

    directory[prefix] = ((unsigned int)table) & 0xfffff000 | 1;
    return table;
}

void vmmSetPage(TKVPageTable table, int src, unsigned int dest) {
    table[src] = dest & 0xfffff000 | 1;
}

void vmmMapPage(TKVPageDirectory directory, unsigned int src, unsigned int dest) {
    TKVPageTable table = vmmGetOrCreatePageTable(directory, src >> 22);
    vmmSetPage(table, (src >> 12) & 0x3ff, dest);
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

