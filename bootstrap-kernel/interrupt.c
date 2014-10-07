extern void syscall_handler_asm();
extern void exc_df_handler();
extern void exc_pf_handler();
extern void exc_gp_handler();
extern void int00_handler();
extern void int01_handler();
extern void int02_handler();
extern void int03_handler();
extern void int04_handler();
extern void int05_handler();
extern void int06_handler();
extern void int07_handler();
extern void int09_handler();
extern void int10_handler();
extern void int11_handler();
extern void int12_handler();
extern void int15_handler();
extern void int16_handler();
extern void int17_handler();
extern void int18_handler();
extern void int19_handler();
extern void int20_handler();

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
    setInterrupt(0, int00_handler);
    setInterrupt(1, int01_handler);
    setInterrupt(2, int02_handler);
    setInterrupt(3, int03_handler);
    setInterrupt(4, int04_handler);
    setInterrupt(5, int05_handler);
    setInterrupt(6, int06_handler);
    setInterrupt(7, int07_handler);
    setInterrupt(8, exc_df_handler);
    setInterrupt(9, int09_handler);
    setInterrupt(10, int10_handler);
    setInterrupt(11, int11_handler);
    setInterrupt(12, int12_handler);
    setInterrupt(13, exc_gp_handler);
    setInterrupt(14, exc_pf_handler);
    setInterrupt(15, int15_handler);
    setInterrupt(16, int16_handler);
    setInterrupt(17, int17_handler);
    setInterrupt(18, int18_handler);
    setInterrupt(19, int19_handler);
    setInterrupt(20, int20_handler);

    setInterrupt(0x80, syscall_handler_asm);

    __asm__ __volatile__ (
        "lidt (%0)\n"
        "sti\n"
        : : "r" (header));
}

