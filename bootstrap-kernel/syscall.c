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

void reportInterruptHandler() {
    // TODO: Figure out which interrupt happened and why
    printStr("Interrupt!\n");
}
