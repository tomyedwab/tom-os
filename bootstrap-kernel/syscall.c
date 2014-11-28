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
        TKMsgHeader *msg;
        // flush_streams
        // For now, just check stdout
        procSyncAllStreams(tk_cur_proc_id);
        while (msg = (TKMsgHeader*)streamReadMsg(procGetStdoutPointer(tk_cur_proc_id))) {
            switch (msg->identifier) {
            case ID_PRINT_STRING:
                {
                    TKMsgPrintString *pmsg = (TKMsgPrintString*)msg;
                    printStr(pmsg->str);
                    break;
                }

            case ID_PRINT_CHAR_AT:
                {            
                    TKMsgPrintCharAt *pmsg = (TKMsgPrintCharAt*)msg;
                    printCharAt(pmsg->x, pmsg->y, pmsg->c, pmsg->color);
                    break;
                }

            case ID_SPAWN_PROCESS:
                {
                    TKMsgSpawnProcess *pmsg = (TKMsgSpawnProcess*)msg;
                    loadELF(pmsg->path_and_filename, &pmsg->path_and_filename[pmsg->filename_offset]);
                }
            };
        }
    }
    if (func == 0x3) {
        // Sleep
        long count = param1;
        long target = getSystemCounter() + count;
        long t, i;
        while ((target - t) > 0) {
            t = getSystemCounter();
            i++;
        }
    }
}
