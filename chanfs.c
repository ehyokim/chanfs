#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>

#include "include/fs.h"

ChanFSObj *root;

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .read = do_read,
    .readdir = do_readdir,
};

int main(int argc, char *argv[])
{
    root = generate_fs();
    return fuse_main(argc, argv, &operations, NULL);
}