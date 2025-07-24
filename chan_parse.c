#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "include/consts.h"
#include "include/chan_parse.h"
#include "include/textproc.h"

extern const char *chan;
extern const int chan_str_len;

typedef struct MemoryStruct {
  char *memory;
  size_t mem_size;
  size_t buffer_size;
} MemoryStruct;

static int find_total_num_threads(cJSON *catalog);
static int find_total_num_replies(cJSON *thread);
static char *constr_thread_url(char *board, postno_t thread_op_no);
static void parse_post_json_object(Post *post, cJSON *post_json_obj);
static MemoryStruct retrieve_web_content(char *url);

static size_t
write_to_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *) userp;

    if (mem->mem_size + (realsize + 1) >= mem->buffer_size) {
        char *ptr = realloc(mem->memory, 2*(mem->buffer_size + realsize + 1));
        if (!ptr) {
            fprintf(stderr, "Write memory callback failed to allocate memory.\n");
            return 0;
        }
        mem->memory = ptr;
        mem->buffer_size = 2*(mem->buffer_size + realsize + 1);
    }

    memcpy(&(mem->memory[mem->mem_size]), contents, realsize);
    mem->mem_size += realsize;
    mem->memory[mem->mem_size] = 0;

    return realsize;
} 

static MemoryStruct 
retrieve_web_content(char *url)
{
    CURL *curl_handle;
    CURLcode res;
    MemoryStruct chunk;

    if (!url) {
	    fprintf(stderr, "Error: Invalid URL");
        return (MemoryStruct) {NULL, -1};

    }

    chunk.memory = malloc(INIT_CURL_MEM_BUF_SIZE);
    if (!chunk.memory) {
        fprintf(stderr, "Initial memory chunk could not be allocated");
        return (MemoryStruct) {NULL, -1};

    }
    chunk.mem_size = 0;
    chunk.buffer_size = INIT_CURL_MEM_BUF_SIZE;

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url) ;
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_to_memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        return (MemoryStruct) {NULL, -1};
    }

    curl_easy_cleanup(curl_handle);
    return chunk;

}

AttachedFile 
download_file(char *board, char *filename)
{
    if (!filename) {
        return (AttachedFile) {NULL, 0};
    }

    size_t url_len = chan_str_len + strlen(board) + strlen(filename) + 7;
    char file_url[url_len];
    sprintf(file_url, "%s/%s/src/%s", chan, board, filename);
    
    MemoryStruct chunk = retrieve_web_content(file_url);
    if (!chunk.memory) {
        return (AttachedFile) {NULL, 0};
    }

    return (AttachedFile) {chunk.memory, chunk.mem_size};
}

Thread 
parse_thread(char *board, postno_t thread_op_no)
{
    Thread results;
    int success_status = 0;

    char *thread_url = constr_thread_url(board, thread_op_no);
    char *webpage_text = retrieve_web_content(thread_url).memory;

    if (!webpage_text)
        goto retrieve_fail;
    
    cJSON *thread = cJSON_Parse(webpage_text);

    if (!thread) {
        fprintf(stderr, "Error: Could not allocate thread JSON struct.\n");
        goto parse_fail;        
    }

    int total_num_replies = find_total_num_replies(thread);
    if (total_num_replies < 0) {
        fprintf(stderr, "Error: Could not parse thread.\n");
        goto traverse_fail;
    }

    /* Everything will be zero or NULL at initialization */
    results.num_of_posts = total_num_replies;
    results.posts = calloc((total_num_replies + 1), sizeof(Post));

    if (!results.posts) {
        fprintf(stderr, "Error: Allocating memory for Post array for Thread struct failed.\n");
        goto traverse_fail;
    }

    Post *thread_replies = results.posts;
    
    cJSON *replies = cJSON_GetObjectItemCaseSensitive(thread, "posts");
    if (!cJSON_IsArray(replies)) {
        fprintf(stderr, "Error: Could not find post array.\n");
        goto traverse_fail;
    }
    
    cJSON *post_json_obj = NULL;
    cJSON_ArrayForEach(post_json_obj, replies) 
    {
        thread_replies->board = board;
        parse_post_json_object(thread_replies, post_json_obj);
        thread_replies++;
    }

    thread_replies = NULL;  //End of pointer array marker.
    success_status = 1;

traverse_fail:
    cJSON_Delete(thread);
parse_fail:
    free(webpage_text);
retrieve_fail:
    free(thread_url);

    return (success_status) ? results : (Thread) {NULL, -1};
}

Board 
parse_board(char *board)
{   
    Board results;
    int success_status = 0;

    size_t url_len = chan_str_len + strlen(board) + 15;
    char catalog_url[url_len]; 
    sprintf(catalog_url, "%s/%s/catalog.json", chan, board);

    char *webpage_text = retrieve_web_content(catalog_url).memory;
    if (!webpage_text)
        goto retrieve_fail;

    cJSON *catalog = cJSON_Parse(webpage_text);

    if (!catalog) {
        fprintf(stderr, "Could not allocate catalog JSON struct.\n");
        goto parse_fail;
    }
    
    int total_num_threads = find_total_num_threads(catalog);
    if (total_num_threads < 0) {
        fprintf(stderr, "Error: Could not parse catalog.\n");
        goto traverse_fail;
    }

    /* Everything will be zero or NULL at initialization */
    results.num_of_threads = total_num_threads;
    results.threads = calloc((total_num_threads + 1), sizeof(Post)); 
    if (!results.threads) {
        fprintf(stderr, "Error: Could not allocate memory for Post array for Board struct. \n");
        goto traverse_fail;
    }
    
    Post *thread_op_posts = results.threads;

    cJSON *threads = NULL;
    cJSON_ArrayForEach(threads, catalog)
    {
        cJSON *thread_list = cJSON_GetObjectItemCaseSensitive(threads, "threads");

        if (!cJSON_IsArray(thread_list)) {
            fprintf(stderr, "Could not find thread array.\n");
            goto traverse_fail;
        }
        
        cJSON *thread_op = NULL;
        cJSON_ArrayForEach(thread_op, thread_list)
        {
            thread_op_posts->board = board;
            parse_post_json_object(thread_op_posts, thread_op);
            thread_op_posts++;
        }
    }

    thread_op_posts = NULL; //End of pointer array marker.
    success_status = 1;

traverse_fail:
    cJSON_Delete(catalog);
parse_fail:
    free(webpage_text);
retrieve_fail:
    return (success_status) ? results : (Board) {NULL, -1};
}

static void 
parse_post_json_object(Post *post, cJSON *post_json_obj)
{
    cJSON *no = cJSON_GetObjectItemCaseSensitive(post_json_obj, "no");
    post->no = (postno_t) no->valueint;

    cJSON *sub = cJSON_GetObjectItemCaseSensitive(post_json_obj, "sub");
    if (cJSON_IsString(sub)) 
        post->sub = strdup(sub->valuestring);
    
    cJSON *com = cJSON_GetObjectItemCaseSensitive(post_json_obj, "com");
    if (cJSON_IsString(com)) {
        post->com = strdup(com->valuestring);
    }
    
    cJSON *name = cJSON_GetObjectItemCaseSensitive(post_json_obj, "name");
    if (cJSON_IsString(name)) 
        post->name = strdup(name->valuestring);
    
    cJSON *tim = cJSON_GetObjectItemCaseSensitive(post_json_obj, "tim");
    if (cJSON_IsString(tim))
        post->tim = strdup(tim->valuestring);

    cJSON *filename = cJSON_GetObjectItemCaseSensitive(post_json_obj, "filename");
    if (cJSON_IsString(filename))
        post->filename = strdup(filename->valuestring);
    
    cJSON *ext = cJSON_GetObjectItemCaseSensitive(post_json_obj, "ext");
    if (cJSON_IsString(ext))
        post->ext = strdup(ext->valuestring);        

    cJSON *trip = cJSON_GetObjectItemCaseSensitive(post_json_obj, "trip");
    if (cJSON_IsString(trip))
        post->filename = strdup(trip->valuestring);

    cJSON *email = cJSON_GetObjectItemCaseSensitive(post_json_obj, "email");
    if (cJSON_IsString(email))
        post->email = strdup(email->valuestring);

    cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(post_json_obj, "time");
    post->timestamp = (time_t) timestamp->valueint;
}

static char *
constr_thread_url(char *board, postno_t thread_op_no) 
{
    char post_buffer[MAX_POST_NO_DIGITS + 1];

    int intstr_res = post_no_to_str(thread_op_no, post_buffer);
    if (intstr_res < 0) {
        return NULL;
    }

    size_t url_len = chan_str_len + strlen(board) + MAX_POST_NO_DIGITS + 12;
    
    char *url = malloc(url_len);
    if (!url) {
        fprintf(stderr, "Error: Could not allocate memory to create url to OP thread.\n");
        return NULL;
    }

    snprintf(url, url_len, "%s/%s/res/%s.json", chan, board, post_buffer);
    return url;
}

int 
post_no_to_str(postno_t thread_no, char buffer[]) 
{
    return snprintf(buffer, MAX_POST_NO_DIGITS, "%llu", thread_no);
}

static int 
find_total_num_replies(cJSON *thread)
{
    cJSON *posts = cJSON_GetObjectItemCaseSensitive(thread, "posts");
    if (!cJSON_IsArray(posts)) {
        fprintf(stderr, "Could not find post array.\n");
        return -1;
    }

    return cJSON_GetArraySize(posts);
}

static int 
find_total_num_threads(cJSON *catalog) 
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

void 
free_board_parse_results(Board results)
{
    /* Free up parse results. */
    for (int j = 0; j < results.num_of_threads; j++) {
        free_post(results.threads[j]);
    }
}

void 
free_thread_parse_results(Thread results)
{
    /* Free up parse results. */
    for (int j = 0; j < results.num_of_posts; j++) {
        free_post(results.posts[j]);
    }
}

void 
free_post(Post post) 
{
    free(post.sub);
    free(post.com);
    free(post.name);
    free(post.tim);
    free(post.filename);
    free(post.ext);
    free(post.trip);
    free(post.email);
} 
