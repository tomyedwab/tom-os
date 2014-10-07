#include "kernel.h"

int main() {
    int i;
    unsigned char *buffer;
    initScreen();
    printStr("Bootstrap kernel loaded.\n");

    initInterrupts();

    initHeap();

    // Initialize the kernel process & switch to the kernel VMM directory
    procInitKernel();

    buffer = (unsigned char *)allocPage();
    if (loadFromDisk(32, 8, buffer) == 1) {
        printStr("Loaded app from disk.\n");
        loadELF(buffer);
    }

    // Infinite loop!
    while (1) {
    }
    return 0;
}
