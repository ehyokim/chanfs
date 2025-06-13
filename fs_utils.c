#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "include/fs_utils.h"

ChanFSObj *generate_fs()
{
    ParseResults *results = parse_chan();
    ChanFSObj *root = init_dir("/", NULL, results->num_of_threads);
    
    for (int i = 0; i < results->num_of_threads; i++) {
        char *title = truncate_name(results->thread_titles[i]);
        sanitize_name(title);
        add_child(root->obj, init_file(title, 1024, root));
    }

    free_parse_results(results);
    return root;

}

static char *truncate_name(char *name) 
{
    char *trun_title_buffer = (char *) malloc(FILENAMELEN + 1);
    memcpy(trun_title_buffer, name, FILENAMELEN);
    trun_title_buffer[FILENAMELEN] = '\0';

    return trun_title_buffer;
}

/*The issue with this is that we lose all of the backslash information. */
static void sanitize_name(char *name) 
{
    for (; *name != '\0'; name++)
        *name = (*name == '/') ? '#' : *name;
}

static void add_child(Chandir *dir, ChanFSObj *child)
{
    *(dir->children + dir->next_free_child) = child;
    dir->next_free_child++;
}
    
       
static ChanFSObj *init_dir(char *name, ChanFSObj *parent_dir, int num_of_children)
{

    ChanFSObj *new_fs_obj = (ChanFSObj *) malloc(sizeof(ChanFSObj));
    if(!new_fs_obj) {
        fprintf(stderr, "Error: could not allocate FS object.\n");
        return NULL;        
    } 

    Chandir *dir = (Chandir *) malloc(sizeof(Chandir));
    if (!dir) {
        fprintf(stderr, "Error: could not allocate directory of name: %s\n", name);
        return NULL;
    }

    new_fs_obj->base_mode = S_IFDIR;
    new_fs_obj->obj = dir;

    new_fs_obj->mode = new_fs_obj->base_mode | DIRPERMS;
    new_fs_obj->name = name;
    new_fs_obj->nlink = 2;
    new_fs_obj->time = time(NULL);
    new_fs_obj->uid = getuid();
    new_fs_obj->gid = getgid();
    dir->num_of_children = num_of_children;
    dir->children = (ChanFSObj **) malloc(num_of_children * sizeof(ChanFSObj *));
    dir->next_free_child = 0;

    return new_fs_obj;
}

static ChanFSObj *init_file(char *name, off_t size, ChanFSObj *curr_dir)
{

    ChanFSObj *new_fs_obj = (ChanFSObj *) malloc(sizeof(ChanFSObj));
    if(!new_fs_obj) {
        fprintf(stderr, "Error: could not allocate FS object.\n");
        return NULL;        
    } 

    Chanfile *file = (Chanfile *) malloc(sizeof(Chanfile));
    if (!file) {
        fprintf(stderr, "Error: could not allocate file of name: %s\n", name);
        return NULL;
    }

    new_fs_obj->base_mode = S_IFREG;
    new_fs_obj->obj = file;
    new_fs_obj->mode = new_fs_obj->base_mode | FILEPERMS;
    new_fs_obj->name = name;
    new_fs_obj->nlink = 1;
    new_fs_obj->time = time(NULL);
    new_fs_obj->uid = getuid();
    new_fs_obj->gid = getgid();

    file->size = size;
    file->curr_dir = curr_dir;
    file->contents = ""; //Just set files to be empty for now.

    return new_fs_obj;
}

static void free_parse_results(ParseResults *results)
{
    /* Free up parse results */
    for (int j = 0; j < results->num_of_threads; j++) {
        free(results->thread_titles[j]);
    }
    free(results->thread_titles);
    free(results);
}


