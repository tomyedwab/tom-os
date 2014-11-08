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

#ifndef NULL
#define NULL 0
#endif

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

    // The current seed for the internal RNG
    unsigned int seed;

    // The offset into the primes table for our stride
    unsigned short stride_offset;

    // The size of the root directory data
    unsigned int root_dir_size;
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

#define TFS_FILENAME_ENTRY 0xFFFFFFFF

typedef struct TFSFileEntry {
    // The mode of the file or directory
    // If this is zero, then the entry is free. If this is TFS_FILENAME_ENTRY,
    // then the entry is in fact a TFSFilenameEntry and not a TFSFileEntry.
    unsigned int mode;

    // The index of the first block for this file
    unsigned int block_index;

    // The size of the file in bytes
    unsigned int file_size;

    // A hash of the name, for efficient lookups
    unsigned short name_hash;

    // Entry number for the first TFSFilenameEntry block of this entry's filename
    unsigned short filename_entry;
} TFSFileEntry;

typedef struct TFSFilenameEntry {
    // For a TFSFilenameEntry this will always be TFS_FILENAME_ENTRY.
    unsigned int mode;

    // The entry number for the next TFSFilenameEntry. This is ignored if
    // there is a null terminator in 'filename'.
    unsigned short next_entry;

    // The next 10 characters in the filename.
    char filename[10];
} TFSFilenameEntry;

typedef struct FileHandle FileHandle;

// This is the size of the FileHandle stucture, so that the caller can
// allocate their own file handle array. Must be kept in sync with FileHandle,
// unfortunately
#define TFS_FILE_HANDLE_SIZE 20

// Public API

// Must be called before doing any other operations
void tfsInit(TFS *tfs, FileHandle *handles, int max_handles);

// Returns 0 on successful initialization of a new filesystem
int tfsInitFilesystem(TFS *tfs, int num_blocks);

// Returns 0 on successful opening of an existing filesystem
int tfsOpenFilesystem(TFS *tfs);

// Directory API

// Returns a file handle for the new directory, or NULL on failure
FileHandle *tfsCreateDirectory(TFS *tfs, const char *path, const char *dir_name);

// Opens a directory for reading by path (0 on success)
FileHandle *tfsOpenPath(TFS *tfs, const char *path);

// Reads values into the parameters for the next entry in the given directory.
// Assumes that all the parameters are zeroed out.
// Returns 0 on success.
int tfsReadNextEntry(TFS *tfs, FileHandle *directory, unsigned int *entry_index, unsigned int *mode, unsigned int *block_index, unsigned int *file_size, char *filename, int filename_size);

// Finds an entry by name and returns the mode, block index and size.
// Returns 0 on success.
int tfsFindEntry(TFS *tfs, FileHandle *directory, char *filename, unsigned int *mode, unsigned int *block_index, unsigned int *file_size);

// Update an existing entry (returns 0 on success)
int tfsUpdateEntry(TFS *tfs, FileHandle *directory, int block_index, unsigned int mode, unsigned int file_size);

// Removes a directory from the parent directory & filesystem
// Directory must be empty
int tfsDeleteDirectory(TFS *tfs, const char *path, const char *dir_name);

// Files API

// Creates a new file in the directory specified by 'path' with the given filename.
// Returns a file handle for the new file, or NULL on failure
FileHandle *tfsCreateFile(TFS *tfs, const char *path, unsigned int mode, const char *file_name);

// Opens a file by path and filename
// Returns a file handle for the file, or NULL on failure
FileHandle *tfsOpenFile(TFS *tfs, char *path, char *file_name);

// Writes 'size' bytes from 'buf' into the file at offset 'offset'
int tfsWriteFile(TFS *tfs, FileHandle *handle, const char *buf, unsigned int size, unsigned int offset);

// Reads up to 'size' bytes from 'buf' from the file at the offset 'offset.
// Returns the number of bytes actually read, or -1 on error.
int tfsReadFile(TFS *tfs, FileHandle *handle, char *buf, unsigned int size, unsigned int offset);

// Removes the file from the directory & filesystem
int tfsDeleteFile(TFS *tfs, char *path, char *file_name);

// Closes a handle to a file or directory
void tfsCloseHandle(FileHandle *handle);

// Returns the number of currently in-use handles
int tfsGetOpenHandleCount();

// Internals
void tfsSetBitmapBit(char *bitmap_buf, int block_index);
void tfsClearBitmapBit(char *bitmap_buf, int block_index);
int tfsCheckBitmapBit(char *bitmap_buf, int block_index);
int tfsAllocateBlock(TFS *tfs, int desired_block_index, unsigned int node_id, unsigned int initial_block, unsigned int previous_block);
int tfsWriteBlockData(TFS *tfs, char *data, int block_index);
int tfsDeallocateBlocks(TFS *tfs, int block_index);
