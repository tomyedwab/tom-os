#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tomfs.h"

int byte_offset = 0;

int read_fn(struct TFS *fs, char *buf, unsigned int block) {
    fseek((FILE *)fs->user_data, block*TFS_BLOCK_SIZE + byte_offset, SEEK_SET);
    fread(buf, 1, TFS_BLOCK_SIZE, (FILE *)fs->user_data);
    return 0;
}

int main(int argc, char *argv[]) {
    TFS tfs;
    FILE *fIn;
    if (argc < 3) {
        printf("cat_file image filename [byte_offset]\n");
        return 0;
    }
    if (argc == 4) {
        byte_offset = atoi(argv[3]);
    }

    fIn = fopen(argv[1], "rb");
    if (!fIn) {
        printf("Failed to read from file.\n");
        return -1;
    }

    tfsInit(&tfs, NULL, 0);
    tfs.read_fn = &read_fn;
    tfs.user_data = fIn;

    if (tfsOpenFilesystem(&tfs) == 0) {
        char dir[256], filename[256];
        int idx;
        FileHandle *handle;

        // Parse path

        for (idx = strlen(argv[2])-1; idx > 0 && argv[2][idx] != '/'; --idx) {}
        argv[2][idx] = 0;
        strcpy(filename, &argv[2][idx+1]);
        strcpy(dir, argv[2]);

        // Open file

        handle = tfsOpenFile(&tfs, dir, filename);
        if (handle) {
            // Read file
            char buffer[513];
            int len, offset = 0;
            while ((len = tfsReadFile(&tfs, handle, buffer, 512, offset)) > 0) {
                buffer[len] = 0;
                printf("%s", buffer);
                offset += len;
            }
            tfsCloseHandle(handle);
        } else {
            printf("Failed to open file %s in directory %s.\n", filename, dir);
        }
    } else {
        printf("Failed to open filesystem.\n");
    }

    fclose(fIn);
    return 0;
}
