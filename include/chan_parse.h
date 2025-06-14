#include <cjson/cJSON.h>
#include <time.h>

typedef struct Post {
    unsigned int no;
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
    Post **threads;
    int num_of_threads;
} Board;

typedef struct Thread {
    Post **posts;
    int num_of_replies;
} Thread;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static char *retrieve_webpage_text(char *url);
Board *parse_board(char *board);
Thread *parse_thread(char *board, unsigned int thread_op_no);
static int find_total_num_threads(cJSON *catalog);
static int find_total_num_replies(cJSON *thread);
static char *constr_catalog_url(char *chan_url, char *board);
static char *constr_thread_url(char *chan_url, char *board, unsigned int thread_op_no); 
void free_board_parse_results(Board *parse_res);
void free_post(Post *post);