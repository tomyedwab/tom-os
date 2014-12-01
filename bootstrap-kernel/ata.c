#include "kernel.h"

unsigned int BASE_ADDRESS = 0;
unsigned int CONTROL_ADDRESS = 0;

int waitForATABusy() {
    unsigned char b;
    do {
        sleep(100);
        b = inb(BASE_ADDRESS + 7);
        if ((b & 0x21) != 0) {
            unsigned char e = inb(BASE_ADDRESS + 1);
            //kprintf("Drive error! %02x %02x\n", b, e);
            return 0;
        }
    } while ((b & 0x80) != 0);
    return b;
}

void ataDetectDevice(int slave) {
    unsigned int bmideBase, irqNum;
    pciGetIDEConfig(slave, &BASE_ADDRESS, &CONTROL_ADDRESS, &bmideBase, &irqNum);
}

int loadFromDisk(int LBA, int sectorCount, unsigned char *buffer) {
    int slavebit = 0;
    int index;
    unsigned char status;

    ataDetectDevice(slavebit);

    // Select drive
    //printStr("Reading from ATA...\n");
    waitForATABusy();
    outb(BASE_ADDRESS + 6, (0xE0 | (slavebit <<  4) | (LBA >> 24 & 0x0F))); 
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
            sleep(100);
        }
    }
    return 1;
}

int writeToDisk(int LBA, int sectorCount, unsigned char *buffer) {
    int slavebit = 0;
    int index;
    unsigned char status;

    ataDetectDevice(slavebit);

    // Select drive
    //printStr("Reading from ATA...\n");
    waitForATABusy();
    outb(BASE_ADDRESS + 6, (0xE0 | (slavebit <<  4) | (LBA >> 24 & 0x0F))); 
    outb(BASE_ADDRESS + 2, (unsigned char)sectorCount);
    outb(BASE_ADDRESS + 3, LBA & 0xff); 
    outb(BASE_ADDRESS + 4, (LBA >> 8) & 0xff);
    outb(BASE_ADDRESS + 5, (LBA >> 16) & 0xff);
    //issue a write sectors command
    outb(BASE_ADDRESS + 7, 0x30);
    //printStr("Requested...\n");
    status = waitForATABusy();
    if (status == 0) { return 0; }
    //printStr("Got data!\n");
    for (index = 0; index < 256 * sectorCount; ) {
        outw(BASE_ADDRESS, ((unsigned short *)buffer)[index]);
        index++;
        if (index % 256 == 0) {
            status = waitForATABusy();
            if (status == 0) { return 0; }
            sleep(100);
        }
    }
    status = waitForATABusy();
    if (status == 0) { return 0; }
    outb(BASE_ADDRESS + 7, 0xe7);
    status = waitForATABusy();
    if (status == 0) { return 0; }
    return 1;
}
