#include "kernel.h"

void printStack() {
    int i;
    unsigned int *bp, *sp, *ip;
    __asm__ __volatile__ (
        "mov %%esp, %0\n"
        "mov %%ebp, %1\n"
        : "=r" (sp), "=r" (bp)
    );
    printStr("STACK ");
    printInt((unsigned int)sp);
    printStr(" BASE ");
    printInt((unsigned int)bp);
    printStr(" IP ");
    printInt((unsigned int)&printStack);
    for (i = 0; i < 128; i++) {
        if (i % 6 == 0) {
            printStr("\n");
            printInt((unsigned int)&bp[i]);
            printStr(": ");
        }
        printInt(bp[i]);
        printStr(" ");
    }
}

void syscallHandler(unsigned int func, unsigned int param1) {
    //printStack();
    if (func == 0x1) {
        // puts
        const char *str = (const char*)procGetSharedPage(tk_cur_proc_id);
        printStr((const char*)str);
        return;
    }
}
