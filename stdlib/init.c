#include "stdlib.h"
#include "streamlib.h"

extern void main(TKStreamPointer *, TKStreamPointer *);

// User-space application initialization
void __init() {
    TKStreamPointer *stdin_ptr = (TKStreamPointer*)USER_SHARED_PAGE_VADDR;
    TKStreamPointer *stdout_ptr;
    TKMsgInitStream *msg;

    // Read in stdout information
    msg = (TKMsgInitStream*)streamReadMsg(stdin_ptr);
    stdout_ptr = msg->pointer;

    main(stdin_ptr, stdout_ptr);
}
