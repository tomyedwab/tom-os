// Tom's Kernel (TK) includes

#include "tk-user.h"

// Constants
#define MAX_PROCESSES (4096/sizeof(TKProcessInfo))
#define MAX_STREAMS (4096/sizeof(TKStreamInfo))

// Hard-coded addresses
#define INTERRUPT_TABLE_ADDR      0x088000
#define GDT_ADDR                  0x089000
#define KERNEL_VMM_ADDR           0x200000
#define KERNEL_SYSCALL_STACK_ADDR 0x201000
#define KERNEL_HEAP_ADDR          0x204000
#define KERNEL_STREAM_START_VADDR 0xC000000

// Typedefs
typedef unsigned int *TKVPageDirectory;
typedef unsigned int *TKVPageTable;
// TODO: Rename to TKProcID - V is for 'virtual'
typedef unsigned int TKVProcID;
typedef unsigned int TKStreamID;

typedef struct {
    TKVProcID proc_id;
    TKVPageDirectory vmm_directory;
    void *stack_vaddr;
    void *shared_page_addr;
    TKStreamID stdin;
    TKStreamPointer stdin_ptr;
    TKStreamID stdout;
    TKStreamPointer stdout_ptr;
} TKProcessInfo;

typedef struct {
    TKStreamID stream_id;    // ID used to reference the stream
    TKVProcID read_owner;    // Process that will be reading from the stream
    TKVProcID write_owner;   // Process that will be writing to the stream
    unsigned int size;       // Actual size (in bytes) of the buffer
    unsigned int num_pages;  // Number of pages allocated
    void *buffer_ptr;        // Real address of the stream buffer
    void *read_vaddr;        // Address of the stream in the reader's address space
    void *write_vaddr;       // Address of the stream in the writer's address space
} TKStreamInfo;

// kernel-entry.asm
unsigned int user_process_jump(TKVPageDirectory proc_id, void *stack_ptr, void *ip);

// Global descriptor table
void gdtInit(void);

// Ports
unsigned char inb(unsigned short port);
void outb(unsigned short port, unsigned char data);
unsigned short inw(unsigned short port);
void outw(unsigned short port, unsigned short data);
unsigned int indw(unsigned short port);
void outdw(unsigned short port, unsigned int data);

// PIC

void initPIC();

// Virtual memory

TKVPageDirectory vmmCreateDirectory();
TKVPageTable vmmGetOrCreatePageTable(TKVPageDirectory directory, unsigned int prefix, int user);
void vmmSetPage(TKVPageTable table, int src, unsigned int dest, int user);
void vmmMapPage(TKVPageDirectory directory, unsigned int src, unsigned int dest, int user);
void vmmSwap(TKVPageDirectory directory);

// Processes

void procInitKernel();
void procInitKernelTSS(void *tss_ptr);
TKVProcID procInitUser();
void procMapPage(TKVProcID proc_id, unsigned int src, unsigned int dest);
unsigned int procActivateAndJump(TKVProcID proc_id, void *ip);
void *procGetSharedPage(TKVProcID proc_id);
TKStreamPointer *procGetStdoutPointer(TKVProcID proc_id);
void halt();
void printStack();

extern TKProcessInfo *tk_process_table;

// Heap
void initHeap();
void *allocPage();
void *heapAllocContiguous(int num_pages);

// Screen
void initScreen();
void clearScreen();
void printChar(const char c);
void printStr(const char *str);
void printByte(unsigned char byte);
void printShort(unsigned short num);
void printInt(unsigned int num);

// Kprintf
int kprintf(const char *fmt, ...);

// Interrupts
void initInterrupts();

// Keyboard
void keyboardInit();
void keyboardProcessCode(unsigned char code);

// ATA driver
int loadFromDisk(int LBA, int sectorCount, unsigned char *buffer);

// Memcpy
void memcpy(void *dest, void *src, int bytes);

// Stream
void streamInit();
TKStreamID streamCreate(unsigned int size);
void streamMapReader(TKStreamID id, TKVProcID owner, unsigned int v_addr);
void streamMapWriter(TKStreamID id, TKVProcID owner, unsigned int v_addr);

// Globals
extern TKVProcID tk_cur_proc_id;
extern unsigned long tk_system_counter;
