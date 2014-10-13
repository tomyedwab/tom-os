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

    // Create a table of processes
    tk_process_table = (TKProcessInfo *)allocPage();

    // 0 = unallocated entry in the table
    for (i = 0; i < MAX_PROCESSES; i++) {
        tk_process_table[i].proc_id = 0;
    }

    // Process ID 1 = Kernel
    tk_process_table[0].proc_id = 1;
    tk_process_table[0].vmm_directory = vmmCreateDirectory();

    // Map first 1024 pages (4MB) to identity
    table = vmmGetOrCreatePageTable(tk_process_table[0].vmm_directory, 0);
    for (i = 0; i < 1024; i++) {
        vmmSetPage(table, i, i << 12);
    }

    vmmSwap(tk_process_table[0].vmm_directory);
}

void procInitKernelTSS(void *tss_ptr) {
    int i;
    tss_entry_struct *tss = (tss_entry_struct*)tss_ptr;
    for (i = 0; i < sizeof(tss_entry_struct); i++)
        ((unsigned char *)tss)[i] = 0;
    tss->ss0 = 0x10; // Kernel data segment
    tss->esp0 = 0x90000; // Kernel stack pointer
}

TKVProcID procInitUser() {
    int i;
    TKProcessInfo *info = 0;
    TKVPageTable table;

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

    // Map first 1024 pages (4MB) to identity
    table = vmmGetOrCreatePageTable(info->vmm_directory, 0);
    for (i = 0; i < 1024; i++) {
        vmmSetPage(table, i, i << 12);
    }

    // Allocate a page for the stack
    info->stack_vaddr = (void*)0x0a000fff;
    vmmMapPage(info->vmm_directory, ((unsigned int)info->stack_vaddr) & 0xfffff000, (unsigned int)allocPage());

    printStr("Created process ");
    printByte(info->proc_id);
    printStr(" vmm: ");
    printInt((unsigned int)info->vmm_directory);
    printStr("\n");
    return info->proc_id;
}

void procMapPage(TKVProcID proc_id, unsigned int src, unsigned int dest) {
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    // TODO: Verify process is valid
    vmmMapPage(info->vmm_directory, src, dest);
}

void procActivateAndJump(TKVProcID proc_id, void *ip) {
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    // TODO: Verify process is valid
    if (proc_id == 1) {
        // TODO: Jump to kernel process?
    } else {
        user_process_jump(info->vmm_directory, info->stack_vaddr, ip);
    }
}

void halt() {
    printStr("System halted.\n");
    while (1) {
    }
}
