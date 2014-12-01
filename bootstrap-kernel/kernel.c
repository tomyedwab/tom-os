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

    pciListDevices();
    printStr("[OK] PCI\n");

    initFilesystem();
    printStr("[OK] Filesystem\n");

    if (loadELF("/bin", "init.elf") != 0) {
        halt();
    }
    printStr("[OK] Init\n");

    // TODO: Wait for shutdown signal
    while (1) {
    }

    return;
}
