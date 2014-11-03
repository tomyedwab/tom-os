#include <stdio.h> // TODO: donotcheckin

#include "tomfs.h"

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
    if (tfsCreateDirectory(tfs) == 0) {
        return -1;
    }

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

int directory_index;
unsigned char directory_block[TFS_BLOCK_SIZE];
int directory_offset;
unsigned char directory_entry[sizeof(TFSFileEntry) + 256];

int tfsCreateDirectory(TFS *tfs) {
    // 2 is the first free block on an empty filesystem
    int block_index = tfsAllocateBlock(tfs, 2, tfs->header.current_node_id, 0, 0);
    if (block_index == 0) {
        return 0;
    }

    tfs->header.current_node_id++;
    if (tfsWriteFilesystemHeader(tfs) != 0) {
        return 0;
    }

    directory_index = block_index;
    directory_offset = sizeof(TFSBlockHeader);
    tfsWriteNextEntry(0040755, 0, ".");
    tfsWriteNextEntry(0040755, 0, "..");

    tfsWriteBlockData(tfs, &directory_block[sizeof(TFSBlockHeader)], block_index);

    return block_index;
}

int tfsOpenDirectory(TFS *tfs, int block_index) {
    directory_index = block_index;

    if (tfs->read_fn(tfs, directory_block, directory_index) != 0) {
        return -1;
    }
    directory_offset = sizeof(TFSBlockHeader);
    return 0;
}

int tfsOpenPath(TFS *tfs, const char *path) {
    TFSFileEntry *entry;
    int path_pos = 0;

    if (path[path_pos] == '/') {
        path_pos++;
    }

    // Open root
    if (tfsOpenDirectory(tfs, 2) != 0) {
        return -1;
    }

    while (path[path_pos]) {
        int next_index = 0;
        while (entry = tfsReadNextEntry()) {
            int c;
            for (c = 0; c < entry->name_size; c++) {
                if (entry->file_name[c] != path[path_pos + c]) {
                    break;
                }
            }
            if (c == entry->name_size &&
                (path[path_pos + c] == '\0' || path[path_pos + c] == '/')) {
                // Found!
                path_pos += c;
                next_index = entry->block_index;
                break;
            }
        }
        if (next_index == 0) {
            // Could not find the subdirectory
            return -1;
        }

        if (tfsOpenDirectory(tfs, next_index) != 0) {
            // Could not open the subdirectory
            return -1;
        }
    }
    return directory_index;
}

TFSFileEntry *tfsReadNextEntry() {
    int i;
    TFSFileEntry *entry;
    for (i = 0; i < sizeof(TFSFileEntry); i++) {
        directory_entry[i] = directory_block[directory_offset + i];
    }
    entry = (TFSFileEntry*)directory_entry;
    if (entry->mode == 0) {
        // Mode = 0 signifies EOF
        return NULL;
    }
    for (i = sizeof(TFSFileEntry); i < sizeof(TFSFileEntry) + entry->name_size - 1; i++) {
        directory_entry[i] = directory_block[directory_offset + i];
    }
    directory_entry[i] = '\0';
    directory_offset += i;
    return entry;
}

int tfsWriteNextEntry(unsigned int mode, int block_index, const char *name) {
    // TODO: Handle spanning into the next block
    int i;
    TFSFileEntry *entry = (TFSFileEntry*)&directory_block[directory_offset];
    if (entry->mode != 0) {
        // Attempting to write over an existing entry
        return -1;
    }
    entry->mode = mode;
    entry->block_index = block_index;
    entry->file_size = 0;
    entry->name_size = 0;
    for (i = 0; name[i]; i++) {
        entry->file_name[i] = name[i];
        entry->name_size++;
    }

    directory_offset += sizeof(TFSFileEntry) + entry->name_size - 1;
    entry = (TFSFileEntry*)&directory_block[directory_offset];
    entry->mode = 0;
    return 0;
}

void tfsWriteDirectory(TFS *tfs) {
    tfsWriteBlockData(tfs, &directory_block[sizeof(TFSBlockHeader)], directory_index);
}

int tfsCreateFile(TFS *tfs) {
    // 3 is the first free block on a filesystem with just one directory (root)
    int block_index = tfsAllocateBlock(tfs, 3, tfs->header.current_node_id, 0, 0);
    if (block_index == 0) {
        return 0;
    }

    tfs->header.current_node_id++;
    if (tfsWriteFilesystemHeader(tfs) != 0) {
        return 0;
    }

    return block_index;
}

// TODO: Check extents and support writing across multiple blocks / extending the file
int tfsWriteFile(TFS *tfs, unsigned int file_index, char *buf, unsigned int size, unsigned int offset) {
    int i;
    char block_buf[TFS_BLOCK_SIZE];

    if (tfs->read_fn(tfs, block_buf, file_index) != 0) {
        return -1;
    }

    for (i = 0; i < size; i++) {
        block_buf[i + offset] = buf[i];
    }

    if (tfs->write_fn(tfs, block_buf, file_index) != 0) {
        return -1;
    }

    return 0;
}

// TODO: Check extents and support writing across multiple blocks / extending the file
int tfsReadFile(TFS *tfs, unsigned int file_index, char *buf, unsigned int size, unsigned int offset) {
    int i;
    char block_buf[TFS_BLOCK_SIZE];

    if (tfs->read_fn(tfs, block_buf, file_index) != 0) {
        return -1;
    }

    for (i = 0; i < size; i++) {
        buf[i] = block_buf[offset + i];
    }
    return size;
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

// TODO: Unit tests for this
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

// TODO: Unit tests for this
int tfsAllocateBlock(TFS *tfs, int desired_block_index, unsigned int node_id, unsigned int initial_block, unsigned int previous_block) {
    int i;
    TFSBlockHeader header;
    char block_buf[TFS_BLOCK_SIZE];
    int block_index = 0;
    if (tfsAttemptToAllocateBlock(tfs, desired_block_index) == 0) {
        block_index = desired_block_index;
    }

    if (block_index == 0) {
        for (i = 0; i < 24; i++) {
            // TODO: Pick a random block number and try again
            desired_block_index++;
            if (tfsAttemptToAllocateBlock(tfs, desired_block_index) == 0) {
                block_index = desired_block_index;
                break;
            }
        }
        if (block_index == 0) {
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

