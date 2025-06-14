#include <sys/types.h>
#include "chan_parse.h"

#define FILEPERMS  S_IRWXU | S_IRWXG | S_IRWXO //TODO: fix these permissons. These are just a placeholder to make things work for now.
#define DIRPERMS  S_IRWXU | S_IRWXG | S_IRWXO

#define FILENAMELEN 20

typedef enum filetype {
    THREAD_OP_TEXT, POST_TEXT, ATTACHED_FILE
} Filetype;

typedef struct ChanFSObj {
    mode_t base_mode;
    mode_t mode;
    char *name;
    nlink_t nlink; 
    time_t time;
    uid_t uid;
    gid_t gid;
    void *obj;
} ChanFSObj;

typedef struct Chandir {
    ChanFSObj **children;
    int num_of_children_slots;
    int num_of_children;
} Chandir;

typedef struct Chanfile {
    int no; //Post number.
    int file_type;
    off_t size;
    ChanFSObj *curr_dir;
    Filetype type;
    char *contents;
} Chanfile;

ChanFSObj *generate_fs(char *board);



