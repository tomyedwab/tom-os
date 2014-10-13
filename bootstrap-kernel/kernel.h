// Tom's Kernel (TK) includes

// Constants
#define MAX_PROCESSES (4096/sizeof(TKProcessInfo))

// Typedefs
typedef unsigned int *TKVPageDirectory;
typedef unsigned int *TKVPageTable;
typedef unsigned int TKVProcID;

typedef struct {
    TKVProcID proc_id;
    TKVPageDirectory vmm_directory;
    void *stack_vaddr;
} TKProcessInfo;

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

// Virtual memory

TKVPageDirectory vmmCreateDirectory();
TKVPageTable vmmGetOrCreatePageTable(TKVPageDirectory directory, int prefix);
void vmmSetPage(TKVPageTable table, int src, unsigned int dest);
void vmmMapPage(TKVPageDirectory directory, unsigned int src, unsigned int dest);
void vmmSwap(TKVPageDirectory directory);

// Processes

void procInitKernel();
void procInitKernelTSS(void *tss_ptr);
TKVProcID procInitUser();
void procMapPage(TKVProcID proc_id, unsigned int src, unsigned int dest);
unsigned int procActivateAndJump(TKVProcID proc_id, void *ip);
void halt();

extern TKProcessInfo *tk_process_table;

// Heap
void initHeap();
void *allocPage();

// Screen
void initScreen();
void clearScreen();
void printChar(const char c);
void printStr(const char *str);
void printByte(unsigned char byte);
void printShort(unsigned short num);
void printInt(unsigned int num);

// Interrupts
void initInterrupts();

// ATA driver
int loadFromDisk(int LBA, int sectorCount, unsigned char *buffer);

// Memcpy
void memcpy(void *dest, void *src, int bytes);
