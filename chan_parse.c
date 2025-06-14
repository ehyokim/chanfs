#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "include/chan_parse.h"

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




Thread *parse_thread(char *board, unsigned int thread_op_no)
{

    char *thread_url = constr_thread_url("https://lainchan.org", board, thread_op_no);
    char *webpage_text = retrieve_webpage_text(thread_url);
    cJSON *thread = cJSON_Parse(webpage_text);

    Thread *results = (Thread *) malloc(sizeof(Thread));
    int total_num_replies = find_total_num_replies(thread);
    results->num_of_replies = total_num_replies;
    results->posts = (Post **) malloc((total_num_replies + 1) * sizeof(Post *));

    cJSON_Delete(thread);
    free(webpage_text);
    free(thread_url);

    return NULL; //To be filled out
}


/*TODO: Find a way to nicely set up the curl environment and then cleanly free everything when you are done. */
//Fix this to reflect Post struct.
Board *parse_board(char *board)
{

        char *catalog_url = constr_catalog_url("https://lainchan.org", board);
        char *webpage_text = retrieve_webpage_text(catalog_url);
        cJSON *catalog = cJSON_Parse(webpage_text);

        Board *results = (Board *) malloc(sizeof(Board));
        int total_num_threads = find_total_num_threads(catalog);
        results->num_of_threads = total_num_threads;
        results->threads = (Post **) malloc((total_num_threads + 1) * sizeof(Post *));

        if(catalog == NULL) {
            fprintf(stderr, "Could not allocate catalog JSON struct.\n");
            return NULL;
        }
        
        Post **thread_op_posts = results->threads;
        int thread_idx = 0;

        cJSON *threads = NULL;
        cJSON_ArrayForEach(threads, catalog)
        {
            cJSON *thread_list = cJSON_GetObjectItemCaseSensitive(threads, "threads");

            if (!cJSON_IsArray(thread_list)) {
                fprintf(stderr, "Could not find thread array.\n");
                return NULL;
            }

            cJSON *thread = NULL;
            char *subject = NULL;
            cJSON_ArrayForEach(thread, thread_list)
            {
                //TODO: fill out error checking for this.
                Post *thread_op = thread_op_posts[thread_idx] = (Post *) calloc(1, sizeof(Post)); 
                
                //Fill out the subject field
                cJSON *sub = cJSON_GetObjectItemCaseSensitive(thread, "sub");
                if (cJSON_IsString(sub)) 
                    subject = sub->valuestring;
                else {
                    sub = cJSON_GetObjectItemCaseSensitive(thread, "com");
                    if (!cJSON_IsString(sub))
                        subject = "No subject";
                    else
                        subject = sub->valuestring;
                } 

                thread_op->sub = (char *) malloc(strlen(subject) + 1);
                strcpy(thread_op->sub, subject);

                cJSON *time = cJSON_GetObjectItemCaseSensitive(thread, "time");
                time_t timestamp = (time_t) time->valueint;
                
                thread_op->timestamp = timestamp;
                thread_idx++;
            }
        }

        thread_op_posts[total_num_threads] = NULL; //End of pointer array marker.

        cJSON_Delete(catalog);
        free(webpage_text);
        free(catalog_url);

        return results;
}

static char *constr_thread_url(char *chan_url, char *board, unsigned int thread_op_no) 
{
    char post_no[20];
    sprintf(post_no, "%u", thread_op_no); 
    size_t url_len = strlen(chan_url) + strlen(post_no) + strlen(post_no) + 12;

    char *url = (char *) malloc(url_len);
    sprintf(url, "%s/%s/res/%s.json", chan_url, board, post_no);

    return url;
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

void free_board_parse_results(Board *results)
{
    /* Free up parse results */
    for (int j = 0; j < results->num_of_threads; j++) {
        free_post(results->threads[j]);
    }
    free(results->threads);
    free(results);
}

void free_post(Post *post) {
    free(post->sub);
    free(post->com);
    free(post->name);
    free(post->tim);
    free(post->filename);
    free(post->ext);
    free(post->trip); //Null pointers may be free. Fix this.
    free(post->email);

    free(post);
} 