#include "fs_utils.h"

int do_getattr( const char *path, struct stat *st);
int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
static ChanFSObj *traverse(const char *path);


