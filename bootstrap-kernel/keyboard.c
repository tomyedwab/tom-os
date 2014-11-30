#include "kernel.h"

char is_shift_pressed;
char state_arrow_key;
TKStreamPointer *keyboard_output_stream;

void keyboardInit() {
    is_shift_pressed = 0;
    keyboard_output_stream = 0;
}

void keyboardOpenStream(TKVProcID proc, int request_num) {
    TKStreamPointer *user_stream;
    TKMsgInitStream *init_msg;

    if (keyboard_output_stream != 0) {
        // For now, only one process can capture the keyboard input at a
        // time.
        return 0;
    }

    // Create a new stream in both the kernel and user processes
    procCreateStream(proc, 1, &user_stream, &keyboard_output_stream);

    init_msg = (TKMsgInitStream*)streamCreateMsg(procGetStdinPointer(tk_cur_proc_id), ID_INIT_STREAM, sizeof(TKMsgInitStream));
    if (init_msg) {
        // Translate the user's stream pointer to the user's address space
        init_msg->pointer = (TKStreamPointer*)heapSmallAllocGetVAddr(user_stream);
        // Pass back the request number so the caller knows which stream this is
        init_msg->request_num = request_num;
    }

    // Flush the write
    streamSync(proc);
}

void keyboardProcessCode(unsigned char code) {
    char type = 0;
    char action = 0;
    char ascii = 0;
    char dir = 0;

    if (state_arrow_key) {
        switch (code & 0x7f) {
        case 0x48: dir = TKB_ARROW_UP; break;
        case 0x50: dir = TKB_ARROW_DOWN; break;
        case 0x4b: dir = TKB_ARROW_LEFT; break;
        case 0x4d: dir = TKB_ARROW_RIGHT; break;
        }
        if (dir != 0) {
            type = TKB_KEY_TYPE_ARROW;
            if (code >= 0x80) {
                action = TKB_KEY_ACTION_UP;
            } else {
                action = TKB_KEY_ACTION_DOWN;
            }
        }
    } else if (code == 0xe0) {
        state_arrow_key = 1;
        return;
    }
    state_arrow_key = 0;

    if (code == 0x36 || code == 0x2a) {
        is_shift_pressed = 1;
        type = TKB_KEY_TYPE_SHIFT;

    } else if (code == 0xb6 || code == 0xaa) {
        is_shift_pressed = 0;
        type = TKB_KEY_TYPE_SHIFT;

    } else if (is_shift_pressed) {
        switch (code & 0x7f) {
        case 0x02: ascii = '!'; break;
        case 0x03: ascii = '@'; break;
        case 0x04: ascii = '#'; break;
        case 0x05: ascii = '$'; break;
        case 0x06: ascii = '%'; break;
        case 0x07: ascii = '^'; break;
        case 0x08: ascii = '&'; break;
        case 0x09: ascii = '*'; break;
        case 0x0a: ascii = '('; break;
        case 0x0b: ascii = ')'; break;
        case 0x0c: ascii = '_'; break;
        case 0x0d: ascii = '+'; break;
        case 0x10: ascii = 'Q'; break;
        case 0x11: ascii = 'W'; break;
        case 0x12: ascii = 'E'; break;
        case 0x13: ascii = 'R'; break;
        case 0x14: ascii = 'T'; break;
        case 0x15: ascii = 'Y'; break;
        case 0x16: ascii = 'U'; break;
        case 0x17: ascii = 'I'; break;
        case 0x18: ascii = 'O'; break;
        case 0x19: ascii = 'P'; break;
        case 0x1a: ascii = '{'; break;
        case 0x1b: ascii = '}'; break;
        case 0x1c: ascii = '\n'; break;
        case 0x1e: ascii = 'A'; break;
        case 0x1f: ascii = 'S'; break;
        case 0x20: ascii = 'D'; break;
        case 0x21: ascii = 'F'; break;
        case 0x22: ascii = 'G'; break;
        case 0x23: ascii = 'H'; break;
        case 0x24: ascii = 'J'; break;
        case 0x25: ascii = 'K'; break;
        case 0x26: ascii = 'L'; break;
        case 0x27: ascii = ':'; break;
        case 0x28: ascii = '"'; break;
        case 0x29: ascii = '~'; break;
        case 0x2b: ascii = '|'; break;
        case 0x2c: ascii = 'Z'; break;
        case 0x2d: ascii = 'X'; break;
        case 0x2e: ascii = 'C'; break;
        case 0x2f: ascii = 'V'; break;
        case 0x30: ascii = 'B'; break;
        case 0x31: ascii = 'N'; break;
        case 0x32: ascii = 'M'; break;
        case 0x33: ascii = '<'; break;
        case 0x34: ascii = '>'; break;
        case 0x35: ascii = '?'; break;
        case 0x39: ascii = ' '; break;
        };

    } else {
        switch (code & 0x7f) {
        case 0x02: ascii = '1'; break;
        case 0x03: ascii = '2'; break;
        case 0x04: ascii = '3'; break;
        case 0x05: ascii = '4'; break;
        case 0x06: ascii = '5'; break;
        case 0x07: ascii = '6'; break;
        case 0x08: ascii = '7'; break;
        case 0x09: ascii = '8'; break;
        case 0x0a: ascii = '9'; break;
        case 0x0b: ascii = '0'; break;
        case 0x0c: ascii = '-'; break;
        case 0x0d: ascii = '='; break;
        case 0x10: ascii = 'q'; break;
        case 0x11: ascii = 'w'; break;
        case 0x12: ascii = 'e'; break;
        case 0x13: ascii = 'r'; break;
        case 0x14: ascii = 't'; break;
        case 0x15: ascii = 'y'; break;
        case 0x16: ascii = 'u'; break;
        case 0x17: ascii = 'i'; break;
        case 0x18: ascii = 'o'; break;
        case 0x19: ascii = 'p'; break;
        case 0x1a: ascii = '['; break;
        case 0x1b: ascii = ']'; break;
        case 0x1c: ascii = '\n'; break;
        case 0x1e: ascii = 'a'; break;
        case 0x1f: ascii = 's'; break;
        case 0x20: ascii = 'd'; break;
        case 0x21: ascii = 'f'; break;
        case 0x22: ascii = 'g'; break;
        case 0x23: ascii = 'h'; break;
        case 0x24: ascii = 'j'; break;
        case 0x25: ascii = 'k'; break;
        case 0x26: ascii = 'l'; break;
        case 0x27: ascii = ';'; break;
        case 0x28: ascii = '\''; break;
        case 0x29: ascii = '`'; break;
        case 0x2b: ascii = '\\'; break;
        case 0x2c: ascii = 'z'; break;
        case 0x2d: ascii = 'x'; break;
        case 0x2e: ascii = 'c'; break;
        case 0x2f: ascii = 'v'; break;
        case 0x30: ascii = 'b'; break;
        case 0x31: ascii = 'n'; break;
        case 0x32: ascii = 'm'; break;
        case 0x33: ascii = ','; break;
        case 0x34: ascii = '.'; break;
        case 0x35: ascii = '/'; break;
        case 0x39: ascii = ' '; break;
        };

    }

    if (ascii != 0) {
        type = TKB_KEY_TYPE_ASCII;
        if (code >= 0x80) {
            action = TKB_KEY_ACTION_UP;
        } else {
            action = TKB_KEY_ACTION_DOWN;
        }
    }

    if (type != 0 && keyboard_output_stream) {
        TKMsgKeyCode *msg = (TKMsgKeyCode*)streamCreateMsg(keyboard_output_stream, ID_KEY_CODE, sizeof(TKMsgKeyCode));
        if (msg) {
            msg->type = type;
            msg->action = action;
            msg->ascii = ascii;
            msg->dir = dir;
        }
        streamSync(1);
    } else {
        // TODO: Handle other character codes
        /*
        printStr("Key: ");
        printByte(code);
        printStr("\n");
        */
    }
}
