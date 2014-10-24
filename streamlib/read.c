#include "tk-user.h"

TKMsgHeader *streamReadMsg(TKStreamPointer *pointer) {
    TKMsgHeader *ret;

    if (pointer->cur_ptr->identifier == ID_EMPTY) {
        // Buffer is empty!
        return 0;
    }

    ret = pointer->cur_ptr;

    // Increment cur_ptr
    pointer->cur_ptr = (TKMsgHeader*)(((unsigned int)pointer->cur_ptr) + ret->length);
    
    // Wrap cur_ptr around if we're past the first buffer_size bytes
    if (((unsigned int)pointer->cur_ptr) > ((unsigned int)pointer->buffer_ptr) + pointer->buffer_size) {
        pointer->cur_ptr = (TKMsgHeader*)(((unsigned int)pointer->cur_ptr) - pointer->buffer_size);
    }

    return ret;
}

void streamMarkMessageRead(TKMsgHeader *msg) {
    msg->identifier = ID_EMPTY;
}
