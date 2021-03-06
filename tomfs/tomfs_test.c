#include <stdio.h>
#include <stdlib.h>

#include "tomfs.h"

#define RUNTEST(x) { printf("Running " #x "...\n"); if (x() != 0) { printf("Test failed!\n"); return -1; } }
#define ASSERT(x) if (!(x)) { printf("Assert failed: " #x " on line %d\n", __LINE__); return -1; }
#define ASSERT_EQUALS(x, y) { int z = (x); if (z != y) { printf("Assert failed: Expected " #x " = %d to equal " #y " on line %d\n", z, __LINE__); return -1; } }
#define ASSERT_NOTEQUALS(x, y) { int z = (x); if (z == y) { printf("Assert failed: Expected " #x " = %d to NOT equal " #y " on line %d\n", z, __LINE__); return -1; } }
#define ASSERT_NOERROR(x) { int z = (x); if (z < 0) { printf("Assert failed: Expected " #x " = %d to be >= 0 on line %d\n", z, __LINE__); return -1; } }
#define ASSERT_ERROR(x) { int z = (x); if (z >= 0) { printf("Assert failed: Expected " #x " = %d to be < 0 on line %d\n", z, __LINE__); return -1; } }

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
    tfsInit(&tfs, NULL, 0);

    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), -1);

    return 0;
}

int test_init_writes_blocks() {
    TFS tfs;
    int counter = 0;
    
    tfs.read_fn = &dummy_count_fn;
    tfs.write_fn = &dummy_count_fn;
    tfs.user_data = &counter;
    tfsInit(&tfs, NULL, 0);

    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), 0);
    ASSERT_EQUALS(counter, 2576);

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
    tfsInit(&tfs, NULL, 0);
    
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
    tfsInit(&tfs, NULL, 0);
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

    mem_ptr.base_addr = malloc(2560 * TFS_BLOCK_SIZE);
    mem_ptr.num_blocks = 2560;
    mem_ptr.overrun = 0;
    
    tfs.read_fn = &mem_read_fn;
    tfs.write_fn = &mem_write_fn;
    tfs.user_data = &mem_ptr;
    tfsInit(&tfs, NULL, 0);
    
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

    return 0;
}

int test_write_files() {
    int i;
    TestMemPtr mem_ptr;
    TFS tfs;
    FileHandle *handle;
    char buf[TFS_BLOCK_DATA_SIZE*5];

    mem_ptr.base_addr = malloc(2560 * TFS_BLOCK_SIZE);
    mem_ptr.num_blocks = 2560;
    mem_ptr.overrun = 0;
    
    tfs.read_fn = &mem_read_fn;
    tfs.write_fn = &mem_write_fn;
    tfs.user_data = &mem_ptr;
    tfsInit(&tfs, NULL, 0);
    
    // There is no valid filesystem yet
    ASSERT_EQUALS(tfsOpenFilesystem(&tfs), -1);

    // Create a filesystem
    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), 0);
    ASSERT_EQUALS(mem_ptr.overrun, 0);

    // Write less than one block worth of data into a file
    ASSERT_NOTEQUALS(handle = tfsCreateFile(&tfs, "", 0644, "test_single_block"), 0);
    ASSERT_NOERROR(tfsWriteFile(&tfs, handle, buf, 64, 0));

    // Write more than one block worth of data into a file, less than a
    // block at a time
    ASSERT_NOTEQUALS(handle = tfsCreateFile(&tfs, "", 0644, "test_small_writes"), 0);
    for (i = 0; i < 5; i++) {
        ASSERT_NOERROR(tfsWriteFile(&tfs, handle, buf, TFS_BLOCK_DATA_SIZE - 100, i * (TFS_BLOCK_DATA_SIZE - 100)));
    }

    // Write more than one block worth of data into a file, exactly a
    // block at a time
    ASSERT_NOTEQUALS(handle = tfsCreateFile(&tfs, "", 0644, "test_med_writes"), 0);
    for (i = 0; i < 5; i++) {
        ASSERT_NOERROR(tfsWriteFile(&tfs, handle, buf, TFS_BLOCK_DATA_SIZE, i * TFS_BLOCK_DATA_SIZE));
    }

    // Write more than one block worth of data into a file, more than a
    // block at a time
    ASSERT_NOTEQUALS(handle = tfsCreateFile(&tfs, "", 0644, "test_large_writes"), 0);
    for (i = 0; i < 5; i++) {
        ASSERT_NOERROR(tfsWriteFile(&tfs, handle, buf, TFS_BLOCK_DATA_SIZE + 100, i * (TFS_BLOCK_DATA_SIZE + 100)));
    }

    // Write 5 blocks worth' of data at one time
    ASSERT_NOTEQUALS(handle = tfsCreateFile(&tfs, "", 0644, "test_huge_writes"), 0);
    ASSERT_NOERROR(tfsWriteFile(&tfs, handle, buf, TFS_BLOCK_DATA_SIZE * 5, 0));

    // Append to an existing file
    ASSERT_NOTEQUALS(handle = tfsOpenFile(&tfs, "", "test_single_block"), 0);
    ASSERT_NOERROR(tfsWriteFile(&tfs, handle, buf, 64, 64));

    return 0;
}

int test_read_files() {
    int i, num;
    TestMemPtr mem_ptr;
    TFS tfs;
    FileHandle *handle;
    char buf[TFS_BLOCK_DATA_SIZE*5];

    mem_ptr.base_addr = malloc(2560 * TFS_BLOCK_SIZE);
    mem_ptr.num_blocks = 2560;
    mem_ptr.overrun = 0;
    
    tfs.read_fn = &mem_read_fn;
    tfs.write_fn = &mem_write_fn;
    tfs.user_data = &mem_ptr;
    tfsInit(&tfs, NULL, 0);
    
    // There is no valid filesystem yet
    ASSERT_EQUALS(tfsOpenFilesystem(&tfs), -1);

    // Create a filesystem
    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), 0);
    ASSERT_EQUALS(mem_ptr.overrun, 0);
    
    for (i = 0; i < TFS_BLOCK_DATA_SIZE * 5 / 4; i++) {
        ((unsigned int *)buf)[i] = 0xffff - i;
    }

    // Write 5 blocks worth' of data at one time
    ASSERT_NOTEQUALS(handle = tfsCreateFile(&tfs, "", 0644, "test_huge_writes"), 0);
    ASSERT_NOERROR(tfsWriteFile(&tfs, handle, buf, TFS_BLOCK_DATA_SIZE * 5, 0));

    // Read the ints back individually and verify they are correct
    for (i = 0; i < TFS_BLOCK_DATA_SIZE * 5 / 4; i++) {
        ASSERT_EQUALS(tfsReadFile(&tfs, handle, &num, 4, i*4), 4);
        ASSERT_EQUALS(num, 0xffff - i);
    }

    // Read the entire buffer back
    ASSERT_EQUALS(tfsReadFile(&tfs, handle, buf, TFS_BLOCK_DATA_SIZE * 5, 0), TFS_BLOCK_DATA_SIZE * 5);
    for (i = 0; i < TFS_BLOCK_DATA_SIZE * 5 / 4; i++) {
        ASSERT_EQUALS(((unsigned int *)buf)[i], 0xffff - i);
    }

    return 0;
}

int test_directories() {
    TestMemPtr mem_ptr;
    TFS tfs;
    FileHandle *dir, *file;
    int idx = 0;
    int count;
    int mode;
    int block_idx;
    int size;
    char filename[256];

    mem_ptr.base_addr = malloc(2560 * TFS_BLOCK_SIZE);
    mem_ptr.num_blocks = 2560;
    mem_ptr.overrun = 0;
    
    tfs.read_fn = &mem_read_fn;
    tfs.write_fn = &mem_write_fn;
    tfs.user_data = &mem_ptr;
    tfsInit(&tfs, NULL, 0);
    
    // Create a filesystem
    ASSERT_EQUALS(tfsInitFilesystem(&tfs, 2560), 0);
    ASSERT_EQUALS(mem_ptr.overrun, 0);

    ASSERT_EQUALS(tfsGetOpenHandleCount(), 0);

    // List files in the root directory
    ASSERT_NOTEQUALS(dir = tfsOpenPath(&tfs, "/"), NULL);
    count = 0;
    while (tfsReadNextEntry(&tfs, dir, &idx, &mode, &block_idx, &size, filename, 256) == 0) {
        ASSERT(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0);
        count++;
    }
    ASSERT_EQUALS(count, 2);

    // Can also look up files by name
    ASSERT_EQUALS(tfsFindEntry(&tfs, dir, ".", &mode, &block_idx, &size), 0);
    ASSERT_EQUALS(tfsFindEntry(&tfs, dir, "..", &mode, &block_idx, &size), 0);

    tfsCloseHandle(dir);
    ASSERT_EQUALS(tfsGetOpenHandleCount(), 0);
    
    // Create a file in the root directory
    ASSERT_NOTEQUALS((file = tfsCreateFile(&tfs, "/", 0644, "root_1")), 0);

    ASSERT_NOTEQUALS(dir = tfsOpenPath(&tfs, "/"), NULL);
    idx = 0;
    count = 0;
    while (tfsReadNextEntry(&tfs, dir, &idx, &mode, &block_idx, &size, filename, 256) == 0) {
        ASSERT(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0 || strcmp(filename, "root_1") == 0);
        count++;
    }
    ASSERT_EQUALS(count, 3);

    tfsCloseHandle(dir);
    tfsCloseHandle(file);
    ASSERT_EQUALS(tfsGetOpenHandleCount(), 0);

    // Create a new subdirectory
    ASSERT_NOTEQUALS(dir = tfsCreateDirectory(&tfs, "/", "testdir"), NULL);
    idx = 0;
    count = 0;
    while (tfsReadNextEntry(&tfs, dir, &idx, &mode, &block_idx, &size, filename, 256) == 0) {
        ASSERT(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0);
        count++;
    }
    ASSERT_EQUALS(count, 2);

    tfsCloseHandle(dir);
    ASSERT_EQUALS(tfsGetOpenHandleCount(), 0);

    ASSERT_NOTEQUALS(dir = tfsOpenPath(&tfs, "/"), NULL);
    idx = 0;
    count = 0;
    while (tfsReadNextEntry(&tfs, dir, &idx, &mode, &block_idx, &size, filename, 256) == 0) {
        ASSERT(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0 || strcmp(filename, "root_1") == 0 || strcmp(filename, "testdir") == 0);
        count++;
    }
    ASSERT_EQUALS(count, 4);

    // Can also look up subdirectories by name
    ASSERT_EQUALS(tfsFindEntry(&tfs, dir, "testdir", &mode, &block_idx, &size), 0);

    tfsCloseHandle(dir);
    ASSERT_EQUALS(tfsGetOpenHandleCount(), 0);

    // Create some files in the subdirectory
    ASSERT_NOTEQUALS(file = tfsCreateFile(&tfs, "/testdir", 0644, "child_1"), 0);
    tfsCloseHandle(file);
    ASSERT_NOTEQUALS(file = tfsCreateFile(&tfs, "/testdir", 0644, "child_2"), 0);
    tfsCloseHandle(file);

    ASSERT_NOTEQUALS(dir = tfsOpenPath(&tfs, "/testdir"), NULL);
    idx = 0;
    count = 0;
    while (tfsReadNextEntry(&tfs, dir, &idx, &mode, &block_idx, &size, filename, 256) == 0) {
        ASSERT(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0 || strcmp(filename, "child_1") == 0 || strcmp(filename, "child_2") == 0);
        count++;
    }
    ASSERT_EQUALS(count, 4);

    tfsCloseHandle(dir);
    ASSERT_EQUALS(tfsGetOpenHandleCount(), 0);

    // One more layer of nesting, because reasons
    ASSERT_NOTEQUALS(dir = tfsCreateDirectory(&tfs, "/testdir", "onemore"), NULL);

    idx = 0;
    count = 0;
    while (tfsReadNextEntry(&tfs, dir, &idx, &mode, &block_idx, &size, filename, 256) == 0) {
        ASSERT(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0);
        count++;
    }
    ASSERT_EQUALS(count, 2);

    tfsCloseHandle(dir);
    ASSERT_EQUALS(tfsGetOpenHandleCount(), 0);

    // Create some files in the subdirectory
    ASSERT_NOTEQUALS(file = tfsCreateFile(&tfs, "/testdir/onemore", 0644, "grand_child_1"), 0);
    tfsCloseHandle(file);
    ASSERT_NOTEQUALS(file = tfsCreateFile(&tfs, "/testdir/onemore", 0644, "child_with_a_name_longer_than_ten_characters_because_why_not"), 0);
    tfsCloseHandle(file);

    ASSERT_NOTEQUALS(dir = tfsOpenPath(&tfs, "/testdir/onemore"), NULL);
    idx = 0;
    count = 0;
    while (tfsReadNextEntry(&tfs, dir, &idx, &mode, &block_idx, &size, filename, 256) == 0) {
        ASSERT(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0 || strcmp(filename, "grand_child_1") == 0 || strcmp(filename, "child_with_a_name_longer_than_ten_characters_because_why_not") == 0);
        count++;
    }
    ASSERT_EQUALS(count, 4);

    tfsCloseHandle(dir);
    ASSERT_EQUALS(tfsGetOpenHandleCount(), 0);
    
    return 0;
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
    RUNTEST(test_directories);
    RUNTEST(test_write_files);
    RUNTEST(test_read_files);
    printf("All tests pass. Yay!\n");
    return 0;
}
