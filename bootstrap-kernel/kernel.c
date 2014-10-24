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

    initPIC();
    printStr("[OK] PIC\n");

    initInterrupts();
    printStr("[OK] Interrupts\n");

    keyboardInit();
    printStr("[OK] Keyboard\n");

    streamInit();
    printStr("[OK] Streams\n");

    buffer = (unsigned char *)heapAllocContiguous(3);
    if (loadFromDisk(34, 20, buffer) == 1) {
        unsigned int ret;
        printStr("Loaded app from disk.\n");
        ret = loadELF(buffer);
        printStr("App ended: "); printInt(ret); printStr("\n");
    }

    return;
}
