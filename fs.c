#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fuse.h>
#include <errno.h>

#include "include/fs.h"

extern ChanFSObj *root;

int do_getattr(const char *path, struct stat *st)
{

    //printf("getattr called with path: %s\n", path);

    ChanFSObj *fs_obj = traverse(path);
    if (!fs_obj) 
        return -ENOENT;


    if (fs_obj->base_mode == S_IFREG) {
        Chanfile *obj = (Chanfile *)fs_obj->obj;
        st->st_size = obj->size;
    } else if (fs_obj->base_mode != S_IFDIR)
        return -ENOENT;

    st->st_mode = fs_obj->mode;
    st->st_nlink = fs_obj->nlink;
    st->st_uid = fs_obj->uid;
    st->st_gid = fs_obj->gid;
    st->st_atime = fs_obj->time;
    st->st_mtime = fs_obj->time;

    return 0;
}

int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    //printf("readdir called with path: %s\n", path);

    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);

    ChanFSObj *fs_obj = traverse(path);
    if (!fs_obj)
        return -ENOENT;

    Chandir *dir = (Chandir *) fs_obj->obj;
    int num_of_children = dir->num_of_children;
    ChanFSObj **children = dir->children;

    for (int i = 0; i < num_of_children; i++) {
        if(filler(buffer, children[i]->name, NULL, 0))
            break;
    }

    return 0;
}

int do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    //printf("readfile called with path: %s\n", path);

    ChanFSObj *fs_obj = traverse(path);
    if (!fs_obj)
        return -ENOENT;

    if(fs_obj->base_mode == S_IFDIR)
        return -EISDIR;

    Chanfile *file = (Chanfile *)fs_obj->obj;
    char *contents = file->contents;
    off_t file_size = file->size;

    if (offset > file_size) 
        return -EINVAL;

    char *offsetted_str = contents + offset;
    size_t bytes_read;
    for (bytes_read = 0; bytes_read < size && (*buffer++ = *offsetted_str++); bytes_read++);

    return bytes_read; //Not exactly correct.
}

/* Since all the files are in the root directory, this very simple for now.*/
static ChanFSObj *traverse(const char *path)
{

    char *pathcpy = strdup(path);
    char *token = strtok(pathcpy, "/"); //Going to be \0 at first. 
    
    ChanFSObj *traverse_ptr = root;

    while (token != NULL) {

        //printf("Parsing token: %s \n", token);

        if (traverse_ptr->base_mode == S_IFREG) {
            fprintf(stderr, "Error: attempting to traverse over a file.\n");
            return NULL;
        }

        Chandir *curr_dir = (Chandir *)traverse_ptr->obj;

        int num_of_children = curr_dir->num_of_children;
        ChanFSObj **children = curr_dir->children;

        int i;
        for(i = 0; i < num_of_children; i++) {
            if (strcmp(children[i]->name, token) == 0) {
                traverse_ptr = children[i];
                break; 
            }
        }

        if (i == num_of_children) {
            fprintf(stderr, "Error: no such file or directory with the name: %s was found.\n", token);
            return NULL;
        }

        token = strtok(NULL, "/");
    }

    free(pathcpy);
    return traverse_ptr;
}

