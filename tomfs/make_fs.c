#include <stdio.h>
#include <stdlib.h>

#include "tomfs.h"

int read_fn(struct TFS *fs, char *buf, unsigned int block) {
    fseek((FILE *)fs->user_data, block*TFS_BLOCK_SIZE, SEEK_SET);
    fread(buf, 1, TFS_BLOCK_SIZE, (FILE *)fs->user_data);
    return 0;
}

int write_fn(struct TFS *fs, char *buf, unsigned int block) {
    fseek((FILE *)fs->user_data, block*TFS_BLOCK_SIZE, SEEK_SET);
    fwrite(buf, 1, TFS_BLOCK_SIZE, (FILE *)fs->user_data);
    return 0;
}

int main(int argc, const char *argv[]) {
    TFS tfs;
    FILE *fOut;
    if (argc < 2) {
        printf("make_fs image\n");
        return 0;
    }

    fOut = fopen(argv[1], "w+b");
    if (!fOut) {
        printf("Failed to write to file.\n");
        return -1;
    }

    tfs.read_fn = &read_fn;
    tfs.write_fn = &write_fn;
    tfs.user_data = fOut;
    if (tfsInitFilesystem(&tfs, 2560) != 0) {
        printf("Failed to initialize filesystem.\n");
        return -1;
    }
    fclose(fOut);

    return 0;
}

