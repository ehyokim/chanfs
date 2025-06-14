#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "include/fs_utils.h"
#include "include/chan_parse.h"

ChanFSObj *generate_fs()
{
    Board *results = parse_board("lit");
    ChanFSObj *root_fs_obj = init_dir("/", NULL, results->num_of_threads, time(NULL));
    
    Chandir *root_dir = root_fs_obj->obj;
    for (int i = 0; i < results->num_of_threads; i++) {
        Post *thread = results->threads[i];
        char *title = truncate_name(thread->sub);
        sanitize_name(title);
        add_child(root_dir, init_dir(title, root_fs_obj, 2, thread->timestamp));
    }

    ChanFSObj **thread_dir_list = root_dir->children;
    for (int j = 0; j < results->num_of_threads; j++) {
        Post *thread = results->threads[j];
        Chandir *thread_dir = thread_dir_list[j]->obj;
        add_child(thread_dir, init_file("Thread.txt", 0, thread_dir_list[j], thread->timestamp));
        add_child(thread_dir, init_file("File", 0, thread_dir_list[j], thread->timestamp)); //Picture and file info.
    }

    free_board_parse_results(results);
    return root_fs_obj;

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
    
       
static ChanFSObj *init_dir(char *name, ChanFSObj *parent_dir, int num_of_children, time_t time)
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
    new_fs_obj->time = time;
    new_fs_obj->uid = getuid();
    new_fs_obj->gid = getgid();
    dir->num_of_children = num_of_children;
    dir->children = (ChanFSObj **) malloc(num_of_children * sizeof(ChanFSObj *));
    dir->next_free_child = 0;

    return new_fs_obj;
}

static ChanFSObj *init_file(char *name, off_t size, ChanFSObj *curr_dir, time_t time)
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
    new_fs_obj->time = time;
    new_fs_obj->uid = getuid();
    new_fs_obj->gid = getgid();

    file->size = size;
    file->curr_dir = curr_dir;

    return new_fs_obj;
}


