#include "kernel.h"

void syscallHandler(unsigned int func, unsigned int param1) {
    /*
    printStr("Syscall! ");
    printByte(func);
    printStr(" ");
    printInt(param1);
    printStr("\n");
    */
    if (func == 0x1) {
        // puts
        printStr((const char*)param1);
        return;
    }
}
