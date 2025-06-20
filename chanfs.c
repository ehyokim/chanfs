#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fuse.h>
#include <curl/curl.h>

#include "include/fs_utils.h"
#include "include/fs.h"

#define MAXNUMBOARDS 500 // Maximum number of boards allowed.
#define MAXNUMFUSEARGS 500

static char* get_board_name(char *board_str);

ChanFSObj *root;
char *chan;

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .read = do_read,
    .readdir = do_readdir,
};

int main(int argc, char *argv[])
{   
    curl_global_init(CURL_GLOBAL_ALL);

    char *board_strs[MAXNUMBOARDS];
    char *fuse_arg_strs[MAXNUMFUSEARGS];
    int next_chanfs_idx = 0;
    int next_fuse_idx = 0;
    chan = NULL;

    /* Divide out the arguments into those that feed into chanfs and those that feed into fuse_main. */
    int i;
    char *arg;
    int len;
    for (i = 0; i < argc && next_chanfs_idx < MAXNUMBOARDS; i++) {
        arg = argv[i];
        len = strlen(arg);
        if (arg[0] == '-' && arg[1] == '/' && arg[len - 1] == '/') //Check if the argument is a board.
            board_strs[next_chanfs_idx++] = get_board_name(argv[i] + 1); 
        else if (arg[0] == '-' && arg[1] == 'l') { //Check if argument is a link to an imageboard.
            chan = argv[++i];
        }
        else
            fuse_arg_strs[next_fuse_idx++] = argv[i];
    }

    if (chan == NULL) {
        fprintf(stderr, "No imageboard url was provided. Exiting.\n");
        return -1;
    }
    
    board_strs[next_chanfs_idx] = NULL;
    fuse_arg_strs[next_fuse_idx] = NULL;

    root = generate_fs(board_strs);
    int fuse_res = fuse_main(next_fuse_idx, fuse_arg_strs, &operations, NULL);

    curl_global_cleanup();


    return fuse_res;
}

/* Parse out the board name from the backslashes. */
static char* get_board_name(char *board_str) {
    char *end_slash_ptr = board_str + 1;
    char *start_slash_ptr = board_str + 1;

    for (; *end_slash_ptr != '/' ; end_slash_ptr++); 
    size_t size_of_name = end_slash_ptr - start_slash_ptr;

    char *board_name = (char *) malloc(size_of_name + 1);
    memcpy(board_name, start_slash_ptr, size_of_name);
    board_name[size_of_name] = '\0';

    return board_name;
}
