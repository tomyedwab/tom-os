#include "kernel.h"

void main() {
    int i;
    unsigned char *buffer;
    initScreen();
    kprintf("Bootstrap kernel loaded.\n");

    initHeap();
    kprintf("[OK] Heap\n");

    // Initialize the kernel process & switch to the kernel VMM directory
    procInitKernel();
    kprintf("[OK] Kernel process\n");

    gdtInit();
    kprintf("[OK] GDT\n");

    initPIC();
    kprintf("[OK] PIC\n");

    initInterrupts();
    kprintf("[OK] Interrupts\n");

    keyboardInit();
    kprintf("[OK] Keyboard\n");

    streamInit();
    kprintf("[OK] Streams\n");

    pciListDevices();
    printStr("[OK] PCI\n");

    initFilesystem();
    kprintf("[OK] Filesystem\n");

    initLogger();
    // Henceforth all kprintf statements will go to the log file, not to the
    // screen
    kprintf("[OK] Logger\n");

    if (loadELF("/bin", "init.elf") != 0) {
        halt();
    }
    printStr("[OK] Init\n");

    // TODO: Wait for shutdown signal
    while (1) {
    }

    return;
}
