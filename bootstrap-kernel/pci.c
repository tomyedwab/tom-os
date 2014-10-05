typedef struct {
    unsigned char bus;
    unsigned char slot;
    unsigned char func;
} PCIDevice;

PCIDevice ide;

unsigned int pciConfigReadDWord(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset) {
    unsigned int address;
    address = (unsigned int)(
            (((unsigned int)bus) << 16) |
            (((unsigned int)slot) << 11) |
            (((unsigned int)func) << 8) |
            (((unsigned int)offset)) |
            0x80000000);

    outdw(0xCF8, address);
    return indw(0xCFC);
}

unsigned int pciGetVendorAndDevice(unsigned char bus, unsigned char slot, unsigned char func)
{
    return pciConfigReadDWord(bus,slot,func,0);
}

unsigned int pciGetType(unsigned char bus, unsigned char slot, unsigned char func)
{
    return pciConfigReadDWord(bus,slot,func,0x8);
}

void clearPCIDevice(PCIDevice *dev) {
    dev->bus = 0xff;
    dev->slot = 0xff;
    dev->func = 0xff;
}

void setPCIDevice(PCIDevice *dev, unsigned char bus, unsigned char slot, unsigned char func) {
    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
}

void listPCIDevices() {
    clearPCIDevice(&ide);

    unsigned short bus, slot, function;
    unsigned int vendor, type;
    printStr("Scanning PCI bus...\n");
    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            vendor = pciGetVendorAndDevice(bus, slot, 0);
            if ((vendor & 0xffff) != 0xffff) {
                for (function = 0; function < 8; function++) {
                    vendor = pciGetVendorAndDevice(bus, slot, function);
                    if ((vendor & 0xffff) != 0xffff) {
                        type = pciGetType(bus, slot, function);
                        printStr("Found vendor ");
                        printShort(vendor & 0xffff);
                        printStr(" device ");
                        printShort((vendor >> 16));
                        printStr(" type ");
                        printShort(type >> 16);
                        printShort(type & 0xffff);
                        printStr(" at ");
                        printByte(bus);
                        printStr(":");
                        printByte(slot);
                        printStr(":");
                        printByte(function);
                        printStr("\n");

                        if ((type >> 16) == 0x0101) {
                            printStr("Found IDE at "); printByte(bus); printStr(":"); printByte(slot); printStr(":"); printByte(function); printStr("\n");
                            setPCIDevice(&ide, bus, slot, function);
                        }
                    }
                }
            }
        }
    }
}
