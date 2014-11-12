#include "kernel.h"

void syscallHandler(unsigned int func, unsigned int param1) {
    //printStack();
    if (func == 0x1) {
        // puts
        const char *str = (const char*)procGetSharedPage(tk_cur_proc_id);
        printStr((const char*)str);
        return;
    }
    if (func == 0x2) {
        // flush_streams
        // For now, just check stdout
        procSyncAllStreams(tk_cur_proc_id);
        TKMsgHeader *msg = (TKMsgHeader*)streamReadMsg(procGetStdoutPointer(tk_cur_proc_id));
        if (msg) {
            if (msg->identifier == ID_PRINT_STRING) {
                TKMsgPrintString *pmsg = (TKMsgPrintString*)msg;
                printStr(pmsg->str);
            }
        }
    }
}
