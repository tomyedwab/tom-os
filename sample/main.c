#include <stdlib/stdlib.h>
#include "streamlib.h"

int main() {
    int i;
    printf_old("Hello, world!\n");
    for (i = 0; i < 20; i++) {
        printf("Hello, streams! %d\n", i);
    }
    while (1) {
        TKMsgHeader *msg = streamReadMsg(&stdin_ptr);
        if (msg) {
            switch (msg->identifier) {
                case ID_KEY_CODE: {
                    TKMsgKeyCode *key_msg = (TKMsgKeyCode*)msg;
                    if (key_msg->type == TKB_KEY_TYPE_ASCII &&
                            key_msg->action == TKB_KEY_ACTION_DOWN) {
                        printf("%c", key_msg->ascii);
                    }
                    break;
                }
            }
        }
    }
    return 0x1234;
}
