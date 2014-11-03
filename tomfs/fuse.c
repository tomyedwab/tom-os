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
    TFSFileEntry *entry;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 1;
        return 0;
    }

    split_path(path, dir_path, file_name);

    if (tfsOpenPath(gTFS, dir_path) < 0) {
        return -ENOENT;
    }
    while (entry = tfsReadNextEntry()) {
        if (strcmp(file_name, entry->file_name) == 0) {
            stbuf->st_mode = entry->mode;
            stbuf->st_nlink = 1;
            stbuf->st_size = entry->file_size;
            return 0;
        }
    }

    return -ENOENT;
}

static int tomfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi)
{
    TFSFileEntry *entry;
    (void) offset;
    (void) fi;

    if (tfsOpenPath(gTFS, path) < 0) {
        return -ENOENT;
    }

    while (entry = tfsReadNextEntry()) {
        filler(buf, entry->file_name, NULL, 0);
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
    char file_name[256];
    int directory_index = tfsCreateDirectory(gTFS);
    if (directory_index == 0) {
        return -ENOMEM;
    }

    split_path(path, dir_path, file_name);

    if (tfsOpenPath(gTFS, dir_path) < 0) {
        return -ENOENT;
    }

    while (tfsReadNextEntry()) { }

    if (tfsWriteNextEntry(mode | 0040000, directory_index, file_name) != 0) {
        return -ENOMEM;
    }
    tfsWriteDirectory(gTFS);
    return 0;
}

static int tomfs_create(const char *path, mode_t mode, struct fuse_file_info *info) {
    char dir_path[1024];
    char file_name[256];
    split_path(path, dir_path, file_name);

    info->fh = tfsCreateFile(gTFS, dir_path, mode, file_name);
    if (info->fh == NULL) {
        return -ENOMEM;
    }

    return 0;
}

static int tomfs_write(const char *path, const char *buf, size_t size, off_t off, struct fuse_file_info *info) {
    FileHandle *handle = info->fh;
    if (tfsWriteFile(gTFS, handle, buf, size, off) != 0) {
        return -ENOENT;
    }
    return 0;
}

static int tomfs_flush(const char *path, struct fuse_file_info *info) {
    return 0;
}

static int tomfs_getxattr(const char *path, const char *name, char *value, size_t size) {
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
    .getxattr   = tomfs_getxattr
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

     tfsInit(gTFS);

     return fuse_main(args.argc, args.argv, &tomfs_oper, NULL);
}
