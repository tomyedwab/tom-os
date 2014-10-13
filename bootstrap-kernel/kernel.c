#include "kernel.h"

void main() {
    int i;
    unsigned char *buffer;
    initScreen();
    printStr("Bootstrap kernel loaded.\n");

    initHeap();
    printStr("[OK] Heap\n");

    // Initialize the kernel process & switch to the kernel VMM directory
    procInitKernel();
    printStr("[OK] Kernel process\n");

    gdtInit();
    printStr("[OK] GDT\n");

    initInterrupts();
    printStr("[OK] Interrupts\n");

    buffer = (unsigned char *)allocPage();
    if (loadFromDisk(32, 8, buffer) == 1) {
        unsigned int ret;
        printStr("Loaded app from disk.\n");
        ret = loadELF(buffer);
        printStr("App ended: "); printInt(ret); printStr("\n");
    }

    return;
}
