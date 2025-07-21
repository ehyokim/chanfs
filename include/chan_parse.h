#include <cjson/cJSON.h>
#include <time.h>

struct str_rep_buffer; 
typedef struct str_rep_buffer StrRepBuffer;

typedef struct Post {
    int no;
    char *board;
    char *sub;
    char *com;
    char *name;
    char *tim;
    char *filename;
    char *ext;
    char *trip;
    char *email;
    time_t timestamp;
} Post;

typedef struct Board {
    Post *threads;
    int num_of_threads;
} Board;

typedef struct Thread {
    Post *posts;
    int num_of_replies;
} Thread;

typedef struct AttachedFile {
    char *file;	
    size_t size;
} AttachedFile;

Board parse_board(char *board);
Thread parse_thread(char *board, int thread_op_no);
AttachedFile download_file(char *board, char *filename);
int post_int_to_str(int thread_no, char buffer[]); 
void free_board_parse_results(Board parse_res);
void free_post(Post post);
void free_thread_parse_results(Thread results);

