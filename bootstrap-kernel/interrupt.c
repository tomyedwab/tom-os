#include "kernel.h"

unsigned long tk_system_counter;

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
extern void irq_handler_00();
extern void irq_handler_01();
extern void irq_handler_02();
extern void irq_handler_03();
extern void irq_handler_04();
extern void irq_handler_05();
extern void irq_handler_06();
extern void irq_handler_07();
extern void irq_handler_08();
extern void irq_handler_09();
extern void irq_handler_10();
extern void irq_handler_11();
extern void irq_handler_12();
extern void irq_handler_13();
extern void irq_handler_14();
extern void irq_handler_15();
extern void fail_handler();

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
    desc->type_attr = 0x8E; // Present, privilege level 0, Seg = 0, Type = xE
}

void initTimer() {
    tk_system_counter = 0;

    outb(0x43, 0);

    // Program the PIT for 1024 Hz (1,193,182 Hz / 1,024 Hz ~= 1165 = 0x486)
    outb(0x40, 0x86);
    outb(0x40, 0x04);
}

void initInterrupts() {
    InterruptTableHeader *header;
    int i;
    interrupt_table = (InterruptTableDescriptor*)INTERRUPT_TABLE_ADDR;

    initTimer();

    printStr("Interrupt table: ");
    printInt((unsigned int)interrupt_table);
    printStr("\n");
    header = (InterruptTableHeader *)&(interrupt_table[256]);

    // Clear table (64 * sizeof(unsigned int) * sizeof(ITD) bytes)
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
    //setInterrupt(8, exc_df_handler);
    setInterrupt(9, int09_handler);
    setInterrupt(10, int10_handler);
    setInterrupt(11, int11_handler);
    setInterrupt(12, int12_handler);
    //setInterrupt(13, exc_gp_handler);
    setInterrupt(14, exc_pf_handler);
    setInterrupt(15, int15_handler);
    setInterrupt(16, int16_handler);
    setInterrupt(17, int17_handler);
    setInterrupt(18, int18_handler);
    setInterrupt(19, int19_handler);
    setInterrupt(20, int20_handler);

    setInterrupt(0x20, irq_handler_00);
    setInterrupt(0x21, irq_handler_01);
    setInterrupt(0x22, irq_handler_02);
    setInterrupt(0x23, irq_handler_03);
    setInterrupt(0x24, irq_handler_04);
    setInterrupt(0x25, irq_handler_05);
    setInterrupt(0x26, irq_handler_06);
    setInterrupt(0x27, irq_handler_07);
    setInterrupt(0x28, irq_handler_08);
    setInterrupt(0x29, irq_handler_09);
    setInterrupt(0x2a, irq_handler_10);
    setInterrupt(0x2b, irq_handler_11);
    setInterrupt(0x2c, irq_handler_12);
    setInterrupt(0x2d, irq_handler_13);
    setInterrupt(0x2e, irq_handler_14);
    setInterrupt(0x2f, irq_handler_15);

    __asm__ __volatile__ (
        "lidt (%0)\n"
        "sti\n"
        : : "r" (header));
}

void excDoubleFaultHandler() {
    // TODO: Do something appropriate
    printStr("Double fault\n");
    halt();
}

void excGeneralProtectionFault(unsigned int error) {
    printStr("General protection fault\n");
    printInt(error); printStr("\n");
    halt();
}

void excPageFault(unsigned int address) {
    if (tk_cur_proc_id != 1) {
        // User process; check for growing stack downwards
        TKProcessInfo *info = &tk_process_table[tk_cur_proc_id-1];
        unsigned int stack_bottom = ((unsigned int)info->stack_vaddr) - (info->stack_pages << 12);
        if (address < stack_bottom && address >= stack_bottom - 4096) {
            // User is requesting the next stack page. Allocate it for them.
            unsigned int stack_page = (unsigned int)allocPage();
            vmmMapPage(info->vmm_directory, ((unsigned int)stack_bottom - 1) & 0xfffff000, stack_page, 1);
            info->stack_pages++;
            return;
        }
    }
    kprintf("Page fault at %X\n", address);
    //printStack();
    halt();
}

void reportInterruptHandler(unsigned int id) {
    printStr("Unhandled interrupt! ");
    printByte(id);
    printStr("\n");
}

void handleIRQ(unsigned int id) {
    //printStack();
    if (id == 0) {
        // System clock!
        tk_system_counter++;
        return;

    } else if (id == 1) {
        // Keyboard!
        unsigned char b = inb(0x60);
        if (b == 0xfa) {
            // Ack
            return;
        }
        keyboardProcessCode(b);
        return;
    }

    printStr("IRQ! ");
    printByte(id);
    printStr("\n");
}

long __attribute__ ((noinline)) getSystemCounter() {
    return tk_system_counter;
}

void sleep(unsigned int count) {
    long t = getSystemCounter(), i;
    long target = t + count;
    while ((target - t) > 0) {
        t = getSystemCounter();
        ++i; // This is necessary otherwise the compiler does something stupid
    }
}
