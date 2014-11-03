// TomFS definitions

// File layout:
// Block 0     - TFSFilesystemHeader
// Block 1     - Block bitmap for blocks 1..16384
// Block 2     - Data block
// ..
// Block 16384 - Data block
// Block 16385 - Block bitmap for blocks 16386..32769
// Block 16386 - Data block
// ...

// Block size is the same as memory page size
#define TFS_BLOCK_SIZE        4096

// A bitmap for a block group is half a block, so we can store a whole binary
// tree in one block
// 2048*8 = 16384
#define TFS_BLOCK_GROUP_SIZE  16384

// Therefore, a block group is 16384 * 4096 = 67,108,864 (64 MB)

// Magic number that identifies this FS
#define TFS_MAGIC             0x0e5c

typedef struct {
    // See TFS_MAGIC
    unsigned short magic;

    // Next unique ID to assign
    unsigned int current_node_id;

    // Total number of blocks in the filesystem
    unsigned int total_blocks;

    // Total number of *data* blocks in the filesystem
    unsigned int data_blocks;
} TFSFilesystemHeader;

typedef struct {
    // Unique ID (or 0 if this block is free)
    unsigned int node_id;
    // Block index of the first block in the file
    unsigned int initial_block;
    // Block index of the last block in the file (or 0 if this is the first)
    unsigned int previous_block;
    // Block index of the next block in the file (or 0 if this is the last)
    unsigned int next_block;
} TFSBlockHeader;

// Block header is followed by 4096-16 = 4080 bytes of data
#define TFS_BLOCK_DATA_SIZE    (TFS_BLOCK_SIZE - sizeof(TFSBlockHeader))

typedef struct TFS {
    // A callback to read a block from the device at the specified blocknum
    // 'fs' is a pointer to this data structure
    // 'buf' is a buffer of size TFS_BLOCK_SIZE to write to
    // 'block' is the block index to read from
    int (*read_fn)(struct TFS *fs, char *buf, unsigned int block);

    // A callback to write a block to the device at the specified blocknum
    // 'fs' is a pointer to this data structure
    // 'buf' is a buffer of size TFS_BLOCK_SIZE to read from
    // 'block' is the block index to write to
    int (*write_fn)(struct TFS *fs, const char *buf, unsigned int block);

    // Userdata to be passed to read_fn/write_fn
    void *user_data;

    // Internal use only
    TFSFilesystemHeader header;
} TFS;

typedef struct TFSFileEntry {
    // The mode of the file or directory
    unsigned int mode;

    // The index of the first block for this file
    unsigned int block_index;

    // The size of the file in bytes
    unsigned int file_size;

    // The size of the filename in bytes (filename is not zero-terminated)
    unsigned short name_size;

    char unused; // For padding

    // The filename
    char file_name[1];
} TFSFileEntry;

// Public API

// Returns 0 on successful initialization of a new filesystem
int tfsInitFilesystem(TFS *tfs, int num_blocks);

// Returns 0 on successful opening of an existing filesystem
int tfsOpenFilesystem(TFS *tfs);

// Directory API

// Returns a block number for the new directory, or 0 on failure
int tfsCreateDirectory(TFS *tfs);

// Opens a directory for reading by block (0 on success)
int tfsOpenDirectory(TFS *tfs, int block_index);

// Opens a directory for reading by path (0 on success)
int tfsOpenPath(TFS *tfs, const char *path);

// Returns the next file entry, or NULL for EOF
TFSFileEntry *tfsReadNextEntry();

// Writes an entry at the end of the currently open directory
// NOTE: Don't forget to call tfsWriteDirectory() after adding all the new
// entries!
int tfsWriteNextEntry(unsigned int mode, int block_index, const char *name);

// Write out any new directory entries in the currently open directory
void tfsWriteDirectory(TFS *tfs);

// Files API

// Returns a block number for the new file, or 0 on failure
int tfsCreateFile(TFS *tfs);

// Writes 'size' bytes from 'buf' into the file at offset 'offset'
int tfsWriteFile(TFS *tfs, unsigned int file_index, char *buf, unsigned int size, unsigned int offset);

// Reads up to 'size' bytes from 'buf' from the file at the offset 'offset.
// Returns the number of bytes actually read, or -1 on error.
int tfsReadFile(TFS *tfs, unsigned int file_index, char *buf, unsigned int size, unsigned int offset);

// Internals
void tfsSetBitmapBit(char *bitmap_buf, int block_index);
void tfsClearBitmapBit(char *bitmap_buf, int block_index);
int tfsCheckBitmapBit(char *bitmap_buf, int block_index);
int tfsAllocateBlock(TFS *tfs, int desired_block_index, unsigned int node_id, unsigned int initial_block, unsigned int previous_block);
int tfsWriteBlockData(TFS *tfs, char *data, int block_index);
