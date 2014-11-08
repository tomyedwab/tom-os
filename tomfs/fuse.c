#define FUSE_USE_VERSION  26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>

#include "tomfs.h"

TFS *gTFS;

int kprintf(const char *fmt, ...) {}

int tomfs_read_cb(struct TFS *fs, char *buf, unsigned int block) {
    fseek((FILE*)fs->user_data, block * TFS_BLOCK_SIZE, SEEK_SET);
    if (fread(buf, TFS_BLOCK_SIZE, 1, (FILE*)fs->user_data) != 1) {
        return -1;
    }
    return 0;
}

int tomfs_write_cb(struct TFS *fs, const char *buf, unsigned int block) {
    fseek((FILE*)fs->user_data, block * TFS_BLOCK_SIZE, SEEK_SET);
    if (fwrite(buf, TFS_BLOCK_SIZE, 1, (FILE*)fs->user_data) != 1) {
        return -1;
    }
    return 0;
}

static void tomfs_open_filesystem(char *filename) {
    FILE *fFS = fopen(filename, "r+b");
    TFS *tfs;

    gTFS = NULL;

    if (!fFS) {
        return;
    }
    gTFS = malloc(sizeof(TFS));
    gTFS->read_fn = tomfs_read_cb;
    gTFS->write_fn = tomfs_write_cb;
    gTFS->user_data = fFS;

    if (tfsOpenFilesystem(gTFS) != 0) {
        fclose(fFS);
        free(gTFS);
        gTFS = NULL;
        return;
    }
}

void split_path(const char *path, char *dir_path, char *file_name) {
    int i;
    int last_slash = 0;
    for (i = 0; path[i]; i++) {
        dir_path[i] = path[i];
        if (path[i] == '/') {
            last_slash = i;
        }
    }
    dir_path[last_slash] = '\0';
    for (i = last_slash + 1; path[i]; i++) {
        file_name[i - last_slash - 1] = path[i];
    }
    file_name[i - last_slash - 1] = '\0';
}

static int tomfs_getattr(const char *path, struct stat *stbuf) {
    char dir_path[1024];
    char file_name[256];
    char entry_file_name[256];
    TFSFileEntry *entry;
    FileHandle *dir;
    int idx, mode, block_idx, size;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 1;
        return 0;
    }
    split_path(path, dir_path, file_name);

    if ((dir = tfsOpenPath(gTFS, dir_path)) == NULL) {
        return -ENOENT;
    }
    if (tfsFindEntry(gTFS, dir, file_name, &mode, &block_idx, &size) == 0) {
        stbuf->st_mode = mode;
        stbuf->st_nlink = 1;
        stbuf->st_size = size;
        tfsCloseHandle(dir);
        return 0;
    }

    tfsCloseHandle(dir);
    return -ENOENT;
}

static int tomfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi)
{
    TFSFileEntry *entry;
    char entry_file_name[256];
    FileHandle *dir;
    int idx, mode, block_idx, size;

    if ((dir = tfsOpenPath(gTFS, path)) == NULL) {
        return -ENOENT;
    }

    idx = 0;
    while (tfsReadNextEntry(gTFS, dir, &idx, &mode, &block_idx, &size, entry_file_name, 256) == 0) {
        filler(buf, entry_file_name, NULL, 0);
    }

    return 0;
}

static int tomfs_open(const char *path, struct fuse_file_info *fi)
{
    unsigned int directory_index;
    char dir_path[1024];
    char file_name[256];

    split_path(path, dir_path, file_name);

    fi->fh = tfsOpenFile(gTFS, dir_path, file_name);
    if (fi->fh == NULL) {
        return -ENOENT;
    }

    return 0;
}

static int tomfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
    FileHandle *handle = info->fh;
    int read = tfsReadFile(gTFS, handle, buf, size, offset);
    if (read < 0) {
        return -ENOENT;
    }
    return read;
}

static int tomfs_mkdir(const char *path, mode_t mode) {
    char dir_path[1024];
    char dir_name[256];
    FileHandle *dir;

    split_path(path, dir_path, dir_name);

    dir = tfsCreateDirectory(gTFS, dir_path, dir_name);
    if (dir == NULL) {
        return -ENOMEM;
    }

    tfsCloseHandle(dir);
    return 0;
}

static int tomfs_create(const char *path, mode_t mode, struct fuse_file_info *info) {
    char dir_path[1024];
    char file_name[256];
    split_path(path, dir_path, file_name);

    printf("tomfs_create %s / %s\n", dir_path, file_name);
    info->fh = tfsCreateFile(gTFS, dir_path, mode, file_name);
    if (info->fh == NULL) {
        printf("tfsCreateFile failed.\n");
        return -ENOMEM;
    }

    printf("tfsCreateFile succeeded.\n");
    return 0;
}

static int tomfs_write(const char *path, const char *buf, size_t size, off_t off, struct fuse_file_info *info) {
    FileHandle *handle = info->fh;
    int count;
    if ((count = tfsWriteFile(gTFS, handle, buf, size, off)) < 0) {
        return -ENOENT;
    }
    return count;
}

static int tomfs_flush(const char *path, struct fuse_file_info *info) {
    return 0;
}

static int tomfs_getxattr(const char *path, const char *name, char *value, size_t size) {
    return 0;
}

static int tomfs_unlink(const char *path) {
    char dir_path[1024];
    char file_name[256];

    split_path(path, dir_path, file_name);

    if (tfsDeleteFile(gTFS, dir_path, file_name) != 0) {
        return -ENOENT;
    }

    return 0;
}

static int tomfs_rmdir(const char *path) {
    char dir_path[1024];
    char dir_name[256];

    split_path(path, dir_path, dir_name);

    if (tfsDeleteDirectory(gTFS, dir_path, dir_name) != 0) {
        return -ENOENT;
    }

    return 0;
}

static struct fuse_operations tomfs_oper = {
	.getattr	= tomfs_getattr,
	.readdir	= tomfs_readdir,
	.open		= tomfs_open,
	.read		= tomfs_read,
    .mkdir      = tomfs_mkdir,
    .create     = tomfs_create,
    .write      = tomfs_write,
    .flush      = tomfs_flush,
    .getxattr   = tomfs_getxattr,
    .unlink     = tomfs_unlink,
    .rmdir      = tomfs_rmdir,
};


struct tomfs_config {
     char *file;
};

#define TFS_OPT(t, p, v) { t, offsetof(struct tomfs_config, p), v }

static struct fuse_opt tomfs_opts[] = {
     TFS_OPT("file=%s", file, 0),
     FUSE_OPT_END
};

int main(int argc, char *argv[])
{
     int i;
     struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
     struct tomfs_config conf;
     TFS *tfs;

     memset(&conf, 0, sizeof(conf));

     fuse_opt_parse(&args, &conf, tomfs_opts, NULL);

     if (!conf.file) {
         printf("tomfs_fuse -o file=FILENAME MOUNTPOINT\n");
         return 0;
     }

     tomfs_open_filesystem(conf.file);
     if (!gTFS) {
         printf("Could not open file %s!\n", conf.file);
     }

     tfsInit(gTFS, NULL, 0);

     return fuse_main(args.argc, args.argv, &tomfs_oper, NULL);
}
