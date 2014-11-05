#include <stdio.h>
#include <stdlib.h>

#include "tomfs.h"

#define RUNTEST(x) { printf("Running " #x "...\n"); if (x() != 0) { printf("Test failed!\n"); return -1; } }
#define ASSERT(x) if (!(x)) { printf("Assert failed: " #x "\n"); return -1; }
#define ASSERT_EQUALS(x, y) { int z = (x); if (z != y) { printf("Assert failed: Expected " #x " = %d to equal " #y "\n", z); return -1; } }
#define ASSERT_NOTEQUALS(x, y) { int z = (x); if (z == y) { printf("Assert failed: Expected " #x " = %d to NOT equal " #y "\n", z); return -1; } }
#define ASSERT_NOERROR(x) { int z = (x); if (z < 0) { printf("Assert failed: Expected " #x " = %d to be >= 0\n", z); return -1; } }
#define ASSERT_ERROR(x) { int z = (x); if (z >= 0) { printf("Assert failed: Expected " #x " = %d to be < 0\n", z); return -1; } }

typedef struct {
    char *base_addr;
    unsigned int num_blocks;
    int overrun; // Set if we try to write outside of the block bounds
} TestMemPtr;

int error_fn(struct TFS *fs, char *buf, unsigned int block) {
    return -1;
}

int dummy_count_fn(struct TFS *fs, char *buf, unsigned int block) {
    int *counter = (int *)fs->user_data;
    (*counter)++;
    return 0;
}

int mem_write_fn(struct TFS *fs, char *buf, unsigned int block) {
    TestMemPtr *ptr = (TestMemPtr *)fs->user_data;
    char *addr;
    int i;
    if (block >= ptr->num_blocks) {
        // Overrun! This should never happen
        ptr->overrun = 1;
        return -1;
    }
    addr = ptr->base_addr + block * TFS_BLOCK_SIZE;
    for (i = 0; i < TFS_BLOCK_SIZE; i++) {
        addr[i] = buf[i];
    }
    return 0;
}

int mem_read_fn(struct TFS *fs, char *buf, unsigned int block) {
    TestMemPtr *ptr = (TestMemPtr *)fs->user_data;
    char *addr;
    int i;
    if (block >= ptr->num_blocks) {
        // Overrun! This should never happen
        ptr->overrun = 1;
        return -1;
    }
    addr = ptr->base_addr + block * TFS_BLOCK_SIZE;
    for (i = 0; i < TFS_BLOCK_SIZE; i++) {
        buf[i] = addr[i];
    }
    return 0;
}

// Returns 0 or 1 if the block node is full, -1 on error
int validate_block_bitmap_recursive(char *bitmap_buf, int level, int idx) {
    int child1, child2;
    int expected_value;
    int bit_index = (1 << (14 - level)) + idx;
    int byte_index = bit_index >> 3;
    int bit_mask = 1 << (bit_index & 0x7);
    int bit_value = (bitmap_buf[byte_index] & bit_mask) ? 1 : 0;

    if (level == 0) {
        return bit_value;
    }

    child1 = validate_block_bitmap_recursive(bitmap_buf, level - 1, (idx << 1) + 0);
    if (child1 == -1) {
        return -1;
    }
    child2 = validate_block_bitmap_recursive(bitmap_buf, level - 1, (idx << 1) + 1);
    if (child1 == -1) {
        return -1;
    }

    expected_value = child1 & child2;

    if (expected_value != bit_value) {
        printf("Expected %d at %d:%d, found %d\n", expected_value, level, idx, bit_value);
        return -1;
    }
    return expected_value;
}

int validate_block_bitmap(char *bitmap_buf) {
    return (validate_block_bitmap_recursive(bitmap_buf, 14, 0) >= 0) ? 0 : -1;
}

int test_init_handles_errors() {
    TFS tfs;
    
    tfs.write_fn = &error_fn;
    tfsInit(&tfs);

    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), -1);

    return 0;
}

int test_init_writes_blocks() {
    TFS tfs;
    int counter = 0;
    
    tfs.read_fn = &dummy_count_fn;
    tfs.write_fn = &dummy_count_fn;
    tfs.user_data = &counter;
    tfsInit(&tfs);

    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), 0);
    ASSERT_EQUALS(counter, 2566);

    return 0;
}

int test_init_works() {
    TestMemPtr mem_ptr;
    TFS tfs;
    TFSFilesystemHeader *header;

    mem_ptr.base_addr = malloc(2560 * TFS_BLOCK_SIZE);
    mem_ptr.num_blocks = 2560;
    mem_ptr.overrun = 0;
    
    tfs.read_fn = &mem_read_fn;
    tfs.write_fn = &mem_write_fn;
    tfs.user_data = &mem_ptr;
    tfsInit(&tfs);
    
    // There is no valid filesystem yet
    ASSERT_EQUALS(tfsOpenFilesystem(&tfs), -1);

    // Create a filesystem
    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), 0);
    ASSERT_EQUALS(mem_ptr.overrun, 0);

    // Validate the header
    header = (TFSFilesystemHeader*)mem_ptr.base_addr;
    ASSERT_EQUALS(header->magic, TFS_MAGIC);
    ASSERT_EQUALS(header->current_node_id, 2);
    ASSERT_EQUALS(header->total_blocks, 2560);
    ASSERT_EQUALS(header->data_blocks, 2558);

    // Only two bits in the block bitmap should be set: the bit for the block
    // containing the bitmap (block 0) and the root directory (block 1)
    ASSERT_EQUALS(validate_block_bitmap(&mem_ptr.base_addr[TFS_BLOCK_SIZE]), 0);
    ASSERT_EQUALS(mem_ptr.base_addr[TFS_BLOCK_SIZE + 2048], 3);

    // Now we can open the filesystem successfully
    ASSERT_EQUALS(tfsOpenFilesystem(&tfs), 0);
    ASSERT_EQUALS(mem_ptr.overrun, 0);

    return 0;
}

int test_open_handles_errors() {
    TFS tfs;
    
    tfs.read_fn = &error_fn;
    tfsInit(&tfs);
    ASSERT_EQUALS(tfsOpenFilesystem(&tfs), -1);

    return 0;
}

int test_set_bitmap() {
    int i, idx;
    char block_bitmap[TFS_BLOCK_SIZE];

    for (i = 0; i < TFS_BLOCK_SIZE; i++) {
        block_bitmap[i] = 0;
    }

    // Test by incrementally setting each bit in a block bitmap until it's
    // full
    idx = 0;
    for (i = 0; i < TFS_BLOCK_GROUP_SIZE; i++) {
        // Block bitmap is valid
        ASSERT_EQUALS(validate_block_bitmap(block_bitmap), 0);
        // Blockgroup is not full
        ASSERT_EQUALS(block_bitmap[0] & 0x2, 0);

        tfsSetBitmapBit(block_bitmap, idx);

        // Skip around through the bitmap by jumping forward by a prime number
        idx = (idx + 977) % TFS_BLOCK_GROUP_SIZE;
    }
    // Block bitmap is still valid
    ASSERT_EQUALS(validate_block_bitmap(block_bitmap), 0);
    // Blockgroup is full
    ASSERT_EQUALS(block_bitmap[0] & 0x2, 0x2);

    // Incrementally clear each but in a block bitmap until it's empty
    idx = 457;
    for (i = 0; i < TFS_BLOCK_GROUP_SIZE; i++) {
        tfsClearBitmapBit(block_bitmap, idx);

        // Block bitmap is valid
        ASSERT_EQUALS(validate_block_bitmap(block_bitmap), 0);
        // Blockgroup is not full
        ASSERT_EQUALS(block_bitmap[0] & 0x2, 0);

        // Skip around through the bitmap by jumping forward by a prime number
        idx = (idx + 977) % TFS_BLOCK_GROUP_SIZE;
    }

    // Now the whole bitmap should be clear
    for (i = 0; i < TFS_BLOCK_SIZE; i++) {
        ASSERT_EQUALS(block_bitmap[i], 0);
    }

    return 0;
}

int test_allocate_blocks() {
    int i;
    TestMemPtr mem_ptr;
    TFS tfs;
    TFSFilesystemHeader *header;

    mem_ptr.base_addr = malloc(2560 * TFS_BLOCK_SIZE);
    mem_ptr.num_blocks = 2560;
    mem_ptr.overrun = 0;
    
    tfs.read_fn = &mem_read_fn;
    tfs.write_fn = &mem_write_fn;
    tfs.user_data = &mem_ptr;
    tfsInit(&tfs);
    
    // There is no valid filesystem yet
    ASSERT_EQUALS(tfsOpenFilesystem(&tfs), -1);

    // Create a filesystem
    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), 0);
    ASSERT_EQUALS(mem_ptr.overrun, 0);

    // Keep allocating random blocks until the whole FS is full
    // (The first block is already allocated by the root directory)
    for (i = 0; i < tfs.header.data_blocks - 1; i++) {
        ASSERT_NOTEQUALS(tfsAllocateBlock(&tfs, 0, 1, 0, 0), 0);
        ASSERT_EQUALS(mem_ptr.overrun, 0);
    }
    // Now all the blocks are full, we cannot allocate any more blocks
    ASSERT_EQUALS(tfsAllocateBlock(&tfs, 0, 1, 0, 0), 0);
    ASSERT_EQUALS(mem_ptr.overrun, 0);
}

int main() {
    // Low-level tests
    RUNTEST(test_set_bitmap);

    // High-level tests
    RUNTEST(test_init_handles_errors);
    RUNTEST(test_open_handles_errors);
    RUNTEST(test_init_writes_blocks);
    RUNTEST(test_init_works);
    RUNTEST(test_allocate_blocks);
    printf("All tests pass. Yay!\n");
    return 0;
}
