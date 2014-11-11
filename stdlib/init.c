#include "stdlib.h"
#include "streamlib.h"

extern void main(TKStreamPointer *, TKStreamPointer *);

// User-space application initialization
void __init() {
    TKStreamPointer stdin_ptr;
    TKStreamPointer stdout_ptr;
    TKMsgInitStream *msg;
    // Initialize stdin at a known address
    stdin_ptr.buffer_ptr = (void*)USER_STREAM_START_VADDR;
    stdin_ptr.cur_ptr = (TKMsgHeader*)stdin_ptr.buffer_ptr;
    stdin_ptr.buffer_size = 4096;

    // Read in stdout information
    msg = (TKMsgInitStream*)streamReadMsg(&stdin_ptr);
    memcpy(&stdout_ptr, &msg->pointer, sizeof(TKStreamPointer));
    streamMarkMessageRead((TKMsgHeader*)msg);

    main(&stdin_ptr, &stdout_ptr);
}
