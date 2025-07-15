typedef struct str_rep_buffer {
    int buffer_size;
    int curr_str_size;
    char *buffer_start;
    char *str_end;
} StrRepBuffer;

typedef enum filename_type {
    ORIGINAL, RENAMED 
} FilenameType;

StrRepBuffer generate_post_str_rep(Post *post);
StrRepBuffer generate_thread_str_rep(Thread thread);
StrRepBuffer new_error_buffer(char *board); 
void free_str_rep_buffer(StrRepBuffer str_buffer);
char *concat_filename_ext(Post *post, FilenameType type);
