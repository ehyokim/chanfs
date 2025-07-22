#include <cjson/cJSON.h>
#include <time.h>

struct str_rep_buffer; 
typedef struct str_rep_buffer StrRepBuffer;

typedef struct Post {
    int no;
    /* The post numbers which this post replies to */
    int *replies_to;
    int num_replies_to;
    /* The post numbers which this post gets a reply from */
    int *replies_from;
    int num_replies_from;
    char *board;
    char *sub;
    char *com;
    char *parsed_com;
    char *name;
    char *tim;
    char *filename;
    char *ext;
    char *trip;
    char *email;
    size_t filesize;
    time_t timestamp;
} Post;

typedef struct Board {
    Post *threads;
    int num_of_threads;
} Board;

typedef struct Thread {
    Post *posts;
    int num_of_posts; //Change this to num_of_posts
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

