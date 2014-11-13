#include "kernel.h"

TKProcessInfo *tk_process_table;
TKVProcID tk_cur_proc_id = 1;

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

    kprintf("Created kernel process %d vmm: %X\n", info->proc_id, info->vmm_directory);

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

    kprintf("Kernel TSS: %X\n", tss_ptr);
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
    info->stack_vaddr = (void*)USER_STACK_START_VADDR;
    stack_page = (unsigned int)allocPage();
    vmmMapPage(info->vmm_directory, ((unsigned int)info->stack_vaddr - 1) & 0xfffff000, stack_page, 1);
    info->stack_pages = 1;

    // Allocate a page for kernel/user shared memory
    info->shared_page_addr = (void *)allocPage();
    vmmMapPage(info->vmm_directory, USER_SHARED_PAGE_VADDR, (unsigned int)info->shared_page_addr, 1);

    // Allocate exactly one page each for stdin/stdout
    // TODO: Handle multiple streams per process
    // TODO: This won't work once there's more than one process
    info->user_stdin = (TKStreamPointer*)info->shared_page_addr;
    streamCreate(
        4096,
        info->proc_id, USER_STREAM_START_VADDR, info->user_stdin,
        1, KERNEL_STREAM_START_VADDR, &info->kernel_stdin);
    info->user_stdout = (TKStreamPointer*)((char*)info->shared_page_addr + sizeof(TKStreamPointer));
    streamCreate(
        4096,
        1, KERNEL_STREAM_START_VADDR + 0x2000, &info->kernel_stdout,
        info->proc_id, USER_STREAM_START_VADDR + 0x2000, info->user_stdout);

    {
        // Send stdout stream via stdin
        TKMsgInitStream *msg = (TKMsgInitStream*)streamCreateMsg(
                &info->kernel_stdin, ID_INIT_STREAM, sizeof(TKMsgInitStream));
        msg->pointer = (TKStreamPointer*)(USER_SHARED_PAGE_VADDR + sizeof(TKStreamPointer));
        streamSyncStreams(info->user_stdin, &info->kernel_stdin);
    }

    kprintf("Created process %d vmm: %X stack %X -> %X\n", info->proc_id, info->vmm_directory, info->stack_vaddr, stack_page);
    kprintf("Shared page: %X\n", info->shared_page_addr);
    return info->proc_id;
}

void procMapPage(TKVProcID proc_id, unsigned int src, unsigned int dest) {
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    // TODO: Verify process is valid
    vmmMapPage(info->vmm_directory, src, dest, 1);
}

unsigned int procActivateAndJump(TKVProcID proc_id, void *ip) {
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    unsigned int ret;
    // TODO: Verify process is valid
    tk_cur_proc_id = proc_id;
    ret = user_process_jump(info->vmm_directory, info->stack_vaddr, ip);
    tk_cur_proc_id = 1;
    return ret;
}

void *procGetSharedPage(TKVProcID proc_id) {
    // TODO: Verify process is valid
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    return info->shared_page_addr;
}

TKStreamPointer *procGetStdoutPointer(TKVProcID proc_id) {
    // TODO: Verify process is valid
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    return &info->kernel_stdout;
}

TKStreamPointer *procGetStdinPointer(TKVProcID proc_id) {
    // TODO: Verify process is valid
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    return &info->kernel_stdin;
}

void procSyncAllStreams(TKVProcID proc_id) {
    // TODO: Verify process is valid
    TKProcessInfo *info = &tk_process_table[proc_id-1];
    streamSyncStreams(info->user_stdin, &info->kernel_stdin);
    streamSyncStreams(&info->kernel_stdout, info->user_stdout);
}

void halt() {
    printStr("System halted.\n");
    while (1) {
    }
}

void printStack() {
    int i;
    unsigned int *bp, *sp, *ip;
    __asm__ __volatile__ (
        "mov %%esp, %0\n"
        "mov %%ebp, %1\n"
        : "=r" (sp), "=r" (bp)
    );
    kprintf("STACK %X BASE %X IP %X", sp, bp, &printStack);
    for (i = 0; i < 128; i++) {
        if (i % 6 == 0) {
            kprintf("\n%X: ", &bp[i]);
        }
        kprintf("%X ", bp[i]);
    }
}
