#include <stdlib/stdlib.h>
#include "streamlib.h"

void main(TKStreamPointer *stdin, TKStreamPointer *stdout) {
    int i;
    for (i = 0; i < 20; i++) {
        fprintf(stdout, "Hello, streams! %d\n", i);
    }
    while (1) {
        TKMsgHeader *msg = streamReadMsg(stdin);
        if (msg) {
            switch (msg->identifier) {
                case ID_KEY_CODE: {
                    TKMsgKeyCode *key_msg = (TKMsgKeyCode*)msg;
                    if (key_msg->type == TKB_KEY_TYPE_ASCII &&
                            key_msg->action == TKB_KEY_ACTION_DOWN) {
                        fprintf(stdout, "%c", key_msg->ascii);
                    }
                    break;
                }
            }
        }
    }
}
