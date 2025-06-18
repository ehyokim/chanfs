#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fuse.h>
#include <errno.h>

#include "include/fs_utils.h"
#include "include/fs.h"

static ChanFSObj *traverse(const char *path);
extern ChanFSObj *root;

int do_getattr(const char *path, struct stat *st)
{
    printf("Querying: %s\n", path);
    ChanFSObj *found_obj = traverse(path);
    if (!found_obj) 
        return -ENOENT;

    if (found_obj->base_mode == S_IFREG) {
        if (!(found_obj->generated_flag))
            generate_file_contents(found_obj);
            
        Chanfile obj = found_obj->fs_obj.chanfile;
        st->st_size = obj.size;
    } else if (found_obj->base_mode != S_IFDIR)
        return -ENOENT;

    st->st_mode = found_obj->mode;
    st->st_nlink = found_obj->nlink;
    st->st_uid = found_obj->uid;
    st->st_gid = found_obj->gid;
    st->st_atime = found_obj->time;
    st->st_mtime = found_obj->time;

    return 0;
}

int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("Reading directory: %s \n", path);
    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);

    ChanFSObj *found_obj = traverse(path);
    if (!found_obj)
        return -ENOENT;

    if (!(found_obj->generated_flag)) //Generate the directory if it hasn't been already from a previous query.
        generate_dir_contents(found_obj);

    Chandir dir = found_obj->fs_obj.chandir;

    int num_of_children = dir.num_of_children;
    ChanFSObj **children = dir.children;

    for (int i = 0; i < num_of_children; i++) {
        if(filler(buffer, children[i]->name, NULL, 0))
            break;
    }

    return 0;
}
/* Idea: Maybe keep a hash table threads hashed or indiced by their post_no. then retriving any thread and its contents will be loaded into memory for the next time
         we need to access them. This could be useful for generating reply subdirectories dynamically without having to retrieve it every time.
*/
int do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("Readfile called with path: %s\n", path);

    ChanFSObj *found_obj = traverse(path);
    if (!found_obj)
        return -ENOENT;

    if(found_obj->base_mode == S_IFDIR)
        return -EISDIR;

    
    Chanfile file = found_obj->fs_obj.chanfile;
    off_t file_size = file.size;

    if (offset > file_size) 
        return -EINVAL;

    char *contents = file.contents.buffer_start;

    char *offsetted_str = contents + offset;
    size_t bytes_read;
    for (bytes_read = 0; bytes_read < size && (*buffer++ = *offsetted_str++); bytes_read++);

    return bytes_read; //Not exactly correct.
}

/* Since all the files are in the root directory, this very simple for now.*/
static ChanFSObj *traverse(const char *path)
{
    char *pathcpy = strdup(path);
    char *token = strtok(pathcpy, "/");
    
    ChanFSObj *traverse_ptr = root;

    while (token != NULL) {

        if (traverse_ptr->base_mode != S_IFDIR) {
            fprintf(stderr, "Error: attempting to traverse over a file or other.\n");
            return NULL;
        }

       Chandir curr_dir = traverse_ptr->fs_obj.chandir;

        int num_of_children = curr_dir.num_of_children;
        ChanFSObj **children = curr_dir.children;

        ChanFSObj *found_obj = NULL;
        int i;
        for(i = 0; i < num_of_children; i++) {
            if (strcmp(children[i]->name, token) == 0) { //TODO: Use a tree-search at this point.
                found_obj = traverse_ptr = children[i];
                break; 
            }
        }

        if (found_obj == NULL) {
            //fprintf(stderr, "Error: no such file or directory with the name: %s was found.\n", token);
            return NULL;
        }

        token = strtok(NULL, "/");
    }

    free(pathcpy);
    return traverse_ptr;
}

