unsigned int *page_directory;

void initVMPageTable(int prefix) {
    if (page_directory[prefix] & 1 > 0) {
        return;
    }
    int i;
    // Allocate a table on the heap
    unsigned int *table = (unsigned int *)allocPage();

    // Clear table
    for (i = 0; i < 1024; i++) {
        table[i] = 0;
    }

    page_directory[prefix] = ((unsigned int)table) & 0xfffff000 | 1;
}

void initVMPage(int prefix, int src, unsigned int dest) {
    unsigned int *table = (unsigned int *)(page_directory[prefix] & 0xfffff000);
    table[src] = dest & 0xfffff000 | 1;
}

// Initialize the page directory table
void initVMM() {
    int i;
    page_directory = (unsigned int*)allocPage();
    for (i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }

    initVMPageTable(0);
    for (i = 0; i < 1024; i++) {
        initVMPage(0, i, i << 12);
    }
}

void enableVMM() {
    unsigned int page_directory_addr = (unsigned int)page_directory;
    __asm__ __volatile__ (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%ebx\n"
        "or $0x80000000, %%ebx\n"
        "mov %%ebx, %%cr0\n"
        : : "r" (page_directory_addr) : "%ebx"
        );
    {
        unsigned int *testMemory = (unsigned int *)480;
        *testMemory = 0x12345678;
    }
}

/*
void disableVMM() {
    __asm__ (
     "mov %%cr0, %%eax\n"
     "and $0x7fffffff, %%eax\n"
     "mov %%eax, %%cr0\n"
     : : : "%eax"
   );
}
*/

/*
void testVMM() {
    const char *ptr;
    char *ptr2;

    // Some arbitrary memory we can access
    unsigned int *testMemory = (unsigned int *)0x1e0;
    unsigned int *mappedMemory = (unsigned int *)0x004001e0; // -> 0x1e0

    *testMemory = 0x12345678;
    *mappedMemory = 0x10101010;

    printStr("a: "); printInt(page_directory[0]); printStr("\n");
    printStr("b: "); printInt(*(int *)0x00021000); printStr("\n");
    printStr("1: "); printInt(*testMemory); printStr("\n");
    printStr("2: "); printInt(*mappedMemory); printStr("\n");

    enableVMM();

    initVMPageTable(1);
    initVMPage(1, 0, 0); // Map 0x0040000-0x00401000 to 0x00000000

    printStr("1: "); printInt(*testMemory); printStr("\n");
    printStr("2: "); printInt(*mappedMemory); printStr("\n");
    *mappedMemory = 0x20202020;

    printStr("1: "); printInt(*testMemory); printStr("\n");
    printStr("2: "); printInt(*mappedMemory); printStr("\n");
}
*/
