#include <streams.h>

// The address of the shared page used for transferring system call data
#define USER_SHARED_PAGE_VADDR    0xF000000
#define USER_STREAM_START_VADDR   0xC000000

// Keyboard constants
#define TKB_KEY_TYPE_ASCII    0x01
#define TKB_KEY_TYPE_SHIFT    0x01
#define TKB_KEY_TYPE_ARROW    0x02

#define TKB_KEY_ACTION_DOWN   0x01
#define TKB_KEY_ACTION_UP     0x02

#define TKB_ARROW_UP          0x01
#define TKB_ARROW_DOWN        0x02
#define TKB_ARROW_LEFT        0x03
#define TKB_ARROW_RIGHT       0x04

// Screen constants
#define TK_SCREEN_ROWS 25
#define TK_SCREEN_COLS 80

// Stream data structures

// TODO: Namespace these
#define ID_INIT_STREAM    0x01
#define ID_PRINT_STRING   0x10
#define ID_PRINT_CHAR_AT  0x11
#define ID_KEY_CODE       0x20
#define ID_SPAWN_PROCESS  0x30
#define ID_OPEN_STREAM    0x40

typedef struct {
    TKMsgHeader header; // ID_INIT_STREAM
    TKStreamPointer *pointer;
    int request_num; // A request number passed in when opening the stream
} TKMsgInitStream;

typedef struct {
    TKMsgHeader header; // ID_PRINT_STRING
    char str[];
} TKMsgPrintString;

typedef struct {
    TKMsgHeader header; // ID_PRINT_CHAR_AT
    char x;
    char y;
    char c;
    char color;
} TKMsgPrintCharAt;

typedef struct {
    TKMsgHeader header; // ID_KEY_CODE
    char type;
    char action;
    char ascii; // for TKB_KEY_TYPE_ASCII
    char dir; // for TKB_KEY_TYPE_ARROW
} TKMsgKeyCode;

typedef struct {
    TKMsgHeader header; // ID_SPAWN_PROCESS
    int filename_offset; // Offset of the filename in path_and_filename
    // Null terminated path + null terminated filename
    char path_and_filename[];
} TKMsgSpawnProcess;

typedef struct {
    TKMsgHeader header; // ID_OPEN_STREAM
    int request_num; // A request number that will be associated with the stream
    char uri[];
} TKMsgOpenStream;

// Make va_list work
typedef unsigned char *va_list;
#define va_start(list, param) (list = (((va_list)&param) + sizeof(param)))
#define va_arg(list, type)    (*(type *)((list += sizeof(type)) - sizeof(type)))
#define va_end(list)
