#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "include/fs_utils.h"

#define INIT_NUM_CHILD_SLOTS 50

static char *truncate_name(char *name);
static void add_child(ChanFSObj *dir, ChanFSObj *child);
static char *set_thread_dir_name(Post *thread_op);
static ChanFSObj *init_dir(char *name, ChanFSObj *parent_dir, time_t time, Dirtype type, AssoInfo asso_info);
static ChanFSObj *init_file(char *name, ChanFSObj *curr_dir, time_t time, Filetype type, AssoInfo asso_info);
static void sanitize_name(char *name);
static StrRepBuffer generate_post_str_rep(Post *post);
static StrRepBuffer generate_thread_str_rep(Thread thread);
static void append_to_buffer_formatted(StrRepBuffer *str_buffer, char *str_formatter, char *str);
static void append_to_buffer(StrRepBuffer *str_buffer, char *str);
static char *generate_time_string(time_t t);
static void concat_str_rep_buffers(StrRepBuffer *s1, StrRepBuffer s2);
static void generate_thread_dir(ChanFSObj *thread_dir_object);
static void generate_post_dir(ChanFSObj *post_dir_object);
static void generate_board_dir(ChanFSObj *board_dir_object); 
static StrRepBuffer new_str_rep_buffer(void); 
static void write_to_file_from_buffer(ChanFSObj *file_obj, StrRepBuffer str_buffer);
static void write_to_file_from_attached_file(ChanFSObj *file_obj, AttachedFile attached_file);
static char *concat_tim_ext(Post *post);


ChanFSObj *generate_fs(char *board_strs[])
{
    ChanFSObj *root_fs_obj = init_dir("/", NULL, time(NULL), ROOT_DIR, (AssoInfo)((Post *) NULL));
    root_fs_obj->generated_flag = 1;
    
    while (*board_strs != NULL) {
        init_dir(*board_strs, root_fs_obj, time(NULL), BOARD_DIR, (AssoInfo) *board_strs);
        board_strs++;
    }

    return root_fs_obj;
}

static void generate_board_dir(ChanFSObj *board_dir_object) 
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

        if(thread_dir_name != NULL)            
            init_dir(thread_dir_name, board_dir_object, thread_op->timestamp, THREAD_DIR, (AssoInfo) thread_op);
    }
}

static void generate_thread_dir(ChanFSObj *thread_dir_object) 
{   
        Post* thread_op = thread_dir_object->asso_info.post;
        char *board = thread_op->board;

        Thread thread_replies = parse_thread(board, thread_op->no);
        int num_of_replies = thread_replies.num_of_replies;

        init_file("Thread.txt", thread_dir_object, thread_op->timestamp, THREAD_OP_TEXT, (AssoInfo) thread_replies);

        char *concat_name;
        if(thread_op->tim != NULL && thread_op->ext != NULL && (concat_name = concat_tim_ext(thread_op)) != NULL)
            init_file(concat_name, thread_dir_object, thread_op->timestamp, ATTACHED_FILE, thread_dir_object->asso_info);     

        Post *replies = thread_replies.posts;
        for (int k = 1; k < num_of_replies; k++) { //Skip first post, which is OP.
            Post *reply = replies + k;
            char *thread_no_str = thread_int_to_str(reply->no);

            if(thread_no_str != NULL)
                init_dir(thread_no_str, thread_dir_object, reply->timestamp, POST_DIR, (AssoInfo) reply); //Create post directory.
        }
}

static void generate_post_dir(ChanFSObj *post_dir_object)
{
    Post *post = post_dir_object->asso_info.post;
    init_file("Post.txt", post_dir_object, post->timestamp, POST_TEXT, post_dir_object->asso_info); //Add Post text to each reply directory.

    char *concat_name;
    if (post->tim != NULL && post->ext != NULL && (concat_name = concat_tim_ext(post)) != NULL)  {
        init_file(concat_name, post_dir_object, post->timestamp, ATTACHED_FILE, post_dir_object->asso_info); //Add Image file if it exists to each post directory.
    }    
}

void generate_file_contents(ChanFSObj *file_obj)
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
                Post *post = file_obj->asso_info.post;
                AttachedFile attached_file = download_file(post->board, file_obj->name);
                write_to_file_from_attached_file(file_obj, attached_file);
                break;
            case ERROR_FILE:
                StrRepBuffer err_str_buffer = new_str_rep_buffer();
                append_to_buffer_formatted(&err_str_buffer, "Error: Board %s has failed to load. Perhaps it doesn't exist?", file_obj->asso_info.board);
                write_to_file_from_buffer(file_obj, err_str_buffer);
                break;
        }
        file_obj->generated_flag = 1;
}

void generate_dir_contents(ChanFSObj *dir_obj)
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
            break;
            //Do nothing for now.
    }
    dir_obj->generated_flag = 1;    
}

void free_str_rep_buffer(StrRepBuffer str_buffer)
{
    free(str_buffer.buffer_start);
}

//This is bad since it generates the post string contents twice at most.
static StrRepBuffer generate_thread_str_rep(Thread thread) 
{
    Post *replies = thread.posts;
    StrRepBuffer thread_str_buffer = generate_post_str_rep(replies); //Begin with converting the OP post.

    if (thread_str_buffer.buffer_size == 0) {
        fprintf(stderr, "Error: Could not start thread string representation buffer.\n");
        return thread_str_buffer;
    } 

    StrRepBuffer reply_str_buffer;
    for (int i = 1; i < thread.num_of_replies; i++) {
        append_to_buffer(&thread_str_buffer, "\n\n---------------------------\n\n");
        reply_str_buffer = generate_post_str_rep(replies + i);
        concat_str_rep_buffers(&thread_str_buffer, reply_str_buffer);
        free_str_rep_buffer(reply_str_buffer);
    }

    return thread_str_buffer;
}

static StrRepBuffer generate_post_str_rep(Post *post) 
{
    StrRepBuffer buffer = new_str_rep_buffer();
    if (buffer.buffer_size == 0) {
        fprintf(stderr, "Error; Could not allocate memory to start a new StrRepBuffer.\n");
        return buffer;
    }

    append_to_buffer_formatted(&buffer, "Board: /%s/\n", post->board);

    char *thread_no_str = thread_int_to_str(post->no); 
    if (thread_no_str != NULL)
        append_to_buffer_formatted(&buffer, "No: %s\n", thread_no_str);

    append_to_buffer_formatted(&buffer, "Name: %s\n", post->name);
    append_to_buffer_formatted(&buffer, "Trip: %s\n", post->trip);
    append_to_buffer_formatted(&buffer, "Email: %s\n", post->email);
 
    char *time_str = generate_time_string(post->timestamp);
    if (time_str != NULL)
        append_to_buffer_formatted(&buffer, "Time: %s\n", time_str);

    append_to_buffer_formatted(&buffer, "Subject: %s\n", post->sub);
    append_to_buffer_formatted(&buffer, "\n\n %s\n", post->com);

    free(thread_no_str);
    free(time_str);

    return buffer;
}

static StrRepBuffer new_str_rep_buffer(void) 
{
    int buffer_size = 100;
    char *buffer_start = malloc(buffer_size);

    if (!buffer_start) {
        buffer_start = "";
        buffer_size = 0;
    }

    char *str_end = buffer_start;
    int curr_str_size = 0;

    return (StrRepBuffer) {buffer_size, curr_str_size, buffer_start, str_end};

}

static void write_to_file_from_attached_file(ChanFSObj *file_obj, AttachedFile attached_file)
{
    Chanfile *file = (Chanfile *) &(file_obj->fs_obj);    
    file->contents = attached_file.file;
    file->size = attached_file.size;
}

static void write_to_file_from_buffer(ChanFSObj *file_obj, StrRepBuffer str_buffer) {
    Chanfile *file = (Chanfile *) &(file_obj->fs_obj);    
    file->contents = str_buffer.buffer_start;
    file->size = str_buffer.curr_str_size;
}

static char *concat_tim_ext(Post *post)
{
    char *concat_file_str = malloc(strlen(post->tim) + strlen(post->ext) + 1);
    if (!concat_file_str) {
        fprintf(stderr, "Error: Could not concatenate tim and ext strings as memory could not be allocated.\n");
        return NULL;
    }
    strcpy(concat_file_str, post->tim);
    strcat(concat_file_str, post->ext);

    return concat_file_str;
}

static char *generate_time_string(time_t t) 
{
    char *buffer = malloc(30);
    if (!buffer) {
        fprintf(stderr, "Error: Could not allocate memory for time string.\n");
        return NULL;
    }

    struct tm *tm_st = localtime(&t);
    strftime(buffer, sizeof(buffer), "%x - %I:%M%p", tm_st);

    return buffer;
}

static void concat_str_rep_buffers(StrRepBuffer *s1, StrRepBuffer s2)
{
    if (s2.curr_str_size + s1->curr_str_size + 1 > s1->buffer_size) {
        int new_buffer_size = s1->buffer_size + (s2.curr_str_size + 1) + 100;
        
        char *new_ptr = realloc(s1->buffer_start, new_buffer_size);
        if (!new_ptr) {
            fprintf(stderr, "Error: Cannot reallocate new buffer or StrRepBuffer");
            return;            
        }

        s1->buffer_start = new_ptr;
        s1->str_end = s1->buffer_start + s1->curr_str_size;
        s1->buffer_size = new_buffer_size;
    }

    sprintf(s1->str_end, "%s", s2.buffer_start);
    s1->curr_str_size += s2.curr_str_size;
    s1->str_end += s2.curr_str_size;
}

static void append_to_buffer(StrRepBuffer *str_buffer, char *str)
{
    append_to_buffer_formatted(str_buffer, "%s", str);
}

static void append_to_buffer_formatted(StrRepBuffer *str_buffer, char *str_formatter, char *str) 
{
    if(!str) return;

    int size_of_str = snprintf(NULL, 0, str_formatter, str);

    if (str_buffer->curr_str_size + size_of_str + 1 > str_buffer->buffer_size) {
        int new_buffer_size = str_buffer->buffer_size + (size_of_str + 1) + 100;

        char *new_ptr = realloc(str_buffer->buffer_start, new_buffer_size);
        if (!new_ptr) {
            fprintf(stderr, "Error: Cannot reallocate new buffer or StrRepBuffer");
            return;
        }

        str_buffer->buffer_start = new_ptr;
        str_buffer->str_end = str_buffer->buffer_start + str_buffer->curr_str_size;
        str_buffer->buffer_size = new_buffer_size;
    }

    sprintf(str_buffer->str_end, str_formatter, str);
    str_buffer->str_end += size_of_str;
    str_buffer->curr_str_size += size_of_str;
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
    char *trun_title_buffer = malloc(FILENAMELEN + 1);
    if (!trun_title_buffer) {
        fprintf(stderr, "Error: Could not allocate buffer for truncated title for %s\n", name);
        return NULL;
    }

    memcpy(trun_title_buffer, name, FILENAMELEN);
    trun_title_buffer[FILENAMELEN] = '\0';

    return trun_title_buffer;
}

/*The issue with this is that we lose all of the backslash information. */
static void sanitize_name(char *name) 
{
    if (!name) return;

    for (; *name != '\0'; name++)
        *name = (*name == '/') ? '#' : *name;
}

static void add_child(ChanFSObj *dir_fs_object, ChanFSObj *child)
{   
    Chandir *dir = (Chandir *) &(dir_fs_object->fs_obj);

    if (dir->num_of_children >= dir->num_of_children_slots) {
        int new_num_of_children_slots = dir->num_of_children_slots + 50;
        ChanFSObj **obj_ptr = realloc(dir->children, new_num_of_children_slots * sizeof(ChanFSObj *));

        if (!obj_ptr) {
            fprintf(stderr, "Error: Memory reallocation failed for procedure to expand children slot for directory object.\n");
            return;
        }

        dir->children = obj_ptr;
        dir->num_of_children_slots = new_num_of_children_slots;
    }

    *(dir->children + dir->num_of_children) = child;
    dir->num_of_children++;
}
    
static ChanFSObj *init_dir(char *name, ChanFSObj *parent_dir, time_t time, Dirtype type, AssoInfo asso_info)
{
    ChanFSObj *new_fs_obj = malloc(sizeof(ChanFSObj));
    if(!new_fs_obj) {
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
    
    if(parent_dir != NULL) {
        if (parent_dir->base_mode != S_IFDIR) {
            fprintf(stderr, "Error: Attempting to add child FS object \"%s\" to a file or other.\n", name);
            goto add_to_file_fail;
        }
        else 
            add_child(parent_dir, new_fs_obj);
    }

    return new_fs_obj;

add_to_file_fail:
    free(children);
allo_child_fail:
    free(new_fs_obj);
allo_fs_obj_fail:
    return NULL;
}

static ChanFSObj *init_file(char *name, ChanFSObj *curr_dir, time_t time, Filetype type, AssoInfo asso_info)
{
    ChanFSObj *new_fs_obj = malloc(sizeof(ChanFSObj));
    if(!new_fs_obj) {
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

    Chanfile new_chanfile = {0, curr_dir, type, {}};
    new_fs_obj->fs_obj = (FSObj) new_chanfile;

    if (curr_dir->base_mode != S_IFDIR) {
        fprintf(stderr, "Error: Attempting to add child FS object \"%s\" to a file or other.\n", name);
        goto add_to_file_fail;
    }
    else 
        add_child(curr_dir, new_fs_obj);

    return new_fs_obj;

add_to_file_fail:
    free(new_fs_obj);
allo_fs_obj_fail:
    return NULL;
}


