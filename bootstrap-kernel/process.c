#include "kernel.h"

TKProcessInfo *tk_process_table;

// A struct describing a Task State Segment.
typedef struct  __attribute__ ((__packed__)) {
   unsigned int prev_tss;   // The previous TSS - if we used hardware task switching this would form a linked list.
   unsigned int esp0;       // The stack pointer to load when we change to kernel mode.
   unsigned int ss0;        // The stack segment to load when we change to kernel mode.
   unsigned int esp1;       // everything below here is unusued now.. 
   unsigned int ss1;
   unsigned int esp2;
   unsigned int ss2;
   unsigned int cr3;
   unsigned int eip;
   unsigned int eflags;
   unsigned int eax;
   unsigned int ecx;
   unsigned int edx;
   unsigned int ebx;
   unsigned int esp;
   unsigned int ebp;
   unsigned int esi;
   unsigned int edi;
   unsigned int es;         
   unsigned int cs;        
   unsigned int ss;        
   unsigned int ds;        
   unsigned int fs;       
   unsigned int gs;         
   unsigned int ldt;      
   unsigned short trap;
   unsigned short iomap_base;
} tss_entry_struct;

void procInitKernel() {
    int i;
    TKVPageTable table;
    TKProcessInfo *info;

    // Create a table of processes
    tk_process_table = (TKProcessInfo *)allocPage();

    // 0 = unallocated entry in the table
    for (i = 0; i < MAX_PROCESSES; i++) {
        tk_process_table[i].proc_id = 0;
    }

    info = &tk_process_table[0];

    // Process ID 1 = Kernel
    info->proc_id = 1;
    info->vmm_directory = (TKVPageDirectory)KERNEL_VMM_ADDR;
    for (i = 0; i < 1024; i++) {
        info->vmm_directory[i] = 0;
    }

    // Map first 1024 pages (4MB) to identity
    table = vmmGetOrCreatePageTable(info->vmm_directory, 0, 0);
    for (i = 0; i < 1024; i++) {
        vmmSetPage(table, i, i << 12, 0);
    }

    printStr("Created kernel process "); printByte(info->proc_id);
    printStr(" vmm: ");                  printInt((unsigned int)info->vmm_directory);
    printStr("\n");

    vmmSwap(info->vmm_directory);
}

void procInitKernelTSS(void *tss_ptr) {
    int i;
    unsigned int syscall_stack = (unsigned int)allocPage();
    tss_entry_struct *tss = (tss_entry_struct*)tss_ptr;
    for (i = 0; i < sizeof(tss_entry_struct); i++)
        ((unsigned char *)tss)[i] = 0;
    tss->ss0 = 0x10; // Kernel data segment
    tss->esp0 = KERNEL_SYSCALL_STACK_ADDR + 0x1000; // Stack pointer for entry through a call gate
    tss->iomap_base = sizeof(tss_entry_struct);
}

TKVProcID procInitUser() {
    int i;
    TKProcessInfo *info = 0;
    TKVPageTable table;
    unsigned int stack_page;

    // First find an unused slot in the process table
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (tk_process_table[i].proc_id == 0) {
            info = &tk_process_table[i];
            info->proc_id = i + 1;
            break;
        }
    }
    if (info == 0) {
        printStr("Could not allocate process!");
        halt();
    }

    info->vmm_directory = vmmCreateDirectory();

    // Identity map kernel code so we can jump into the interrupt handlers
    for (i = 0x8000; i < 0x90000; i += 0x1000) {
        vmmMapPage(info->vmm_directory, i, i, 1);
    }
    // Identity map syscall stack space
    for (i = 0x0; i < 0x4000; i += 0x1000) {
        vmmMapPage(info->vmm_directory, i + KERNEL_SYSCALL_STACK_ADDR, i + KERNEL_SYSCALL_STACK_ADDR, 1);
    }

    // Allocate a page for the stack
    info->stack_vaddr = (void*)0x0a000fff;
    stack_page = (unsigned int)allocPage();
    vmmMapPage(info->vmm_directory, ((unsigned int)info->stack_vaddr) & 0xfffff000, stack_page, 1);

    printStr("Created process "); printByte(info->proc_id);
    printStr(" vmm: ");           printInt((unsigned int)info->vmm_directory);
    printStr(" stack: ");         printInt((unsigned int)info->stack_vaddr);
    printStr(" -> ");             printInt((unsigned int)stack_page);
    printStr("\n");
    return info->proc_id;
}

void procMapPage(TKVProcID proc_id, unsigned int src, unsigned int dest) {
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    // TODO: Verify process is valid
    vmmMapPage(info->vmm_directory, src, dest, 1);
}

unsigned int procActivateAndJump(TKVProcID proc_id, void *ip) {
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    // TODO: Verify process is valid
    return user_process_jump(info->vmm_directory, info->stack_vaddr, ip);
}

void halt() {
    printStr("System halted.\n");
    while (1) {
    }
}
