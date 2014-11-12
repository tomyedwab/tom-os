/************************************************
 * Constants                                    *
 ************************************************/

// We can write if and only if the WRITE flag is set
// We can read if and only if the WRITE flag is NOT set
#define TKS_FLAGS_WRITE       (1<<0)

#define TKS_ID_EMPTY          0xfefefefe

#ifndef TKS_MAX_MESSAGE_SIZE
// The maximum size of a single message (buffers may be much bigger)
#define TKS_MAX_MESSAGE_SIZE  4096
#endif

/************************************************
 * Structs                                      *
 ************************************************/

/**
 * A stream pointer which gives a process access to one endpoint of a stream.
 *
 * A stream pointer can only be either read-only or write-only (depending on
 * the TKS_FLAGS_WRITE flag) and thus cur_offset is either the read head offset
 * or the write head offset. Changes in the head offset aren't "public" until
 * the system sets write_offset or read_offset on both pointers simultaneously,
 * which obviously requires a system call. This allows writes or reads to be
 * batched without making multiple system calls.
 *
 * The stream is written to/read from a single ring buffer except that all
 * messages are contiguous in memory (a message is never broken in two parts at
 * the buffer edges). This can be implemented one of two ways:
 *   1. buffer_ptr points to allocated memory that is actually
 *      buffer_size + TKS_MAX_MESSAGE_SIZE bytes long
 *      (or buffer_size * 2 if buffer_size < TKS_MAX_MESSAGE_SIZE)
 *   2. buffer_ptr is mapped to virtual memory pages such that the page[s]
 *      immediately following the allocated buffer map to the beginning of
 *      the buffer, so writes to buffer_ptr[buffer_size+N] actually write to
 *      buffer_ptr[N] for min(buffer_size, TKS_MAX_MESSAGE_SIZE) bytes.
 *
 * Thus a 16-byte message starting at buffer_ptr[buffer_size-2] would write
 * until buffer_ptr[buffer_size+13] and then the following message would be
 * written starting at buffer_ptr[14].
 */
typedef struct {
    // Read/write and other flags (see TKS_FLAGS_*)
    unsigned int flags;
    // The base pointer of the stream
    char *buffer_ptr;
    // The size (in bytes) of the stream
    unsigned int buffer_size;
    // The current offset of the write head
    unsigned int write_offset;
    // The current offset of the read head
    unsigned int read_offset;
    // The offset of the last byte written to or read from
    unsigned int cur_offset;
} TKStreamPointer;

typedef unsigned int TKMessageID;

// This is the header for all message traveling over a stream
typedef struct {
    TKMessageID identifier;
    unsigned int length;
} TKMsgHeader;

/************************************************
 * Methods                                      *
 ************************************************/

// Initialize a pair of stream endpoints with shared buffer.
// (read_buffer and write_buffer refer to the same buffer but possibly in
// different memory spaces)
void streamInitStreams(TKStreamPointer *read_stream, TKStreamPointer *write_stream, char *read_buffer, char *write_buffer, unsigned int size);

// Sync the write_offset and read_offset so they are identical in both streams,
// read_offset is read_stream's cur_offset, and write_offset is write_stream's
// cur_offset. This needs to happen in a system call because only the kernel
// has access to both process's stream pointers.
// Returns 0 on success and -1 if one of the clients corrupted their stream
// (read/write offsets don't match)
int streamSyncStreams(TKStreamPointer *read_stream, TKStreamPointer *write_stream);

// Read a message from the stream. Returns a pointer to the message in memory
// or NULL on error. This can be called at any time from a user process.
TKMsgHeader *streamReadMsg(TKStreamPointer *pointer);

// Allocate space in the stream for a new message. The identifier and the
// length are used to initialize the message header, and the caller is left
// to fill in the body of the message. Returns a NULL pointer on error.
// This can be called at any time from a user process.
TKMsgHeader *streamCreateMsg(TKStreamPointer *pointer, TKMessageID identifier, unsigned int length);
