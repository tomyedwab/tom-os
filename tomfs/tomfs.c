#include "tomfs.h"

typedef struct FileHandle {
    unsigned int block_index;
    FileHandle *directory;
    unsigned int mode;
    unsigned int current_size;
    unsigned int ref_count;
} FileHandle;

#ifndef EXTERNAL_FILE_HANDLES
#define MAX_FILE_HANDLES 1024

FileHandle gFileHandles[MAX_FILE_HANDLES];
#else
FileHandle *gFileHandles;
int MAX_FILE_HANDLES;
#endif

int kprintf(const char *fmt, ...);

// 60 prime numbers
static int gPrimeNumberTable[] = {
    179, 181, 191, 193, 197, 199, 211, 223, 227, 229,
    233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
    283, 293, 307, 311, 313, 317, 331, 337, 347, 349,
    353, 359, 367, 373, 379, 383, 389, 397, 401, 409,
    419, 421, 431, 433, 439, 443, 449, 457, 461, 463,
    467, 479, 487, 491, 499, 503, 509, 521, 523, 541 };

static FileHandle *get_file_handle(unsigned int block_index, FileHandle *directory, unsigned int mode, unsigned int current_size) {
    int i;
    for (i = 0; i < MAX_FILE_HANDLES; i++) {
        if (gFileHandles[i].block_index == block_index) {
            gFileHandles[i].ref_count++;
            return &gFileHandles[i];
        }
    }
    for (i = 0; i < MAX_FILE_HANDLES; i++) {
        if (gFileHandles[i].block_index == 0) {
            gFileHandles[i].block_index = block_index;
            gFileHandles[i].directory = directory;
            gFileHandles[i].mode = mode;
            gFileHandles[i].current_size = current_size;
            gFileHandles[i].ref_count = 1;
            if (directory) {
                directory->ref_count++;
            }
            return &gFileHandles[i];
        }
    }
    return NULL;
}

void tfsInit(TFS *tfs, FileHandle *handles, int max_handles) {
    int i;
#ifdef EXTERNAL_FILE_HANDLES
    gFileHandles = handles;
    MAX_FILE_HANDLES = max_handles;
#endif
    for (i = 0; i < MAX_FILE_HANDLES; i++) {
        gFileHandles[i].block_index = 0;
    }
}

int tfsWriteFilesystemHeader(TFS *tfs) {
    int i;
    char block_buf[TFS_BLOCK_SIZE];

    for (i = 0; i < sizeof(TFSFilesystemHeader); i++) {
        block_buf[i] = ((char *)&tfs->header)[i];
    }
    for (i = sizeof(TFSFilesystemHeader); i < TFS_BLOCK_SIZE; i++) {
        block_buf[i] = 0;
    }

    if (tfs->write_fn(tfs, block_buf, 0) != 0) {
        return -1;
    }

    return 0;
}

int tfsInitFilesystem(TFS *tfs, int num_blocks) {
    int i;
    char block_buf[TFS_BLOCK_SIZE];
    char bitmap_buf[TFS_BLOCK_SIZE];
    FileHandle *handle;

    // Initialize header
    tfs->header.magic = TFS_MAGIC;
    tfs->header.current_node_id = 1;
    tfs->header.total_blocks = num_blocks;
    // Data blocks are num_blocks - 1 for the filesystem header
    // - ciel((num_blocks - 1) / TFS_BLOCK_GROUP_SIZE) for block bitmaps
    tfs->header.data_blocks = num_blocks - 1 - (num_blocks + TFS_BLOCK_GROUP_SIZE - 2) / TFS_BLOCK_GROUP_SIZE;

    if (tfsWriteFilesystemHeader(tfs) != 0) {
        return -1;
    }

    // Zero out the rest of the structure
    for (i = 0; i < sizeof(TFSFilesystemHeader); i++) {
        block_buf[i] = 0;
    }

    // Create bitmap structure
    for (i = 0; i < TFS_BLOCK_SIZE; i++) {
        bitmap_buf[i] = 0;
    }
    // The bitmap is initialized to have exactly one bit set: the bit
    // corresponding to the block this bitmap is stored in (block 0 in this
    // block group)
    tfsSetBitmapBit(bitmap_buf, 0);

    // Write out all the remaining bitmap & data blocks
    for (i = 1; i < num_blocks; i++) {
        if ((i - 1) % TFS_BLOCK_GROUP_SIZE == 0) {
            if (tfs->write_fn(tfs, bitmap_buf, i) != 0) {
                return -1;
            }
        } else {
            if (tfs->write_fn(tfs, block_buf, i) != 0) {
                return -1;
            }
        }
    }

    // Create root directory
    if ((handle = tfsCreateDirectory(tfs, NULL, NULL)) == NULL) {
        return -1;
    }

    tfsCloseHandle(handle);

    return 0;
}

int tfsOpenFilesystem(TFS *tfs) {
    int i;
    char block_buf[TFS_BLOCK_SIZE];
    if (tfs->read_fn(tfs, block_buf, 0) != 0) {
        return -1;
    }

    for (i = 0; i < sizeof(TFSFilesystemHeader); i++) {
        ((char *)&tfs->header)[i] = block_buf[i];
    }

    if (tfs->header.magic != TFS_MAGIC) {
        return -1;
    }

    return 0;
}

FileHandle *tfsCreateDirectory(TFS *tfs, const char *path, const char *dir_name) {
    FileHandle *handle = tfsCreateFile(tfs, path, 0040755, dir_name);
    if (handle) {
        tfsAppendDirectoryEntry(tfs, handle, 0040755, 0, 0, ".");
        tfsAppendDirectoryEntry(tfs, handle, 0040755, 0, 0, "..");
    }
    return handle;
}

unsigned long hash_filename(char *filename) {
    unsigned long hash = 5381;
    int c;

    while (c = *filename++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

// TODO: Reclaim space from deleted entries
int tfsAppendDirectoryEntry(TFS *tfs, FileHandle *handle, unsigned int mode, unsigned int block_index, unsigned int file_size, char *filename) {
    int i;
    int entry_idx;
    char *fpos;
    TFSFileEntry entry;
    TFSFilenameEntry name_entry;

    entry.mode = mode;
    entry.block_index = block_index;
    entry.file_size = file_size;
    entry.name_hash = (unsigned short)(hash_filename(filename));

    entry_idx = handle->current_size / sizeof(TFSFileEntry);
    entry.filename_entry = entry_idx;

    fpos = filename;
    i = 0;
    do {
        name_entry.mode = TFS_FILENAME_ENTRY;
        name_entry.filename[i++] = *fpos;
        if (i == 10 || !*fpos) {
            name_entry.next_entry = entry_idx + 1;
            if (tfsWriteFile(tfs, handle, (char*)&name_entry, sizeof(TFSFilenameEntry), entry_idx * sizeof(TFSFileEntry)) != sizeof(TFSFilenameEntry)) {
                return -1;
            }
            entry_idx = name_entry.next_entry;
            i = 0;
        }
    } while (*fpos++);

    if (tfsWriteFile(tfs, handle, (char*)&entry, sizeof(TFSFileEntry), entry_idx * sizeof(TFSFileEntry)) != sizeof(TFSFileEntry)) {
        return -1;
    }

    return 0;
}

FileHandle *tfsOpenPath(TFS *tfs, const char *path) {
    FileHandle *handle;
    TFSFileEntry *entry;
    int path_pos = 0;
    char path_entry[256];

    if (path[path_pos] == '/') {
        path_pos++;
    }

    // Open root directory
    handle = get_file_handle(2, NULL, 0040755, tfs->header.root_dir_size);
    if (handle == NULL) {
        return NULL;
    }

    while (path[path_pos]) {
        unsigned int mode, block_index, file_size;
        int idx = 0;
        FileHandle *prev_handle = handle;
        while (path[idx + path_pos] && path[idx + path_pos] != '/' && idx < 255) {
            path_entry[idx] = path[idx + path_pos];
            idx++;
        }
        if (idx == 255) {
            tfsCloseHandle(prev_handle);
            return -1;
        }
        if (path[idx + path_pos] == '/') {
            path_pos += idx + 1;
        } else {
            path_pos += idx;
        }
        path_entry[idx] = '\0';
        if (tfsFindEntry(tfs, handle, path_entry, &mode, &block_index, &file_size) != 0) {
            // Could not find the subdirectory. Release the parent handle.
            tfsCloseHandle(handle);
            return NULL;
        }
        // Found directory
        handle = get_file_handle(block_index, handle, mode, file_size);
        // Release the parent handle since the child handle has a reference to it
        tfsCloseHandle(prev_handle);
        if (!handle) {
            return NULL;
        }
    }
    return handle;
}

int tfsReadNextEntry(TFS *tfs, FileHandle *directory, unsigned int *entry_index, unsigned int *mode, unsigned int *block_index, unsigned int *file_size, char *filename, int filename_size) {
    int i, filename_entry;
    char *fout;
    TFSFileEntry entry;
    TFSFilenameEntry name_entry;
    int num_entries = directory->current_size / sizeof(TFSFileEntry);
    if ((*entry_index) >= num_entries) {
        return -1;
    }

    while (1) {
        if (tfsReadFile(tfs, directory, (char*)&entry, sizeof(TFSFileEntry), (*entry_index) * sizeof(TFSFileEntry)) != sizeof(TFSFileEntry)) {
            return -1;
        }
        if (entry.mode != 0 && entry.mode != TFS_FILENAME_ENTRY) {
            break;
        }
        if (++(*entry_index) >= num_entries) {
            return -1;
        }
    }

    *mode = entry.mode;
    *block_index = entry.block_index;
    *file_size = entry.file_size;
    fout = filename;
    filename_entry = entry.filename_entry;
    while (1) {
        if (tfsReadFile(tfs, directory, (char*)&name_entry, sizeof(TFSFilenameEntry),filename_entry * sizeof(TFSFileEntry)) != sizeof(TFSFileEntry)) {
            return -1;
        }
        for (i = 0; i < 10; i++) {
            *fout = name_entry.filename[i];
            if (fout == &filename[filename_size - 1]) {
                // Ran out of buffer space, so truncate.
                *fout = '\0';
            }
            if (*fout == '\0') {
                break;
            }
            fout++;
        }
        if (i < 10) {
            break;
        }
        filename_entry = name_entry.next_entry;
    }
    ++(*entry_index);
    return 0;
}

int tfsFindEntry(TFS *tfs, FileHandle *directory, char *filename, unsigned int *mode, unsigned int *block_index, unsigned int *file_size) {
    int i;
    TFSFileEntry entry;
    TFSFilenameEntry name_entry;
    unsigned short name_hash = (unsigned short)hash_filename(filename);
    int num_entries = directory->current_size / sizeof(TFSFileEntry);

    for (i = 0; i < num_entries; i++) {
        if (tfsReadFile(tfs, directory, (char*)&entry, sizeof(TFSFileEntry), i * sizeof(TFSFileEntry)) != sizeof(TFSFileEntry)) {
            return -1;
        }
        if (entry.name_hash == name_hash) {
            // Verify by actually comparing the strings
            int filename_entry = entry.filename_entry;
            char *fcmp = filename;
            int mismatch = 0;
            while (1) {
                if (tfsReadFile(tfs, directory, (char*)&name_entry, sizeof(TFSFilenameEntry), filename_entry * sizeof(TFSFileEntry)) != sizeof(TFSFileEntry)) {
                    return -1;
                }
                for (i = 0; i < 10; i++) {
                    if (*fcmp != name_entry.filename[i]) {
                        mismatch = 1;
                        break;
                    }
                    if (*fcmp == '\0') {
                        // Both strings ended - we have a match
                        break;
                    }
                    fcmp++;
                }
                if (mismatch == 1 || *fcmp == '\0') {
                    break;
                }
                filename_entry = name_entry.next_entry;
            }
            if (mismatch == 0) {
                // We have a full match on filename
                *mode = entry.mode;
                *block_index = entry.block_index;
                *file_size = entry.file_size;
                return 0;
            }
        }
    }

    return -1;
}

int tfsUpdateEntry(TFS *tfs, FileHandle *directory, int block_index, unsigned int mode, unsigned int file_size) {
    int i;
    TFSFileEntry entry;
    int num_entries = directory->current_size / sizeof(TFSFileEntry);

    for (i = 0; i < num_entries; i++) {
        if (tfsReadFile(tfs, directory, (char*)&entry, sizeof(TFSFileEntry), i * sizeof(TFSFileEntry)) != sizeof(TFSFileEntry)) {
            return -1;
        }
        if (entry.block_index == block_index) {
            entry.mode = mode;
            entry.file_size = file_size;
            if (!tfsWriteFile(tfs, directory, (char*)&entry, sizeof(TFSFileEntry), i * sizeof(TFSFileEntry)) != sizeof(TFSFileEntry)) {
                return -1;
            }
            return 0;
        }
    }

    return -1;
}

// TODO: Reimplement & write unit tests
int tfsDeleteDirectory(TFS *tfs, const char *path, const char *dir_name) {
    /*
    TFSFileEntry *entry;
    unsigned int directory_index;
    int i, start;
    char dir_path[1024];

    // Open directory we are attempting to delete
    for (i = 0; path[i]; i++) { dir_path[i] = path[i]; }
    dir_path[i] = '/';
    start = i + 1;
    for (i = 0; dir_name[i]; i++) { dir_path[i + start] = dir_name[i]; }
    dir_path[i + start] = '\0';

    if (tfsOpenPath(tfs, dir_path) < 0) {
        return -1;
    }
    // Directory must already be empty (except for . and ..)
    tfsReadNextEntry();
    tfsReadNextEntry();
    if (tfsReadNextEntry() != NULL) {
        return -1;
    }

    directory_index = tfsOpenPath(tfs, path);
    if (directory_index < 0) {
        return -1;
    }
    while (entry = tfsReadNextEntry()) {
        int i;
        for (i = 0; dir_name[i]; i++) {
            if (dir_name[i] != entry->file_name[i]) {
                break;
            }
        }
        if (!dir_name[i] && !entry->file_name[i]) {
            // Remove the entry & dellocate all the directory blocks
            if (tfsUpdateEntry(entry->block_index, TFS_DELETED_FILE, 0) != 0 ||
                tfsDeallocateBlocks(tfs, entry->block_index) != 0) {
                return -1;
            }
            tfsWriteDirectory(tfs);
            return 0;
        }
    }
    */
    return -1;
}

FileHandle *tfsCreateFile(TFS *tfs, const char *path, unsigned int mode, const char *file_name) {
    int block_index;
    FileHandle *dir = NULL, *file;

    if (path) {
        // Find the directory
        if ((dir = tfsOpenPath(tfs, path)) == NULL) {
            return NULL;
        }
    }

    // 2 is the first free block on the filesystem
    block_index = tfsAllocateBlock(tfs, 2, tfs->header.current_node_id, 0, 0);
    if (block_index == 0) {
        tfsCloseHandle(dir);
        return NULL;
    }

    tfs->header.current_node_id++;
    if (tfsWriteFilesystemHeader(tfs) != 0) {
        tfsCloseHandle(dir);
        return NULL;
    }

    if (dir && tfsAppendDirectoryEntry(tfs, dir, mode, block_index, 0, file_name) != 0) {
        tfsCloseHandle(dir);
        return NULL;
    }

    file = get_file_handle(block_index, dir, mode, 0);
    tfsCloseHandle(dir);
    return file;
}

FileHandle *tfsOpenFile(TFS *tfs, char *path, char *file_name) {
    int mode, block_index, file_size;
    TFSFileEntry *entry;
    FileHandle *dir;

    if ((dir = tfsOpenPath(tfs, path)) == NULL) {
        return NULL;
    }
    if (tfsFindEntry(tfs, dir, file_name, &mode, &block_index, &file_size) != 0) {
        return NULL;
    }
    return get_file_handle(block_index, dir, mode, file_size);
}

int tfsWriteFile(TFS *tfs, FileHandle *handle, const char *buf, unsigned int size, unsigned int offset) {
    int i, cur_offset, cur_block_index, next_block_index, bytes_to_write, block_offset, buf_offset;
    char block_buf[TFS_BLOCK_SIZE];
    TFSBlockHeader *header = (TFSBlockHeader*)block_buf;

    if (!handle || handle->block_index == 0) {
        return -1;
    }

    if (offset > handle->current_size) {
        // Can't start a write past the end of the file
        return -1;
    }

    cur_block_index = handle->block_index;
    cur_offset = 0;
    do {
        if (tfs->read_fn(tfs, block_buf, cur_block_index) != 0) {
            return -1;
        }
        if (offset - cur_offset < TFS_BLOCK_DATA_SIZE) {
            // This block contains the start of our data
            block_offset = offset - cur_offset;
            break;
        }
        cur_offset += TFS_BLOCK_DATA_SIZE;
        if (header->next_block == 0) {
            if (offset - cur_offset == 0) {
                // This write coincides with the beginning of a new block, so
                // we don't expect it to exist yet. Create it.
                header->next_block = tfsAllocateBlock(tfs, cur_block_index + 1, header->node_id, header->initial_block, cur_block_index);
                if (header->next_block == 0) {
                    // Failed to allocate block. Abort.
                    return -1;
                }
                // Write the block to update the header
                if (tfs->write_fn(tfs, block_buf, cur_block_index) != 0) {
                    return -1;
                }
            } else {
                // There is no block even though our file size dictates there
                // out to be. Bail.
                return -1;
            }
        }
        cur_block_index = header->next_block;
    } while (1);

    bytes_to_write = size;
    buf_offset = 0;
    while (1) {
        int block_bytes = (bytes_to_write > TFS_BLOCK_DATA_SIZE - block_offset) ? TFS_BLOCK_DATA_SIZE - block_offset : bytes_to_write;

        for (i = 0; i < block_bytes; i++) {
            block_buf[i + block_offset + sizeof(TFSBlockHeader)] = buf[i + buf_offset];
        }
        buf_offset += block_bytes;
        bytes_to_write -= block_bytes;
        block_offset = 0;

        if (bytes_to_write == 0) {
            if (tfs->write_fn(tfs, block_buf, cur_block_index) != 0) {
                return -1;
            }
            break;
        }

        if (header->next_block == 0) {
            // Allocate the next block before writing current block so we update
            // the header appropriately
            header->next_block = tfsAllocateBlock(tfs, cur_block_index + 1, header->node_id, header->initial_block, cur_block_index);
            if (header->next_block == 0) {
                // Failed to allocate block. Abort.
                return -1;
            }
        }

        if (tfs->write_fn(tfs, block_buf, cur_block_index) != 0) {
            return -1;
        }

        cur_block_index = header->next_block;
        if (tfs->read_fn(tfs, block_buf, cur_block_index) != 0) {
            return -1;
        }
    }

    // Did we expand the file past its original size?
    if (offset + size > handle->current_size) {
        // Update handle
        handle->current_size = offset + size;

        // Update directory
        if (handle->directory) {
            tfsUpdateEntry(tfs, handle->directory, handle->block_index, handle->mode, handle->current_size);
        } else {
            // This should only happen for the root directory!
            tfs->header.root_dir_size = handle->current_size;
            tfsWriteFilesystemHeader(tfs);
        }
    }

    return size;
}

int tfsReadFile(TFS *tfs, FileHandle *handle, char *buf, unsigned int size, unsigned int offset) {
    int i, cur_offset, cur_block_index, next_block_index, bytes_to_read, block_offset, buf_offset;
    char block_buf[TFS_BLOCK_SIZE];
    TFSBlockHeader *header = (TFSBlockHeader*)block_buf;

    if (!handle || handle->block_index == 0) {
        return -1;
    }

    if (offset > handle->current_size) {
        // Can't start a read past the end of the file
        return -1;
    }

    cur_block_index = handle->block_index;
    cur_offset = 0;
    do {
        if (tfs->read_fn(tfs, block_buf, cur_block_index) != 0) {
            return -1;
        }
        if (offset - cur_offset < TFS_BLOCK_DATA_SIZE) {
            // This block contains the start of our data
            block_offset = offset - cur_offset;
            break;
        }
        cur_offset += TFS_BLOCK_DATA_SIZE;
        if (header->next_block == 0) {
            // There is no block even though our file size dictates there
            // out to be. Bail.
            return -1;
        }
        cur_block_index = header->next_block;
    } while (1);

    bytes_to_read = size;
    buf_offset = 0;
    while (1) {
        int block_bytes = (bytes_to_read > TFS_BLOCK_DATA_SIZE - block_offset) ? TFS_BLOCK_DATA_SIZE - block_offset : bytes_to_read;

        for (i = 0; i < block_bytes; i++) {
            buf[i + buf_offset] = block_buf[i + block_offset + sizeof(TFSBlockHeader)];
        }
        buf_offset += block_bytes;
        bytes_to_read -= block_bytes;
        block_offset = 0;

        if (bytes_to_read == 0 || header->next_block == 0) {
            break;
        }

        // Read the next block
        cur_block_index = header->next_block;
        if (tfs->read_fn(tfs, block_buf, cur_block_index) != 0) {
            return -1;
        }
    }

    if (bytes_to_read > 0) {
        // Hit the end of the file, so return the number of bytes actuall read
        size -= bytes_to_read;
    }
    return size;
}

// TODO: Reimplement & write unit tests
int tfsDeleteFile(TFS *tfs, char *path, char *file_name) {
    /*
    TFSFileEntry *entry;
    unsigned int directory_index;

    directory_index = tfsOpenPath(tfs, path);
    if (directory_index < 0) {
        return -1;
    }
    while (entry = tfsReadNextEntry()) {
        int i;
        for (i = 0; file_name[i]; i++) {
            if (file_name[i] != entry->file_name[i]) {
                break;
            }
        }
        if (!file_name[i] && !entry->file_name[i]) {
            // Remove the entry & dellocate all the file blocks
            if (tfsUpdateEntry(entry->block_index, TFS_DELETED_FILE, 0) != 0 ||
                tfsDeallocateBlocks(tfs, entry->block_index) != 0) {
                return -1;
            }
            tfsWriteDirectory(tfs);
            return 0;
        }
    }
    */
    return -1;
}

void tfsSetBitmapBit(char *bitmap_buf, int block_index) {
    int cur_bit_offset = 2048 * 8;
    while (cur_bit_offset > 0) {
        int byte_offset = (cur_bit_offset + block_index) >> 3;
        int bit_mask = 1 << ((cur_bit_offset + block_index) & 0x7);
        int alternate_bit_mask;
        bitmap_buf[byte_offset] |= bit_mask;
        switch (bit_mask) {
        case 0x01: alternate_bit_mask = 0x02; break;
        case 0x02: alternate_bit_mask = 0x01; break;
        case 0x04: alternate_bit_mask = 0x08; break;
        case 0x08: alternate_bit_mask = 0x04; break;
        case 0x10: alternate_bit_mask = 0x20; break;
        case 0x20: alternate_bit_mask = 0x10; break;
        case 0x40: alternate_bit_mask = 0x80; break;
        case 0x80: alternate_bit_mask = 0x40; break;
        }
        if ((bitmap_buf[byte_offset] & alternate_bit_mask) == 0) {
            // The other branch of the binary tree is not full, so the parent
            // is not full either. Break.
            break;
        }

        cur_bit_offset >>= 1;
        block_index >>= 1;
    }
}

void tfsClearBitmapBit(char *bitmap_buf, int block_index) {
    int cur_bit_offset = 2048 * 8;
    while (cur_bit_offset > 0) {
        int byte_offset = (cur_bit_offset + block_index) >> 3;
        int bit_mask = 1 << ((cur_bit_offset + block_index) & 0x7);
        int alternate_bit_mask;

        if ((bitmap_buf[byte_offset] & bit_mask) == 0) {
            // This bit is already clear; we can exit early
            break;
        }
        bitmap_buf[byte_offset] &= ~bit_mask;
        switch (bit_mask) {
        case 0x01: alternate_bit_mask = 0x02; break;
        case 0x02: alternate_bit_mask = 0x01; break;
        case 0x04: alternate_bit_mask = 0x08; break;
        case 0x08: alternate_bit_mask = 0x04; break;
        case 0x10: alternate_bit_mask = 0x20; break;
        case 0x20: alternate_bit_mask = 0x10; break;
        case 0x40: alternate_bit_mask = 0x80; break;
        case 0x80: alternate_bit_mask = 0x40; break;
        }

        cur_bit_offset >>= 1;
        block_index >>= 1;
    }
}

int tfsCheckBitmapBit(char *bitmap_buf, int block_num) {
    int byte_offset = 2048 + (block_num >> 3);
    int bit_mask = 1 << (block_num & 0x7);
    return (bitmap_buf[byte_offset] & bit_mask) ? 1 : 0;
}

int tfsAttemptToAllocateBlock(TFS *tfs, int block_index) {
    char block_bitmap[TFS_BLOCK_SIZE];
    int block_group_num = (block_index - 1) / TFS_BLOCK_GROUP_SIZE;
    int block_num = (block_index - 1) % TFS_BLOCK_GROUP_SIZE;

    if (tfs->read_fn(tfs, block_bitmap, 1 + block_group_num * TFS_BLOCK_GROUP_SIZE) != 0) {
        return -1;
    }

    if (tfsCheckBitmapBit(block_bitmap, block_num) == 1) {
        return -1;
    }

    tfsSetBitmapBit(block_bitmap, block_num);
    if (tfs->write_fn(tfs, block_bitmap, 1 + block_group_num * TFS_BLOCK_GROUP_SIZE) != 0) {
        return -1;
    }

    return 0;
}

int find_empty_block_recursive(char *block_bitmap, int level, int idx, int *seed, int stride, int modulo, int block_group_size) {
    int ret;
    int bit_index = (1 << (14 - level)) + idx;
    int byte_index = bit_index >> 3;
    int bit_mask = 1 << (bit_index & 0x7);

    // Check for out-of-bounds first
    if ((idx << level) >= block_group_size) {
        // Block is past the end of the filesystem
        return -1;
    }

    // Check the "subtree full" bit at this level
    if ((block_bitmap[byte_index] & bit_mask) != 0) {
        // This subtree is full.
        return -1;
    }

    if (level == 0) {
        // We are at the block level, so we must have an available block
        return idx;
    }

    // Flip a coin to determine whether to go left or right first
    if ((*seed) % modulo < (modulo >> 1)) {
        *seed += stride;
        ret = find_empty_block_recursive(block_bitmap, level - 1, (idx << 1) + 0, seed, stride, modulo, block_group_size);
        if (ret >= 0) {
            return ret;
        }
        ret = find_empty_block_recursive(block_bitmap, level - 1, (idx << 1) + 1, seed, stride, modulo, block_group_size);
        if (ret >= 0) {
            return ret;
        }
    } else {
        *seed += stride;
        ret = find_empty_block_recursive(block_bitmap, level - 1, (idx << 1) + 1, seed, stride, modulo, block_group_size);
        if (ret >= 0) {
            return ret;
        }
        ret = find_empty_block_recursive(block_bitmap, level - 1, (idx << 1) + 0, seed, stride, modulo, block_group_size);
        if (ret >= 0) {
            return ret;
        }
    }

    // Didn't find one in either subtree. This can happen legitimately if the
    // right subtree extends beyond the end of the filesystem
    return -1;
}

int tfsFindEmptyBlock(TFS *tfs) {
    int i;
    char block_bitmap[TFS_BLOCK_SIZE];
    int num_block_groups = (tfs->header.total_blocks + (TFS_BLOCK_GROUP_SIZE - 1)) / TFS_BLOCK_GROUP_SIZE;
    int seed = tfs->header.seed;
    int stride = gPrimeNumberTable[tfs->header.stride_offset];
    int modulo = 1291; // TODO: Random modulo?
    int found_block = -1;
    for (i = 0; i < num_block_groups; i++) {
        // Look for free blocks in group (seed % num_block_groups)
        int block_group_num = seed % num_block_groups;
        // The last block group may have fewer blocks than the rest, so we
        // shouldn't return blocks past the end of the filesystem
        int block_group_size = (i == (num_block_groups - 1)) ?
            (tfs->header.total_blocks - 1) % TFS_BLOCK_GROUP_SIZE : TFS_BLOCK_GROUP_SIZE;
        int block_num;

        // Load the block bitmap for the block group
        if (tfs->read_fn(tfs, block_bitmap, 1 + block_group_num * TFS_BLOCK_GROUP_SIZE) != 0) {
            break;
        }

        block_num = find_empty_block_recursive(block_bitmap, 14, 0, &seed, stride, modulo, block_group_size);
        if (block_num >= 0) {
            // We found a block, return it
            found_block = 1 + (block_group_num * TFS_BLOCK_GROUP_SIZE) + block_num;
            break;
        }
        
        seed += stride;
    }

    // Update our seed & stride offset
    tfs->header.seed = seed;
    tfs->header.stride_offset = (tfs->header.stride_offset + 1) % 60;
    tfsWriteFilesystemHeader(tfs);

    return found_block;
}

int tfsAllocateBlock(TFS *tfs, int desired_block_index, unsigned int node_id, unsigned int initial_block, unsigned int previous_block) {
    int i;
    TFSBlockHeader header;
    char block_buf[TFS_BLOCK_SIZE];
    int block_index = 0;
    if (tfsAttemptToAllocateBlock(tfs, desired_block_index) == 0) {
        block_index = desired_block_index;
    }

    if (block_index == 0) {
        block_index = tfsFindEmptyBlock(tfs);
        if (block_index < 0) {
            return 0;
        }
        if (tfsAttemptToAllocateBlock(tfs, block_index) != 0) {
            return 0;
        }
    }

    header.node_id = node_id;
    header.initial_block = (initial_block == 0) ? block_index : initial_block;
    header.previous_block = previous_block;
    header.next_block = 0;

    for (i = 0; i < sizeof(TFSBlockHeader); i++) {
        block_buf[i] = ((char *)&header)[i];
    }
    for (i = sizeof(TFSBlockHeader); i < TFS_BLOCK_SIZE; i++) {
        block_buf[i] = 0;
    }

    if (tfs->write_fn(tfs, block_buf, block_index) != 0) {
        return 0;
    }

    return block_index;
}

int tfsWriteBlockData(TFS *tfs, char *data, int block_index) {
    int i;
    char block_buf[TFS_BLOCK_SIZE];

    if (tfs->read_fn(tfs, block_buf, block_index) != 0) {
        return -1;
    }

    for (i = sizeof(TFSBlockHeader); i < TFS_BLOCK_SIZE; i++) {
        block_buf[i] = data[i - sizeof(TFSBlockHeader)];
    }

    if (tfs->write_fn(tfs, block_buf, block_index) != 0) {
        return -1;
    }

    return 0;
}

// TODO: Follow the linked list and deallocate all the blocks
int tfsDeallocateBlocks(TFS *tfs, int block_index) {
    char block_bitmap[TFS_BLOCK_SIZE];
    int block_group_num = (block_index - 1) / TFS_BLOCK_GROUP_SIZE;
    int block_num = (block_index - 1) % TFS_BLOCK_GROUP_SIZE;

    if (tfs->read_fn(tfs, block_bitmap, 1 + block_group_num * TFS_BLOCK_GROUP_SIZE) != 0) {
        return -1;
    }

    tfsClearBitmapBit(block_bitmap, block_num);
    if (tfs->write_fn(tfs, block_bitmap, 1 + block_group_num * TFS_BLOCK_GROUP_SIZE) != 0) {
        return -1;
    }

    return 0;

    return 0;
}

void tfsCloseHandle(FileHandle *handle) {
    if (!handle) return;

    handle->ref_count--;
    if (handle->ref_count == 0) {
        if (handle->directory) {
            // Recursively release the parent directory handle
            tfsCloseHandle(handle->directory);
        }
        handle->block_index = 0;
        handle->directory = NULL;
        handle->mode = 0;
        handle->current_size = 0;
    }
}

int tfsGetOpenHandleCount() {
    int i, count = 0;
    for (i = 0; i < MAX_FILE_HANDLES; i++) {
        if (gFileHandles[i].block_index != 0) {
            count += gFileHandles[i].ref_count;
        }
    }
    return count;
}

