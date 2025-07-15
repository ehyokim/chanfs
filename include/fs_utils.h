#include <sys/types.h>
#include "chan_parse.h"

#define FILEPERMS  S_IRUSR | S_IRGRP | S_IROTH //TODO: fix these permissons. These are just a placeholder to make things work for now.
#define DIRPERMS  S_IRUSR | S_IRGRP | S_IROTH

#define FILENAMELEN 20

typedef enum filetype {
    THREAD_OP_TEXT, POST_TEXT, ATTACHED_FILE, ERROR_FILE
} Filetype;

typedef enum dirtype {
    ROOT_DIR, BOARD_DIR, THREAD_DIR, POST_DIR
} Dirtype;

typedef struct str_rep_buffer {
    int buffer_size;
    int curr_str_size;
    char *buffer_start;
    char *str_end;
} StrRepBuffer;

struct ChanFSObj;
typedef struct ChanFSObj ChanFSObj;

typedef struct Chandir {
    ChanFSObj **children;
    int num_of_children_slots;
    int num_of_children;
    Dirtype type;
} Chandir;

typedef struct Chanfile {
    off_t size;
    ChanFSObj *curr_dir;
    Filetype type;
    char *contents;
} Chanfile;

typedef union asso_info {
    Post *post; 
    Thread thread;
    char *board;
} AssoInfo;

typedef union fs_obj {
    Chandir chandir;
    Chanfile chanfile;
} FSObj;

struct ChanFSObj {
    mode_t base_mode;
    mode_t mode;
    char *name;
    nlink_t nlink; 
    time_t time;
    uid_t uid;
    gid_t gid;
    int generated_flag;
    AssoInfo asso_info;
    FSObj fs_obj;
};


void generate_fs(char *board_strs[]);
void generate_file_contents(ChanFSObj *file_obj);
void generate_dir_contents(ChanFSObj *dir_obj);
void free_str_rep_buffer(StrRepBuffer str_buffer);

 



