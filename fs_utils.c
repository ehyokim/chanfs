#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "include/fs_utils.h"

static char *truncate_name(char *name);
static void add_child(Chandir *dir, ChanFSObj *child);
static char *set_thread_dir_name(Post *thread_op);
static ChanFSObj *init_dir(char *name, ChanFSObj *parent_dir, time_t time);
static ChanFSObj *init_file(char *name, off_t size, ChanFSObj *curr_dir, time_t time, Filetype type);
static void sanitize_name(char *name);

ChanFSObj *generate_fs(char *board)
{
    Board *results = parse_board(board);
    ChanFSObj *root_fs_obj = init_dir("/", NULL, time(NULL));
    
    for (int i = 0; i < results->num_of_threads; i++) {
        Post *thread = results->threads[i];
        char *thread_dir_name = set_thread_dir_name(thread);            
        init_dir(thread_dir_name, root_fs_obj, thread->timestamp);
    }

    Chandir *root_dir = root_fs_obj->obj;
    ChanFSObj **thread_dir_list = root_dir->children;

    for (int j = 0; j < results->num_of_threads; j++) {
        Post *thread_op = results->threads[j];
        ChanFSObj *thread_dir_obj = thread_dir_list[j];

        init_file("Thread.txt", 0, thread_dir_obj, thread_op->timestamp, THREAD_OP_TEXT);
        init_file("File", 0, thread_dir_obj, thread_op->timestamp, ATTACHED_FILE); //Picture and file info.

        int thread_op_num = thread_op->no;
        Thread *thread_replies = parse_thread(board, thread_op_num);
        int num_of_replies = thread_replies->num_of_replies;

        Post **replies = thread_replies->posts;
        for (int k = 1; k < num_of_replies; k++) { //Skip first post, which is OP.
            Post *reply = replies[k];
            
            char *thread_no_str = thread_uint_to_str(reply->no);

            ChanFSObj *post_fs_obj = init_dir(thread_no_str, thread_dir_obj, reply->timestamp); //Create post directory.
            init_file("Post.txt", 0, post_fs_obj, reply->timestamp, POST_TEXT); //Add Post text to each reply directory.

            if (reply->tim != NULL && reply->ext != NULL)  {
                char *concat_file_str = (char *) malloc(strlen(reply->tim) + strlen(reply->ext) + 1);
                strcpy(concat_file_str, reply->tim);
                strcat(concat_file_str, reply->ext);
                init_file(concat_file_str, 0, thread_dir_obj, reply->timestamp, ATTACHED_FILE); //Add Image file if it exists to each post directory.
            }
        }
        
        free_thread_parse_results(thread_replies);
    }

    free_board_parse_results(results);
    return root_fs_obj;

}

static char *set_thread_dir_name(Post *thread_op)
{   
    char *untreated_dir_name;

    if ((untreated_dir_name = thread_op->sub) != NULL);
    else if((untreated_dir_name = thread_op->com) != NULL);
    else
        untreated_dir_name = "No Subject";
    
    char * treated_dir_name;
    sanitize_name(treated_dir_name = truncate_name(untreated_dir_name));
    return treated_dir_name;
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
    if (dir->num_of_children >= dir->num_of_children_slots) {
        dir->num_of_children_slots += 50;
        dir->children = (ChanFSObj **) realloc(dir->children, dir->num_of_children_slots * sizeof(ChanFSObj *));
    }

    *(dir->children + dir->num_of_children) = child;
    dir->num_of_children++;
}
    
//Add child and initialization can be merged.       
static ChanFSObj *init_dir(char *name, ChanFSObj *parent_dir, time_t time)
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
    dir->num_of_children_slots = 50;
    dir->children = (ChanFSObj **) malloc(50 * sizeof(ChanFSObj *)); //We work in increments of 50 objects. 
    dir->num_of_children = 0;
    
    if(parent_dir != NULL) {
        if (parent_dir->base_mode != S_IFDIR)
            fprintf(stderr, "Error: Attempting to add child FS object \"%s\" to a file or other.\n", name);
        else 
            add_child((Chandir *)parent_dir->obj, new_fs_obj);
    }

    return new_fs_obj;
}

static ChanFSObj *init_file(char *name, off_t size, ChanFSObj *curr_dir, time_t time, Filetype type)
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

    file->contents = "";

    file->size = size;
    file->curr_dir = curr_dir;
    file->type = type;

    if (curr_dir->base_mode != S_IFDIR)
        fprintf(stderr, "Error: Attempting to add child FS object \"%s\" to a file or other.\n", name);
    else 
        add_child((Chandir *)curr_dir->obj, new_fs_obj);

    return new_fs_obj;
}


