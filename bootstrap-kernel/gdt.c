#include "kernel.h"

void syscall_handler();

typedef struct {
    unsigned int d1;
    unsigned int d2;
} TKSegmentDescriptor;

typedef struct  __attribute__ ((__packed__)) {
    unsigned short limit;
    unsigned int base_address;
    unsigned short padding; // do not use
    TKSegmentDescriptor descriptors[];
} TKGlobalDescriptorTable;

TKGlobalDescriptorTable *GDT;

#define TYPE_DATA_READ_WRITE   2
#define TYPE_TSS               9
#define TYPE_CODE_EXECUTE_READ 10
#define TYPE_CODE_CONFORMING   14

#define NUM_DESCRIPTORS 6

void gdtMakeDescriptor(TKSegmentDescriptor *descriptor, unsigned int base, unsigned int limit, unsigned int privilege, unsigned int type, unsigned int nontss) {
    unsigned int granularity = 0;
    if (limit > 0xfffff) {
        granularity = 1;
        limit >>= 12;
    }
    descriptor->d1 = (
        (limit & 0xffff) |              // Limit (bits 0-15)
        ((base & 0xffffff) << 16));     // Base (bits 0-23)
    descriptor->d2 =(
        (limit & 0xf0000) |             // Limit (bits 16-19)
        ((base & 0xff000000) >> 24) |   // Base (bits 16-23)
        ((base & 0xff00000000)) |       // Base (bits 24-31)
        (granularity << 23) |           // Granularity bit
        (nontss << 22) |                // D/B bit set = 32-bit addressing
        (1 << 15) |                     // Present flag
        ((privilege & 0x3) << 13) |     // Privilege level (0-3)
        (nontss << 12) |                // Non-system (code or data) segment
        ((type & 0xf) << 8));           // Type (0-15)
}

void gdtMakeCallGate(TKSegmentDescriptor *descriptor, unsigned int segment, unsigned int fn_entry, unsigned int privilege, unsigned int params) {
    descriptor->d1 = (
        (fn_entry & 0xffff) |           // Fn address (bits 0-15)
        ((segment & 0xffff) << 16));    // Segment (bits 0-23)
    descriptor->d2 = (
        (fn_entry & 0xffff0000) |       // Fn address (bits 16-23)
        (1 << 15) |                     // Present flag
        ((privilege & 0x3) << 13) |     // Privilege level (0-3)
        (params & 0xf) |                // params (0-15)
        0x0c00);                        // Type (12)
}

void gdtInit(void) {
    int i;
    void *kernel_tss;
    // TODO: Don't need a whole page for this, use a smaller allocation.
    GDT = (TKGlobalDescriptorTable*)allocPage();
    GDT->limit = (NUM_DESCRIPTORS*8) - 1;
    GDT->base_address = (unsigned int)&GDT->descriptors[0];
    GDT->padding = 0xfefe;

    kernel_tss = (void*)&GDT[NUM_DESCRIPTORS];

    // Descriptor 0 [NULL descriptor]
    GDT->descriptors[0].d1 = 0;
    GDT->descriptors[0].d2 = 0;

    // Descriptor 0x08 [Kernel code]
    gdtMakeDescriptor(&GDT->descriptors[1], 0, 0x88000, 0, TYPE_CODE_CONFORMING, 1);
    // Descriptor 0x10 [Kernel data]
    gdtMakeDescriptor(&GDT->descriptors[2], 0x88000, 0xffffffff, 0, TYPE_DATA_READ_WRITE, 1);

    // Descriptor 0x18 [User code]
    gdtMakeDescriptor(&GDT->descriptors[3], 0x90000, 0xffffffff, 3, TYPE_CODE_EXECUTE_READ, 1);
    
    // Descriptor 0x20 [User data]
    gdtMakeDescriptor(&GDT->descriptors[4], 0x90000, 0xffffffff, 3, TYPE_DATA_READ_WRITE, 1);

    // Descriptor 0x28 [Syscall call gate]
    gdtMakeCallGate(&GDT->descriptors[5], 0x08, (unsigned int)(void*)syscall_handler, 3, 2);
    /*
    // Descriptor 0x28 [Kernel TSS]
    gdtMakeDescriptor(&GDT->descriptors[5], (unsigned int)kernel_tss, 0x67, 3, TYPE_TSS, 0);

    procInitKernelTSS(kernel_tss);
    */

    __asm__ __volatile__ (
        "lgdt (%0)"
        : : "r" (GDT)
    );
}
