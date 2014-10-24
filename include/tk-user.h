// The address of the shared page used for transferring system call data
#define USER_SHARED_PAGE_VADDR    0xF000000
#define USER_STREAM_START_VADDR   0xC000000

// Stream data structures

typedef unsigned int TKMessageID;

#define ID_EMPTY          0xfefefefe
#define ID_INIT_STREAM    0x01
#define ID_PRINT_STRING   0x02

typedef struct {
    TKMessageID identifier;
    unsigned int length;
} TKMsgHeader;

typedef struct {
    void *buffer_ptr;
    TKMsgHeader *cur_ptr;
    unsigned int buffer_size;
} TKStreamPointer;

typedef struct {
    TKMsgHeader header; // ID_INIT_STREAM
    TKStreamPointer pointer;
} TKMsgInitStream;

typedef struct {
    TKMsgHeader header; // ID_PRINT_STRING
    char str[];
} TKMsgPrintString;
