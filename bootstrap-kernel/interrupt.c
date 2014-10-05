extern void syscall_handler_asm();
extern void gpf_handler_asm();

typedef struct __attribute__((__packed__)) {
    unsigned short limit;
    unsigned int address;
} InterruptTableHeader;


typedef struct __attribute__((__packed__))  {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char unused;
    unsigned char type_attr;
    unsigned short offset_high;
} InterruptTableDescriptor;

InterruptTableDescriptor *interrupt_table;

void setInterrupt(unsigned char interrupt, void *handler) {
    InterruptTableDescriptor *desc = &interrupt_table[interrupt];

    desc->offset_low = ((unsigned int)handler) & 0xffff;
    desc->offset_high = (unsigned int)handler >> 16;
    desc->selector = 0x08;
    desc->type_attr = 0x8E; // Present, privilege level xb, Seg = 0, Type = xE
}

void initInterrupts() {
    InterruptTableHeader *header;
    int i;
    interrupt_table = (InterruptTableDescriptor*)allocPage();
    header = (InterruptTableHeader *)&(interrupt_table[256]);

    // Clear table
    for (i = 0; i < 64 * sizeof(InterruptTableDescriptor); i++) {
        ((unsigned int *)interrupt_table)[i] = 0;
    }

    // Init header
    header->limit = 8 * 256 - 1;
    header->address = (unsigned int)interrupt_table;

    // TODO: Figure out minimal set of interrupts that need to be bound
    for (i = 0; i < 256; i++)
        setInterrupt(i, gpf_handler_asm);

    setInterrupt(0x80, syscall_handler_asm);

    __asm__ __volatile__ (
        "lidt (%0)\n"
        "sti\n"
        : : "r" (header));
}

