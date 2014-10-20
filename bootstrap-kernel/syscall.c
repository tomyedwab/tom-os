#include "kernel.h"

void syscallHandler(unsigned int func, unsigned int param1) {
    //printStack();
    if (func == 0x1) {
        // puts
        const char *str = (const char*)procGetSharedPage(tk_cur_proc_id);
        printStr((const char*)str);
        return;
    }
}
