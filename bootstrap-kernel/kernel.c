#include "kernel.h"

int main() {
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
        printStr("Loaded app from disk.\n");
        loadELF(buffer);
        printStr("Back to main.\n");
    }

    // Infinite loop!
    halt();
    return 0;
}
