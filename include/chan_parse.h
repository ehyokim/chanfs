#include <cjson/cJSON.h>
#include <time.h>

/* According to the vichan API specs, a post can be from 1-9999999999999 */
typedef unsigned long long postno_t;

struct str_rep_buffer; 
typedef struct str_rep_buffer StrRepBuffer;

typedef struct Post {
    postno_t no;
    /* The post numbers which this post replies to */
    postno_t *replies_to;
    int num_replies_to;
    /* The post numbers which this post gets a reply from */
    postno_t *replies_from;
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
Thread parse_thread(char *board, postno_t thread_op_no);
AttachedFile download_file(char *board, char *filename);
int post_int_to_str(postno_t thread_no, char buffer[]); 
void free_board_parse_results(Board parse_res);
void free_post(Post post);
void free_thread_parse_results(Thread results);

