#include "memcpy.c"
#include "heap.c"
#include "vmm.c"
#include "screen.c"
#include "interrupt.c"
#include "ports.c"
#include "pci.c"
#include "ata.c"
#include "elf.c"
#include "syscall.c"

int main() {
    int i;
    unsigned char *buffer;
    video_memory = (char *)0xb8000;
    x = 0;
    y = 0;
    clearScreen();
    printStr("Bootstrap kernel loaded.\n");

    initHeap();

    // Initialize virtual memory
    initVMM();
    enableVMM();

    // Initialize interrupts
//    listPCIDevices();
//

    buffer = (unsigned char *)allocPage();
    if (loadFromDisk(32, 8, buffer) == 1) {
        printStr("Loaded app from disk.\n");
        initInterrupts(); // TODO: move this earlier
        loadELF(buffer);
    }

    // Infinite loop!
    while (1) {
    }
    return 0;
}
