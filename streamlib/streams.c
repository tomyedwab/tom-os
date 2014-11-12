#include <streams.h>

void streamInitStreams(TKStreamPointer *read_stream, TKStreamPointer *write_stream, char *read_buffer, char *write_buffer, unsigned int size) {
    read_stream->flags = 0;
    read_stream->buffer_ptr = read_buffer;
    read_stream->buffer_size = size;
    read_stream->write_offset = 0;
    read_stream->read_offset = 0;
    read_stream->cur_offset = 0;

    write_stream->flags = TKS_FLAGS_WRITE;
    write_stream->buffer_ptr = write_buffer;
    write_stream->buffer_size = size;
    write_stream->write_offset = 0;
    write_stream->read_offset = 0;
    write_stream->cur_offset = 0;
}

int streamSyncStreams(TKStreamPointer *read_stream, TKStreamPointer *write_stream) {
    if (!read_stream || !write_stream) {
        return -1;
    }
    if ((read_stream->flags & TKS_FLAGS_WRITE) != 0) {
        // read_stream should not be writeable
        return -1;
    }
    if ((write_stream->flags & TKS_FLAGS_WRITE) == 0) {
        // write_stream must be writeable
        return -1;
    }
    
    // Detect if one of the processes messed up its stream by comparing the
    // fields they're not supposed to touch
    if (read_stream->buffer_size != write_stream->buffer_size) {
        return -1;
    }
    if (read_stream->write_offset != write_stream->write_offset) {
        return -1;
    }
    if (read_stream->read_offset != write_stream->read_offset) {
        return -1;
    }

    // Syncronize the read/write offsets
    read_stream->write_offset = write_stream->write_offset = write_stream->cur_offset;
    read_stream->read_offset = write_stream->read_offset = read_stream->cur_offset;

    return 0;
}

TKMsgHeader *streamReadMsg(TKStreamPointer *pointer) {
    TKMsgHeader *ret;

    if (pointer->flags & TKS_FLAGS_WRITE != 0) {
        // Cannot read from a write-only stream
        return 0;
    }

    if (pointer->cur_offset == pointer->write_offset) {
        // Buffer is empty!
        return 0;
    }

    ret = (TKMsgHeader*)(&pointer->buffer_ptr[pointer->cur_offset]);

    // Increment cur_offset
    pointer->cur_offset += ret->length;

    // Wrap cur_ptr around if we're past the first buffer_size bytes
    if (pointer->cur_offset > pointer->buffer_size) {
        pointer->cur_offset -= pointer->buffer_size;
    }

    return ret;
}

TKMsgHeader *streamCreateMsg(TKStreamPointer *pointer, TKMessageID identifier, unsigned int length) {
    int offset = 0;
    int cur_offset;
    TKMsgHeader *ret;

    if ((pointer->flags & TKS_FLAGS_WRITE) == 0) {
        // Cannot write to a read-only stream
        return 0;
    }

    // Length cannot be shorter than the header, duh
    if (length < sizeof(TKMsgHeader)) {
        return 0;
    }

    // Check for sufficient space in the buffer
    if (length > TKS_MAX_MESSAGE_SIZE || length >= pointer->buffer_size) {
        return 0;
    }
    // Write pointer can never pass or even reach parity to the read pointer,
    // otherwise we couldn't distinguish a completely full buffer from a
    // completely empty one. write_offset should only be equal to read_offset
    // when the reader has caught up to the writer.
    if (pointer->cur_offset < pointer->read_offset && pointer->cur_offset + length >= pointer->read_offset) {
        //printf("WRITE FAILED: %d < %d <= %d\n", pointer->cur_offset, pointer->read_offset, pointer->cur_offset + length); // donotcheckin
        return 0;
    }
    if (pointer->cur_offset >= pointer->read_offset && pointer->cur_offset + length >= pointer->read_offset + pointer->buffer_size) {
        //printf("WRITE FAILED: %d < %d <= %d\n", pointer->cur_offset, pointer->read_offset + pointer->buffer_size, pointer->cur_offset + length);
        return 0;
    }

    ret = (TKMsgHeader*)(&pointer->buffer_ptr[pointer->cur_offset]);
    ret->identifier = identifier;
    ret->length = length;

    // Increment cur_offset
    pointer->cur_offset += length;

    // Wrap cur_ptr around if we're past the first buffer_size bytes
    if (pointer->cur_offset > pointer->buffer_size) {
        pointer->cur_offset -= pointer->buffer_size;
    }
    
    return ret;
}
