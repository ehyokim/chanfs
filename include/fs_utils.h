#include <sys/types.h>

/* For some reason, I need RWX user perms on MacOS. */
#ifdef __APPLE__
    #define FILEPERMS  S_IRWXU | S_IRGRP | S_IROTH
    #define DIRPERMS  S_IRWXU | S_IRGRP | S_IROTH
#else 
    #define FILEPERMS S_IRUSR | S_IRGRP | S_IROTH 
    #define DIRPERMS S_IRUSR | S_IRGRP | S_IROTH 
#endif

typedef enum file_type {
    THREAD_OP_TEXT, POST_TEXT, ATTACHED_FILE, ERROR_FILE
} Filetype;

typedef enum dir_type {
    ROOT_DIR, BOARD_DIR, THREAD_DIR, POST_DIR
} Dirtype;

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

 



