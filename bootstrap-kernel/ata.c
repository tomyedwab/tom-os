#include "kernel.h"

#define BASE_ADDRESS 0x1F0

int waitForATABusy() {
    unsigned char b;
    do {
        sleep(100);
        b = inb(BASE_ADDRESS + 7);
        if ((b & 0x21) != 0) {
            unsigned char e = inb(BASE_ADDRESS + 1);
            kprintf("Drive error! %02x %02x\n", b, e);
            return 0;
        }
    } while ((b & 0x80) != 0);
    return b;
}

int loadFromDisk(int LBA, int sectorCount, unsigned char *buffer) {
    int slavebit = 0;
    int index;
    unsigned char status;

    // Select drive
    printStr("Reading from ATA...\n");
    outb(BASE_ADDRESS + 6, (0xE0 | (slavebit <<  4) | (LBA >> 24 & 0x0F))); 
    sleep(100);
    outb(BASE_ADDRESS + 2, (unsigned char)sectorCount);
    outb(BASE_ADDRESS + 3, LBA & 0xff); 
    outb(BASE_ADDRESS + 4, (LBA >> 8) & 0xff);
    outb(BASE_ADDRESS + 5, (LBA >> 16) & 0xff);
    //issue a read sectors command
    outb(BASE_ADDRESS + 7, 0x20);
    printStr("Requested...\n");
    status = waitForATABusy();
    if (status == 0) { return 0; }
    printStr("Got data!\n");
    for (index = 0; index < 256 * sectorCount; ) {
        ((unsigned short *)buffer)[index] = inw(BASE_ADDRESS);
        index++;
        if (index % 256 == 0) {
            status = waitForATABusy();
            if (status == 0) { return 0; }
            sleep(100);
        }
    }
    return 1;
}
