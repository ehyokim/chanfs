#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "include/chan_parse.h"

static int find_total_num_threads(cJSON *catalog);
static int find_total_num_replies(cJSON *thread);
static char *constr_catalog_url(char *chan_url, char *board);
static char *constr_thread_url(char *chan_url, char *board, int thread_op_no);
static Post parse_post_json_object(char * board, cJSON *post_json_obj);
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static char *retrieve_webpage_text(char *url);

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *) userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    
    if (!ptr) {
        printf("Write memory callback failed to allocate memory.\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
} 

static char *retrieve_webpage_text(char *url)
{
    CURL *curl_handle;
    CURLcode res;

    struct MemoryStruct chunk;

    char *mem_chunk = chunk.memory = malloc(1);  /* grown as needed by the realloc above */
    if (!mem_chunk) {
        fprintf(stderr, "Initial memory chunk could not be allocated");
    }
    chunk.size = 0;    /* no data at this point */

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url) ;
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    res = curl_easy_perform(curl_handle);

    /* check for errors */
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        return NULL;
    }

    curl_easy_cleanup(curl_handle);
    return (char *)chunk.memory;
}

//Fix error handling here. 
Thread parse_thread(char *board, int thread_op_no)
{

    char *thread_url = constr_thread_url("https://lainchan.org", board, thread_op_no);
    char *webpage_text = retrieve_webpage_text(thread_url);
    cJSON *thread = cJSON_Parse(webpage_text);

    if (thread == NULL) {
        fprintf(stderr, "Could not allocate thread JSON struct.\n");
        return (Thread) {NULL, -1};
    }

    Thread results;
    int total_num_replies = find_total_num_replies(thread);
    results.num_of_replies = total_num_replies;
    results.posts = (Post *) malloc((total_num_replies + 1) * sizeof(Post));

    Post *thread_replies = results.posts;
    
    cJSON *replies = cJSON_GetObjectItemCaseSensitive(thread, "posts");
    if (!cJSON_IsArray(replies)) {
        fprintf(stderr, "Could not find post array.\n");
        return (Thread) {NULL, -1};
    }
    
    cJSON *post_json_obj = NULL;
    cJSON_ArrayForEach(post_json_obj, replies) 
    {
         *thread_replies = parse_post_json_object(board, post_json_obj);
        thread_replies++;
    }

    thread_replies = NULL;  //End of pointer array marker.

    cJSON_Delete(thread);
    free(webpage_text);
    free(thread_url);

    return results;
}

Board parse_board(char *board)
{

    char *catalog_url = constr_catalog_url("https://lainchan.org", board);
    char *webpage_text = retrieve_webpage_text(catalog_url);
    cJSON *catalog = cJSON_Parse(webpage_text);

    Board results;
    int total_num_threads = find_total_num_threads(catalog);
    results.num_of_threads = total_num_threads;
    results.threads = (Post *) malloc((total_num_threads + 1) * sizeof(Post)); //Could just allocate a continguous block of structs rather than pointers to structs.

    if(catalog == NULL) {
        fprintf(stderr, "Could not allocate catalog JSON struct.\n");
        return (Board) {NULL, -1};
    }
    
    Post *thread_op_posts = results.threads;

    cJSON *threads = NULL;
    cJSON_ArrayForEach(threads, catalog)
    {
        cJSON *thread_list = cJSON_GetObjectItemCaseSensitive(threads, "threads");

        if (!cJSON_IsArray(thread_list)) {
            fprintf(stderr, "Could not find thread array.\n");
            return (Board) {NULL, -1};
        }
        
        cJSON *thread_op = NULL;
        cJSON_ArrayForEach(thread_op, thread_list)
        {
            *thread_op_posts = parse_post_json_object(board, thread_op);
            thread_op_posts++;
        }
    }

    thread_op_posts = NULL; //End of pointer array marker.

    cJSON_Delete(catalog);
    free(webpage_text);
    free(catalog_url);

    return results;
}

static Post parse_post_json_object(char *board, cJSON *post_json_obj)
{

    Post post = {};

    cJSON *no = cJSON_GetObjectItemCaseSensitive(post_json_obj, "no");
    post.no = no->valueint;

    cJSON *sub = cJSON_GetObjectItemCaseSensitive(post_json_obj, "sub");
    if (cJSON_IsString(sub)) 
        post.sub = strdup(sub->valuestring);
    
    cJSON *com = cJSON_GetObjectItemCaseSensitive(post_json_obj, "com");
    if (cJSON_IsString(com)) 
        post.com = strdup(com->valuestring);

    cJSON *name = cJSON_GetObjectItemCaseSensitive(post_json_obj, "name");
    if (cJSON_IsString(name)) 
        post.name = strdup(name->valuestring);
    
    cJSON *tim = cJSON_GetObjectItemCaseSensitive(post_json_obj, "tim");
    if (cJSON_IsString(tim))
        post.tim = strdup(tim->valuestring);

    cJSON *filename = cJSON_GetObjectItemCaseSensitive(post_json_obj, "filename");
    if (cJSON_IsString(filename))
        post.filename = strdup(filename->valuestring);
    
    cJSON *ext = cJSON_GetObjectItemCaseSensitive(post_json_obj, "ext");
    if (cJSON_IsString(ext))
        post.ext = strdup(ext->valuestring);        

    cJSON *trip = cJSON_GetObjectItemCaseSensitive(post_json_obj, "trip");
    if (cJSON_IsString(trip))
        post.filename = strdup(trip->valuestring);

    cJSON *email = cJSON_GetObjectItemCaseSensitive(post_json_obj, "email");
    if (cJSON_IsString(email))
        post.email = strdup(email->valuestring);

    cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(post_json_obj, "time");
    post.timestamp = (time_t) timestamp->valueint;

    post.board = board;
    return post;
}

static char *constr_thread_url(char *chan_url, char *board, int thread_op_no) 
{
    char *post_no = thread_int_to_str(thread_op_no);
    size_t url_len = strlen(chan_url) + strlen(post_no) + strlen(post_no) + 12;

    char *url = (char *) malloc(url_len);
    sprintf(url, "%s/%s/res/%s.json", chan_url, board, post_no);

    free(post_no);
    return url;
}

char *thread_int_to_str(int thread_no) 
 {
    char *post_no_str = (char *) malloc(13);
    sprintf(post_no_str, "%d", thread_no);
    return post_no_str; 
 }


static char *constr_catalog_url(char *chan_url, char *board)
{
    size_t url_len = strlen(chan_url) + strlen(board) + 15;
    char *url = (char *) malloc(url_len); 
    sprintf(url, "%s/%s/catalog.json", chan_url, board);

    return url;
}

static int find_total_num_replies(cJSON *thread)
{
    cJSON *posts = cJSON_GetObjectItemCaseSensitive(thread, "posts");
    if (!cJSON_IsArray(posts)) {
        fprintf(stderr, "Could not find post array.\n");
        return -1;
    }

    return cJSON_GetArraySize(posts);
}

static int find_total_num_threads(cJSON *catalog) 
{
    int total_num_threads = 0;

    cJSON *threads = NULL;
    cJSON_ArrayForEach(threads, catalog)
    {
        cJSON *thread_list = cJSON_GetObjectItemCaseSensitive(threads, "threads");
        if (!cJSON_IsArray(thread_list)) {
            fprintf(stderr, "Could not find thread array.\n");
            return -1;
        }

        total_num_threads += cJSON_GetArraySize(thread_list);
    }

    return total_num_threads;
}

void free_board_parse_results(Board results)
{
    /* Free up parse results. */
    for (int j = 0; j < results.num_of_threads; j++) {
        free_post(results.threads[j]);
    }
}

void free_thread_parse_results(Thread results)
{
    /* Free up parse results. */
    for (int j = 0; j < results.num_of_replies; j++) {
        free_post(results.posts[j]);
    }
}

void free_post(Post post) {
    free(post.sub);
    free(post.com);
    free(post.name);
    free(post.tim);
    free(post.filename);
    free(post.ext);
    free(post.trip);
    free(post.email);
} 