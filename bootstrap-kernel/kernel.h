// Tom's Kernel (TK) includes

#include "tk-user.h"

// Constants
#define MAX_PROCESSES (4096/sizeof(TKProcessInfo))
#define MAX_STREAMS (4096/sizeof(TKStreamInfo))

// Hard-coded addresses
#define INTERRUPT_TABLE_ADDR      0x088000
#define GDT_ADDR                  0x089000
#define KERNEL_VMM_ADDR           0x091000
#define KERNEL_HEAP_ADDR          0x204000

// Virtual addresses
#define USER_STACK_START_VADDR    0xA000000
#define USER_HEAP_START_VADDR     0x1000000

// Process flags
#define PROC_FLAGS_TERMINATED     (1<<0)
#define PROC_FLAGS_RUNNABLE       (1<<1)

// Typedefs
typedef unsigned int *TKVPageDirectory;
typedef unsigned int *TKVPageTable;
// TODO: Rename to TKProcID - V is for 'virtual'
typedef unsigned int TKVProcID;
typedef unsigned int TKStreamID;

typedef struct TKSmallAllocatorPage TKSmallAllocatorPage;

typedef struct {
    TKVProcID proc_id;
    TKVPageDirectory vmm_directory;
    int flags;
    void *stack_vaddr;
    int stack_pages;
    void *kernel_stack_addr;
    void *shared_page_addr;
    int heap_mapped_pages; // Number of pages mapped into process virtual memory starting at USER_HEAP_START_VADDR

    // Kernel has to keep pointers to stdin/stdout so it can read from/write to them
    TKStreamPointer kernel_stdin;  // stdin write pointer
    TKStreamPointer kernel_stdout; // stdout read pointer

    // Kernel tracks all stream pointers for the process which are mapped into shared pages
    TKStreamPointer *user_stdin;   // stdin read pointer in kernel memory space
    TKStreamPointer *user_stdout;  // stdout write pointer in kernel memory space
    TKSmallAllocatorPage *stream_list; // Allocator list of all the streams (except stdin)

    long active_start; // last time we switched to this process
    long active_end; // last time we switched away from this process
    void *active_stack_addr; // PC saved when we last switched away from the process
} TKProcessInfo;

typedef struct {
    TKStreamID stream_id;        // ID used to reference the stream
    TKVProcID read_owner;        // Process that will be reading from the stream
    TKStreamPointer *read_ptr;   // Address of read stream pointer
    TKVProcID write_owner;       // Process that will be writing to the stream
    TKStreamPointer *write_ptr;  // Address of write stream pointer
    unsigned int size;           // Actual size (in bytes) of the buffer
    unsigned int num_pages;      // Number of pages allocated
    void *buffer_ptr;            // Real address of the stream buffer
} TKStreamInfo;

// kernel-entry.asm
void user_process_jump(TKVPageDirectory proc_id, void *stack_ptr, void *ip, void **stack_save_addr);
void dummy_irq();
void context_switch(void **stack_save_addr, void *new_stack, void *new_vmm_directory);

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
void procCreateStream(TKVProcID proc_read, TKVProcID proc_write, TKStreamPointer **read_stream_out, TKStreamPointer **write_stream_out);
void procMapPage(TKVProcID proc_id, unsigned int src, unsigned int dest);
void *procMapHeapPage(TKVProcID proc_id, unsigned int dest);
void procStart(TKVProcID proc_id, void *ip);
void procCheckContextSwitch();
void procExit();
void *procGetSharedPage(TKVProcID proc_id);
TKStreamPointer *procGetStdoutPointer(TKVProcID proc_id);
void halt();
void printStack();

extern TKProcessInfo *tk_process_table;

// Heap
void initHeap();
void *allocPage();
void *heapVirtAllocContiguous(int num_pages);
void *heapSmallAlloc(TKSmallAllocatorPage **allocator_pool, TKVProcID owner, int size);
void *heapSmallAllocGetNext(void *cur_ptr);

// Screen
void initScreen();
void clearScreen();
void printCharAt(int x, int y, const char c, char color);
void printChar(const char c);
void printStr(const char *str);
void printByte(unsigned char byte);
void printShort(unsigned short num);
void printInt(unsigned int num);

// Kprintf
int kprintf(const char *fmt, ...);
void initLogger();

// Interrupts
void initInterrupts();
long getSystemCounter();
void sleep(unsigned int count);

// Keyboard
void keyboardInit();
void keyboardOpenStream(TKVProcID proc, int request_num);
void keyboardProcessCode(unsigned char code);

// PCI
void pciListDevices();
int pciGetIDEConfig(int slave, unsigned int *cmdBase, unsigned int *ctrlBase, unsigned int *bmideBase, int *irqNum);

// ATA driver
int loadFromDisk(int LBA, int sectorCount, unsigned char *buffer);

// Filesystem
extern struct TFS gTFS;
void initFilesystem();

// Memcpy
void memcpy(void *dest, void *src, int bytes);

// ELF
int loadELF(const char *path, const char *file_name);

// Stream
void streamInit();
TKStreamID streamCreate(unsigned int size, TKVProcID read_owner, TKStreamPointer *read_ptr, TKVProcID write_owner, TKStreamPointer *write_ptr);
void streamSync(TKVProcID proc_id);

// Globals
extern TKVProcID tk_cur_proc_id;
extern unsigned long tk_system_counter;
