#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <curl/curl.h>

#include "include/fs.h"

ChanFSObj *root;

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .read = do_read,
    .readdir = do_readdir,
};

int main(int argc, char *argv[])
{   
    curl_global_init(CURL_GLOBAL_ALL);

    root = generate_fs();
    int fuse_res = fuse_main(argc, argv, &operations, NULL);

    curl_global_cleanup();


    return fuse_res;
}