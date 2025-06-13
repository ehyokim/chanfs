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

/*TODO: Find a way to nicely set up the curl environment and then cleanly free everything when you are done. */
ParseResults *parse_chan()
{
        curl_global_init(CURL_GLOBAL_ALL);

        char *webpage_text = retrieve_webpage_text("https://lainchan.org/lit/catalog.json");
        cJSON *catalog = cJSON_Parse(webpage_text);

        ParseResults *results = (ParseResults *) malloc(sizeof(ParseResults));
        int total_num_threads = find_total_num_threads(catalog);
        results->num_of_threads = total_num_threads;
        results->thread_titles = (char **) malloc((total_num_threads + 1) * sizeof(char *));

        if(catalog == NULL) {
            fprintf(stderr, "Could not allocate JSON struct");
        }
        
        char **thread_titles = results->thread_titles;
        int thread_idx = 0;

        cJSON *threads = NULL;
        cJSON_ArrayForEach(threads, catalog)
        {
            cJSON *thread_list = cJSON_GetObjectItemCaseSensitive(threads, "threads");

            if (!cJSON_IsArray(thread_list)) {
                fprintf(stderr, "Could not find thread array");
                return 0;
            }

            cJSON *thread = NULL;
            char *title = NULL;
            cJSON_ArrayForEach(thread, thread_list)
            {
                cJSON *com = cJSON_GetObjectItemCaseSensitive(thread, "com");

                if (!cJSON_IsString(com)) {
                    fprintf(stderr, "Could not find com");
                    return 0;
                }

                title = com->valuestring;
                thread_titles[thread_idx] = (char *) malloc(strlen(title) + 1); //TODO: fill out error checking for this.
                strcpy(thread_titles[thread_idx], title);
                thread_idx++;
            }
        }

        thread_titles[total_num_threads] = NULL; //End of pointer array marker.

        cJSON_Delete(catalog);
        free(webpage_text);
        curl_global_cleanup();

        return results;
}

int find_total_num_threads(cJSON *catalog) 
{
    int total_num_threads = 0;

    cJSON *threads = NULL;
    cJSON_ArrayForEach(threads, catalog)
    {
        cJSON *thread_list = cJSON_GetObjectItemCaseSensitive(threads, "threads");
        if (!cJSON_IsArray(thread_list)) {
            fprintf(stderr, "Could not find thread array");
            return -1;
        }

        total_num_threads += cJSON_GetArraySize(thread_list);
    }

    return total_num_threads;
}
