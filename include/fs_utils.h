#include <sys/types.h>

#define FILEPERMS  S_IRWXU | S_IRWXG | S_IRWXO //TODO: fix these permissons. These are just a placeholder to make things work for now.
#define DIRPERMS  S_IRWXU | S_IRWXG | S_IRWXO

#define FILENAMELEN 20

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
    int num_of_children;
    int next_free_child;
} Chandir;

typedef struct Chanfile {
    unsigned int no; //Post number.
    int file_type;
    off_t size;
    ChanFSObj *curr_dir;
} Chanfile;


ChanFSObj *generate_fs(void);
static char *truncate_name(char *name);
static void add_child(Chandir *dir, ChanFSObj *child);
static ChanFSObj *init_dir(char *name, ChanFSObj *parent_dir, int num_of_children, time_t time);
static ChanFSObj *init_file(char *name, off_t size, ChanFSObj *curr_dir, time_t time);
static void sanitize_name(char *name);


