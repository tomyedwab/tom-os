#include "tk-user.h"

TKMsgHeader *streamCreateMsg(TKStreamPointer *pointer, TKMessageID identifier, unsigned int length) {
    int offset = 0;
    int cur_offset;
    TKMsgHeader *ret;

    if (pointer->cur_ptr->identifier != ID_EMPTY) {
        // Buffer is full!
        return 0;
    }

    // TODO: Check for sufficient space in the buffer

    ret = (TKMsgHeader*)pointer->cur_ptr;
    ret->identifier = identifier;
    ret->length = length;

    // Increment cur_ptr
    pointer->cur_ptr = (TKMsgHeader*)(((unsigned int)pointer->buffer_ptr) + cur_offset + length);
    
    // Wrap cur_ptr around if we're past the first buffer_size bytes
    if (((unsigned int)pointer->cur_ptr) > ((unsigned int)pointer->buffer_ptr) + pointer->buffer_size) {
        pointer->cur_ptr = (TKMsgHeader*)(((unsigned int)pointer->cur_ptr) - pointer->buffer_size);
    }

    return ret;
}
