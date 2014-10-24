// The address of the shared page used for transferring system call data
#define USER_SHARED_PAGE_VADDR    0xF000000
#define USER_STREAM_START_VADDR   0xC000000

// Keyboard constants
#define TKB_KEY_TYPE_ASCII    0x01
#define TKB_KEY_TYPE_SHIFT    0x01

#define TKB_KEY_ACTION_DOWN   0x01
#define TKB_KEY_ACTION_UP     0x02

// Stream data structures

typedef unsigned int TKMessageID;

// TODO: Namespace these
#define ID_EMPTY          0xfefefefe
#define ID_INIT_STREAM    0x01
#define ID_PRINT_STRING   0x02
#define ID_KEY_CODE       0x03

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

typedef struct {
    TKMsgHeader header; // ID_KEY_CODE
    char type;
    char action;
    char ascii;
} TKMsgKeyCode;

// Make va_list work
typedef unsigned char *va_list;
#define va_start(list, param) (list = (((va_list)&param) + sizeof(param)))
#define va_arg(list, type)    (*(type *)((list += sizeof(type)) - sizeof(type)))
#define va_end(list)
