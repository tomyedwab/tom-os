#include "kernel.h"

TKProcessInfo *tk_process_table;

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

    printStr("Created process ");
    printByte(info->proc_id);
    printStr(" vmm: ");
    printInt(info->vmm_directory);
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
    vmmActivate(info->vmm_directory, ip);
}
