#include <cjson/cJSON.h>

typedef struct ParseResults {
    char **thread_titles;
    int num_of_threads;
} ParseResults;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static char *retrieve_webpage_text(char *url);
ParseResults *parse_chan();
int find_total_num_threads(cJSON *catalog);