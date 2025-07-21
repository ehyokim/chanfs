#define FUSE_USE_VERSION 30

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fuse.h>
#include <curl/curl.h>

#include "include/chan_parse.h"
#include "include/fs_utils.h"
#include "include/fs.h"


static char* get_board_name(char *board_str);

/* */
char *chan;
int chan_str_len;

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .read = do_read,
    .readdir = do_readdir,
};

int 
main(int argc, char *argv[])
{   
    curl_global_init(CURL_GLOBAL_ALL);

    char *board_strs[argc-1];
    char *fuse_arg_strs[argc-1];
    int board_idx = 0, fuse_idx = 0;
    chan = NULL;

    /* Divide out the arguments into those that feed into chanfs and those that feed into fuse_main. 
       There are not many arguments to give as input here, so we chose to opt for a manual approach. 
    */
    int i, len;
    char *arg;
    for (i = 0; i < argc; i++) {
        arg = argv[i];

        if(arg[0] == '-') {
            switch (arg[1]) {
                case '/':
                    len = strlen(arg);
                    if (arg[len - 1] != '/') {
                        fprintf(stderr, "Error: malformed board argument. Exiting.");
                        exit(-1);
                    }
                    board_strs[board_idx++] = get_board_name(arg + 1);
                    break;
                case 'c':
                    chan = argv[++i];
		    chan_str_len = strlen(chan);
                    break;
                default:
                    fuse_arg_strs[fuse_idx++] = arg;
		    break;
            }
        } else 
            fuse_arg_strs[fuse_idx++] = arg;
    }         

    if (!chan) {
        fprintf(stderr, "Error: no imageboard url was provided. Exiting.\n");
        exit(0);
    }
    
    board_strs[board_idx] = NULL;
    fuse_arg_strs[fuse_idx] = NULL;

    /* Generate the FS from the supplied board names. */
    generate_fs(board_strs);

    int fuse_res = fuse_main(fuse_idx, fuse_arg_strs, &operations, NULL);

    curl_global_cleanup();
    return fuse_res;
}

/* Parse out the board name from the backslashes. */
static char *
get_board_name(char *board_str) {
    char *end_slash_ptr = board_str + 1;
    char *start_slash_ptr = board_str + 1;

    for (; *end_slash_ptr != '/' ; end_slash_ptr++); 
    
    *end_slash_ptr = '\0';
    return start_slash_ptr;
}
