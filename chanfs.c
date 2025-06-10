#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "include/chanfs.h"

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
    if(!ptr) {
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
    } 

int main(void)
{
    CURL *curl_handle;
    CURLcode res;

    struct MemoryStruct chunk;

    chunk.memory = malloc(1);  /* grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, "https://lainchan.org/lit/catalog.json");

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

    /* some servers do not like requests that are made without a user-agent
        field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* get it! */
    res = curl_easy_perform(curl_handle);

    /* check for errors */
    if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    }
    else {
    /*
        * Now, our chunk.memory points to a memory block that is chunk.size
        * bytes big and contains the remote file.
        *
        * Do something nice with it!
        */

    //printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
        cJSON *catalog = cJSON_Parse(chunk.memory);
        if(catalog == NULL) {
            printf("Could not allocate JSON struct");
        }
        
        cJSON *threads = NULL;
        cJSON_ArrayForEach(threads, catalog)
        {
            cJSON *thread_list = cJSON_GetObjectItemCaseSensitive(threads, "threads");
            if (!cJSON_IsArray(thread_list)) {
                printf("Could not find thread array");
                return 1;
            }

            cJSON *thread = NULL;
            cJSON_ArrayForEach(thread, thread_list)
            {
                cJSON *com = cJSON_GetObjectItemCaseSensitive(thread, "com");
                if (!cJSON_IsString(com)) {
                    printf("Could not find com");
                    return 1;
                }
                printf("%s\n", com->valuestring);
            }
        }

        /*
        char *tree = cJSON_Print(catalog);
        if(tree == NULL) {
            printf("Failed to print catalog");
        }
        printf("%s", tree);

        cJSON_Delete(catalog);
        }
        */

        cJSON_Delete(catalog);

        /* cleanup curl stuff */
        curl_easy_cleanup(curl_handle);
        free(chunk.memory);

        /* we are done with libcurl, so clean it up */
        curl_global_cleanup();

        return 0;
    }
}