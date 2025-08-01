#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include/consts.h"
#include "include/chan_parse.h"
#include "include/textproc.h"
#include "include/fs_utils.h"

static void write_to_file_from_buffer(ChanFSObj *file_obj, StrRepBuffer str_buffer);
static void write_to_file_from_attached_file(ChanFSObj *file_obj, AttachedFile attached_file);
static char *truncate_name(char *name);
static int add_child(ChanFSObj *dir, ChanFSObj *child);
static char *set_thread_dir_name(Post *thread_op);
static ChanFSObj *init_dir(char *name, ChanFSObj *parent_dir, time_t time, Dirtype type, AssoInfo asso_info);
static ChanFSObj *init_file(char *name, ChanFSObj *curr_dir, time_t time, Filetype type, AssoInfo asso_info);
static void sanitize_name(char *name);
static void generate_thread_dir(ChanFSObj *thread_dir_object);
static void generate_post_dir(ChanFSObj *post_dir_object);
static void generate_board_dir(ChanFSObj *board_dir_object); 
static AttachedFile download_attached_file(Post *post);

ChanFSObj *root;

/* Generates the initial root directory and instantiates the FS. */
void 
generate_fs(char *board_strs[])
{
    root = init_dir("/", NULL, time(NULL), ROOT_DIR, (AssoInfo)((Post *) NULL));
    if (!root) {
        fprintf(stderr, "Error: Could not allocate memory for root directory FS object.\n");
        /* If we can't even allocate memory for the root object, then just abort the whole thing */
        exit(-1);
    }
    root->generated_flag = 1;
    
    /* Generate all of the board directories housing all of the threads. */
    while (*board_strs) {
        ChanFSObj *board_dir_obj = init_dir(*board_strs, root, time(NULL), BOARD_DIR, (AssoInfo) *board_strs);
        if (!board_dir_obj) {
            fprintf(stderr, "Error: Could not allocate memory for board directory FS object.\n");
        }
        board_strs++;
    }
}

/* Given a dir FS object corresponding to a board directory, generate all of the directories 
   corresponding to threads living within that board. */
static void 
generate_board_dir(ChanFSObj *board_dir_object) 
{
    char *board = board_dir_object->asso_info.board;
    Board results = parse_board(board);

    /* Write to an error file in the board directory if parsing goes wrong. */
    if (!results.threads) {
        fprintf(stderr, "Error: Board /%s/ could not be read.\n", board);
        init_file("Error.txt", board_dir_object, time(NULL), ERROR_FILE, board_dir_object->asso_info);
    }
    
    for (int i = 0; i < results.num_of_threads; i++) {
        Post *thread_op = results.threads + i;
        char *thread_dir_name = set_thread_dir_name(thread_op);

        if(thread_dir_name) {         
            init_dir(thread_dir_name, board_dir_object, thread_op->timestamp, THREAD_DIR, (AssoInfo) thread_op);
        }
    }
}

/* Given a dir FS object corresponding to a thread directory, generate all of post directories and the thread.txt */
static void 
generate_thread_dir(ChanFSObj *thread_dir_object) 
{   
        Post* thread_op = thread_dir_object->asso_info.post;
        char *board = thread_op->board;

        Thread thread_replies = parse_thread(board, thread_op->no);
        parse_html_for_thread(thread_replies);

        int num_of_posts = thread_replies.num_of_posts;

        init_file("Thread.txt", thread_dir_object, thread_op->timestamp, THREAD_OP_TEXT, (AssoInfo) thread_replies); //Add thread.txt

        /* Create a FS object for the attached file if one exists */
        char *concat_name = concat_filename_ext(thread_op, ORIGINAL);
        if (concat_name) {
            init_file(concat_name, thread_dir_object, thread_op->timestamp, ATTACHED_FILE, thread_dir_object->asso_info);     
        }

        Post *replies = thread_replies.posts;
        for (int k = 1; k < num_of_posts; k++) { //Skip first post, which is OP.
            Post *reply = replies + k;
            char *post_no_str = malloc(MAX_POST_NO_DIGITS + 1);
            int intres_res = post_no_to_str(reply->no, post_no_str);

            if (intres_res >= 0) {
                init_dir(post_no_str, thread_dir_object, reply->timestamp, POST_DIR, (AssoInfo) reply); //Create post directory
            }
        }
}

/* Generate the post directory including its post.txt and any attached files. */
static void 
generate_post_dir(ChanFSObj *post_dir_object)
{
    Post *post = post_dir_object->asso_info.post;
    init_file("Post.txt", post_dir_object, post->timestamp, POST_TEXT, post_dir_object->asso_info); //Add Post text to each reply directory.

    char *concat_name = concat_filename_ext(post, ORIGINAL);
    if (concat_name) {
        init_file(concat_name, post_dir_object, post->timestamp, ATTACHED_FILE, post_dir_object->asso_info); //Add Image file if it exists to each post directory.   
    }
}

/* Generate the contents of a given file FS object to be handed over to FUSE. */
void 
generate_file_contents(ChanFSObj *file_obj)
{       
        Chanfile file = file_obj->fs_obj.chanfile;
        Filetype file_type = file.type;

        StrRepBuffer str_buffer;
        switch (file_type) {
            case POST_TEXT:
                str_buffer = generate_post_str_rep(file_obj->asso_info.post); 
                write_to_file_from_buffer(file_obj, str_buffer);
                break;
            case THREAD_OP_TEXT:
                str_buffer = generate_thread_str_rep(file_obj->asso_info.thread);
                write_to_file_from_buffer(file_obj, str_buffer);
                break;
            case ATTACHED_FILE:
                AttachedFile attached_file = download_attached_file(file_obj->asso_info.post);
                write_to_file_from_attached_file(file_obj, attached_file);
                break;
            case ERROR_FILE:
                str_buffer = new_error_buffer(file_obj->asso_info.board);
                write_to_file_from_buffer(file_obj, str_buffer);
                break;
        }
        file_obj->generated_flag = 1;
}

static AttachedFile 
download_attached_file(Post *post) 
{
    char *concat_name = concat_filename_ext(post, RENAMED);
    if (!concat_name) {
        return (AttachedFile) {NULL, 0};
    }

    AttachedFile dld_file = download_file(post->board, concat_name);
    free(concat_name);

    return dld_file;
}

/* Generate the contents of a given directory FS object to be handed over to FUSE. */
void 
generate_dir_contents(ChanFSObj *dir_obj)
{
    Chandir dir = dir_obj->fs_obj.chandir;    
    Dirtype dir_type = dir.type;

    switch (dir_type) {
        case THREAD_DIR:
            generate_thread_dir(dir_obj);
            break;
        case POST_DIR:
            generate_post_dir(dir_obj);
            break;
        case BOARD_DIR:
            generate_board_dir(dir_obj);
            break;
        case ROOT_DIR:
        default:
            break;
    }
    dir_obj->generated_flag = 1;    
}

static void 
write_to_file_from_attached_file(ChanFSObj *file_obj, AttachedFile attached_file)
{
    Chanfile *file = (Chanfile *) &(file_obj->fs_obj);    
    file->contents = attached_file.file;
    file->size = attached_file.size;
}

/* A simple function that just sets a file FS object's contents to some aggregate string representation buffer. */
static void 
write_to_file_from_buffer(ChanFSObj *file_obj, StrRepBuffer str_buffer) 
{
    Chanfile *file = (Chanfile *) &(file_obj->fs_obj);    
    file->contents = str_buffer.buffer_start;
    file->size = str_buffer.curr_str_size;
}


/*  Sets the name of directories containing thread information. */
static char *
set_thread_dir_name(Post *thread_op)
{   
    char *untreated_dir_name;

    if (untreated_dir_name = thread_op->sub);
    else if(untreated_dir_name = thread_op->com);
    else {
        untreated_dir_name = "No Subject";
    }
    
    char *treated_dir_name = truncate_name(untreated_dir_name);
    sanitize_name(treated_dir_name);

    return treated_dir_name;
}

/* The strings used for titles of files and directories are truncated down to size 
 * to encourage readability and to reduce clutter. 
 */
static char *
truncate_name(char *name) 
{
    char *trun_title_buffer = malloc(MAX_FILENAME_LEN + 1);
    if (!trun_title_buffer) {
        fprintf(stderr, "Error: Could not allocate buffer for truncated title for %s\n", name);
        return NULL;
    }

    strncpy(trun_title_buffer, name, MAX_FILENAME_LEN);
    trun_title_buffer[MAX_FILENAME_LEN] = 0;

    return trun_title_buffer;
}

/* Used to replace any backslashes found in the filenames with a '#' character.
 * This is a hacky way to ensure that the filenames do not interfere with the absolute pathname. 
 * The issue with this is that we lose all of the backslash information. 
 */
static void 
sanitize_name(char *name) 
{
    if (!name) return;
    for (; *name != '\0'; name++) {
        switch (*name) {
            case '/':
                *name = '#';
                break;
            case ' ':
                *name = '_';
                break;
            default:
                break;
        }
    }
}

/* Adding any child file or directory underneath a supplied directory FS object. */
static int 
add_child(ChanFSObj *dir_fs_object, ChanFSObj *child)
{   
    Chandir *dir = (Chandir *) &(dir_fs_object->fs_obj);

    if (dir->num_of_children >= dir->num_of_children_slots) {
        int new_num_of_children_slots = dir->num_of_children_slots + INIT_NUM_CHILD_SLOTS;
        ChanFSObj **obj_ptr = realloc(dir->children, new_num_of_children_slots * sizeof(ChanFSObj *));
        if (!obj_ptr) {
            fprintf(stderr, "Error: Memory reallocation failed for procedure to" 
                            "expand children slot for directory object.\n");
            return 0; // Do not add child if no memory can be allocated.
        }

        dir->children = obj_ptr;
        dir->num_of_children_slots = new_num_of_children_slots;
    }

  *(dir->children + dir->num_of_children) = child;
    dir->num_of_children++;
    return 1;
}

/* Directory FS objects are initialized and added to a parent directory. The appropriate attributes are also set */
static ChanFSObj *
init_dir(char *name, ChanFSObj *parent_dir, time_t time, Dirtype type, AssoInfo asso_info)
{
    ChanFSObj *new_fs_obj = malloc(sizeof(ChanFSObj));
    if (!new_fs_obj) {
        fprintf(stderr, "Error: Could not allocate FS object.\n");
        goto allo_fs_obj_fail;     
    } 

    new_fs_obj->base_mode = S_IFDIR;
    new_fs_obj->mode = new_fs_obj->base_mode | DIRPERMS;
    new_fs_obj->name = name;
    new_fs_obj->nlink = 2;
    new_fs_obj->time = time;
    new_fs_obj->uid = getuid();
    new_fs_obj->gid = getgid();
    new_fs_obj->generated_flag = 0;
    new_fs_obj->asso_info = asso_info;

    ChanFSObj **children = malloc(INIT_NUM_CHILD_SLOTS * sizeof(ChanFSObj *));
    if (!children) {
        fprintf(stderr, "Error: Could not allocate array of children ChanFSObj pointers.\n");
        goto allo_child_fail;
    }
    
    Chandir new_chandir = {children, INIT_NUM_CHILD_SLOTS, 0, type};
    new_fs_obj->fs_obj = (FSObj) new_chandir;
    
    if (parent_dir) {
        if (parent_dir->base_mode != S_IFDIR) {
            fprintf(stderr, "Error: Attempting to add child FS object \"%s\" to a file or other.\n", name);
            goto add_to_file_fail;
        }
        else {
            if(!add_child(parent_dir, new_fs_obj))
                goto add_to_file_fail;
        }
    }

    return new_fs_obj;

add_to_file_fail:
    free(children);
allo_child_fail:
    free(new_fs_obj);
allo_fs_obj_fail:
    return NULL;
}


/* File FS objects are initialized and added to a parent directory. The appropriate attributes are also set */
static ChanFSObj *
init_file(char *name, ChanFSObj *curr_dir, time_t time, Filetype type, AssoInfo asso_info)
{
    ChanFSObj *new_fs_obj = malloc(sizeof(ChanFSObj));
    if (!new_fs_obj) {
        fprintf(stderr, "Error: could not allocate FS file object.\n");
        goto allo_fs_obj_fail;        
    } 

    new_fs_obj->base_mode = S_IFREG;
    new_fs_obj->mode = new_fs_obj->base_mode | FILEPERMS;
    new_fs_obj->name = name;
    new_fs_obj->nlink = 1;
    new_fs_obj->time = time;
    new_fs_obj->uid = getuid();
    new_fs_obj->gid = getgid();
    new_fs_obj->generated_flag = 0;
    new_fs_obj->asso_info = asso_info;

    Chanfile new_chanfile = {0, 
                            curr_dir, 
                            type, 
                            NULL};

    new_fs_obj->fs_obj = (FSObj) new_chanfile;

    if (curr_dir->base_mode != S_IFDIR) {
        fprintf(stderr, "Error: Attempting to add child FS object \"%s\" to a file or other.\n", name);
        goto add_to_file_fail;
    }
    else {
        if(!add_child(curr_dir, new_fs_obj))
            goto add_to_file_fail;
    }

    return new_fs_obj;

add_to_file_fail:
    free(new_fs_obj);
allo_fs_obj_fail:
    return NULL;
}


