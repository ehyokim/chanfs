#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fuse.h>
#include <errno.h>

#include "include/consts.h"
#include "include/chan_parse.h"
#include "include/fs_utils.h"
#include "include/fs.h"

extern ChanFSObj *root;

static ChanFSObj *traverse(const char *path);

/* The FUSE getattr function */
int 
do_getattr(const char *path, struct stat *st)
{
    printf("Querying: %s\n", path);
    ChanFSObj *found_obj = traverse(path);
    if (!found_obj) {
        return -ENOENT;
    }
    if (found_obj->base_mode == S_IFREG) {
        Chanfile file = found_obj->fs_obj.chanfile;
        /* Generate the contents of files as needed. */
        if (!(found_obj->generated_flag)) {
            generate_file_contents(found_obj);
        }
        st->st_size = file.size;
    } else if (found_obj->base_mode != S_IFDIR) {
        return -ENOENT;
    }

    st->st_mode = found_obj->mode;
    st->st_nlink = found_obj->nlink;
    st->st_uid = found_obj->uid;
    st->st_gid = found_obj->gid;
    st->st_atime = found_obj->time;
    st->st_mtime = found_obj->time;

    return 0;
}

/* The FUSE readdir function */
int 
do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("Reading directory: %s \n", path);
    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);

    ChanFSObj *found_obj = traverse(path); //Traverse the FS to find the directory.
    if (!found_obj) {
        return -ENOENT;
    }

    /* Generate the directory if it hasn't been already from a previous query. */
    if (!(found_obj->generated_flag)) {
        generate_dir_contents(found_obj);
    } 

    Chandir dir = found_obj->fs_obj.chandir;

    /* Iterate through all of the children (files and directories) found under this directory. */
    int num_of_children = dir.num_of_children;
    ChanFSObj **children = dir.children;

    for (int i = 0; i < num_of_children; i++) {
        if(filler(buffer, children[i]->name, NULL, 0))
            break;
    }

    return 0;
}

/* The FUSE read function */
int 
do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("Readfile called with path: %s\n", path);

    ChanFSObj *found_obj = traverse(path);
    if (!found_obj) {
        return -ENOENT;
    }

    if(found_obj->base_mode == S_IFDIR) {
        return -EISDIR;
    }

    /* Access the file contents and copy them onto the buffer. */
    //if (!(found_obj->generated_flag)) {
    //    generate_file_contents(found_obj);
    //}

    Chanfile file = found_obj->fs_obj.chanfile;
    off_t file_size = file.size;
    char *contents = file.contents;

    if (!contents) {
        fprintf(stderr, "Error: file contents are null. Cannot read file.\n");
        return -EIO;
    }

    if (offset > file_size) {
        return -EINVAL;
    }

    size_t bytes_read = (offset + size > file_size) ? file_size - offset : size;
    memcpy(buffer, contents + offset, bytes_read);
    
    return bytes_read;
}

/* Function to traverse the FS according to a supplied path. */
static ChanFSObj *
traverse(const char *path)
{
    char *pathcpy = strdup(path);
    if (!pathcpy) {
        fprintf(stderr, "Error: Could not allocate memory for traversal operation.\n");
        return NULL;
    }

    /* Tokenize the path and follow each step of the way down. */
    char *token = strtok(pathcpy, "/");
    ChanFSObj *traverse_ptr = root;

    int query_successful = 1;
    while (token) {
        if (traverse_ptr->base_mode != S_IFDIR) {
            fprintf(stderr, "Error: attempting to traverse over a file or other.\n");
            query_successful = 0;
            break;
        }

        Chandir curr_dir = traverse_ptr->fs_obj.chandir;
        int num_of_children = curr_dir.num_of_children;
        ChanFSObj **children = curr_dir.children;

        ChanFSObj *found_obj = NULL;
        for(int i = 0; i < num_of_children; i++) {
            if (strcmp(children[i]->name, token) == 0) { //TODO: Use a tree-search at this point.
                found_obj = traverse_ptr = children[i];
                break; 
            }
        }

        if (!found_obj) {
            fprintf(stderr, 
                    "Error: no such file or directory with the name: %s was found.\n", 
                    token);
            query_successful = 0;
            break;
        }

        token = strtok(NULL, "/");
    }

    free(pathcpy);
    return (query_successful) ? traverse_ptr : NULL;
}

