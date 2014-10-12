#include "kernel.h"

#define BASE_ADDRESS 0x1F0

void sleep() {
    int counter = 0;
    int test = 0;
    for (counter = 0; counter < 255; counter++) {
        test += 1;
    }
}

int waitForATABusy() {
    unsigned char b;
    do {
        sleep();
        b = inb(BASE_ADDRESS + 7);
        if ((b & 0x21) != 0) {
            unsigned char e = inb(BASE_ADDRESS + 1);
            printStr("Drive error! "); printByte(b); printStr(" "); printByte(e); printStr("\n");
            return 0;
        }
    } while ((b & 0x80) != 0);
    return b;
}


int loadFromDisk(int LBA, int sectorCount, unsigned char *buffer) {
    int slavebit = 1;
    int index;
    unsigned char status;

    // Select drive
    printStr("Reading from ATA...\n");
    outb(BASE_ADDRESS + 6, (0xE0 | (slavebit <<  4) | (LBA >> 24 & 0x0F))); 
    sleep();
    outb(BASE_ADDRESS + 2, (unsigned char)sectorCount);
    outb(BASE_ADDRESS + 3, LBA & 0xff); 
    outb(BASE_ADDRESS + 4, (LBA >> 8) & 0xff);
    outb(BASE_ADDRESS + 5, (LBA >> 16) & 0xff);
    //issue a read sectors command
    outb(BASE_ADDRESS + 7, 0x20);
    //printStr("Requested...\n");
    status = waitForATABusy();
    if (status == 0) { return 0; }
    //printStr("Got data!\n");
    for (index = 0; index < 256 * sectorCount; ) {
        ((unsigned short *)buffer)[index] = inw(BASE_ADDRESS);
        index++;
        if (index % 256 == 0) {
            status = waitForATABusy();
            if (status == 0) { return 0; }
            sleep();
        }
    }
    return 1;
}
