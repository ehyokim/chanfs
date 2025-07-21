struct str_rep_buffer {
    int buffer_size;
    int curr_str_size;
    int used_cols;
    char *buffer_start;
    char *str_end;
};

typedef enum filename_type {
    ORIGINAL, RENAMED 
} FilenameType;

StrRepBuffer generate_post_str_rep(Post *post);
StrRepBuffer generate_thread_str_rep(Thread thread);
StrRepBuffer new_error_buffer(char *board); 
StrRepBuffer new_str_rep_buffer(char *input_str_buffer, int buffer_size) ; 
void free_str_rep_buffer(StrRepBuffer str_buffer);
void append_to_buffer(StrRepBuffer *str_buffer, char *str, int str_len);
char *concat_filename_ext(Post *post, FilenameType type);
void parse_html_for_thread(Thread thread);