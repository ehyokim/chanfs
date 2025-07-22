#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <lexbor/html/tokenizer.h>

#include "include/consts.h"
#include "include/chan_parse.h"
#include "include/textproc.h"


static void append_to_buffer_cols(StrRepBuffer *str_buffer, char *str, int str_len);
static void flush_divider_to_str_rep_buffer(StrRepBuffer *str_buffer);
static void check_buffer_dims(StrRepBuffer *str_buffer, int str_len);
static size_t generate_time_string(const time_t t, char buffer[]);
static void concat_str_rep_buffers(StrRepBuffer *s1, StrRepBuffer s2);
static void append_to_buffer_formatted(StrRepBuffer *str_buffer, char *str_formatter, char *str);
static int concat_ori_truncated_filename_ext(Post *post, char buffer[]);
static char *parse_html(Post *post); 

typedef struct html_parse_data {
    int is_link;
    int num_replies_found;
    int size_of_reply_buf;
    int *replies_found;
    StrRepBuffer parsed_text;
} HTMLParseStruct;

static int 
add_reply_to_html_data_struct(char *reply_no, HTMLParseStruct *pd)
{
    if (pd->num_replies_found == 0) {
        pd->replies_found = malloc(30 * sizeof(int)); //Probably won't be more than 30 replies.
        
        if (!(pd->replies_found)) {
            fprintf(stderr, "Error: Cannot allocate memory for replies array in HTML parser struct\n");
            return 1;
        }
        
        pd->size_of_reply_buf = 30;

    } else if ((pd->num_replies_found + 1) >= pd->size_of_reply_buf) {
         int *np = realloc(pd->replies_found, 2*pd->num_replies_found * sizeof(int));

        if (!np) {
            fprintf(stderr, "Error: Cannot allocate memory for replies array in HTML parser struct\n");
            return 1;                        
        }

        pd->replies_found = np;
        pd->size_of_reply_buf = 2*pd->num_replies_found;
    } 
    
    *(pd->replies_found + pd->num_replies_found) = atoi(reply_no);
    pd->num_replies_found++;

    return 0;
}

static lxb_html_token_t *
tokenizer_callback(lxb_html_tokenizer_t *tkz, lxb_html_token_t *token, void *ctx) 
{
    HTMLParseStruct *parsed_text_ptr = (HTMLParseStruct *) ctx;

    lxb_html_token_attr_t *attr;
    const lxb_char_t *name;

    size_t len;
    char *value_str;

    unsigned int is_close_token = token->type & LXB_HTML_TOKEN_TYPE_CLOSE;
    int *is_link = &parsed_text_ptr->is_link;

    switch (token->tag_id) {
        case LXB_TAG__TEXT:
            len = token->text_end - token->text_start;
            value_str = token->text_start;
            break;
        case LXB_TAG_SPAN:
            return token;
            break;
        case LXB_TAG_A:
            if (is_close_token) {
                /* If we find a close token and its associated to a link, 
                 * print the closing parenthesis. */
                if (*is_link) {
                    value_str = ")";
                    len = 1;
                    *is_link = 0;
                } else {
                    return token;
                }
                break;
            }
            name = lxb_html_token_attr_name(token->attr_first, NULL);  
            if (strcmp(name, "onclick") == 0) {
                /* This should be href since we are working with a reply here */
                attr = token->attr_first->next;
                value_str = attr->value;
                /* Start from the end where the reply post number is located */
                value_str += strlen(value_str);
                /* Find the reply post number from the href field */
                char c;
                len = 0;
                while ((c = *(--value_str)) != '#') {
                    if (islower(c)) {
                        fprintf(stderr, "Error: Demarking # non-existent in reply link.");
                        return token;
                    }
                    len++;
                }
                /* Begin from first digit of reply post */
                value_str++;
                /* Now record the found reply */
                add_reply_to_html_data_struct(value_str, parsed_text_ptr);
                return token;
            } else if (strcmp(name, "href") == 0) {
                /* Otherwise, it will be a regular link */
                value_str = "(Link: ";
                len = 7;
                *is_link = 1;
            } else {
                /* Otherwise, if it's some other link, we don't care and discard. */
                return token;
            }
            break;
        case LXB_TAG_BR:
            value_str = "\n";
            len = 1;
            break;
        default:
            return token;
            break;   
    }

    append_to_buffer(&parsed_text_ptr->parsed_text, value_str, len);
    return token;
}


static char *
parse_html(Post *post) 
{
    lxb_status_t status;
    lxb_html_tokenizer_t *tkz;
    char *raw_str = post->com;

    if (!raw_str) {
        return NULL;
    }

    tkz = lxb_html_tokenizer_create();
    status = lxb_html_tokenizer_init(tkz);
    if (status != LXB_STATUS_OK) {
        fprintf(stderr, "Error: HTML tokenizer initization failed.");
        goto init_fail;
    }

    /* Initialize an empty buffer equal to length of input string */
    size_t tot_len_input = strlen(raw_str);
    char *new_buf = malloc(tot_len_input + 1);
    if (!new_buf) {
        fprintf(stderr, "Error: Cannot allocate memory for parsed HTML buffer\n");
        goto init_fail;
    }

    *new_buf = 0;
    StrRepBuffer parsed_text = new_str_rep_buffer(new_buf, tot_len_input + 1);
    HTMLParseStruct parse_struct = {0, 0, 0, NULL, parsed_text};

    lxb_html_tokenizer_callback_token_done_set(tkz, tokenizer_callback, &parse_struct);

    status = lxb_html_tokenizer_begin(tkz);
    if (status != LXB_STATUS_OK) {
        fprintf(stderr, "Error: Failed to prepare tokenizer object.");
        goto parse_fail;
    }

    status = lxb_html_tokenizer_chunk(tkz, raw_str, tot_len_input);
    if (status != LXB_STATUS_OK) {
        fprintf(stderr, "Error: Failed to parse HTML");
        goto parse_fail;
    }

    status = lxb_html_tokenizer_end(tkz);
    if (status != LXB_STATUS_OK) {
        fprintf(stderr, "Error: Failed to stop parsing of HTML");
        goto parse_fail;
    }

    lxb_html_tokenizer_destroy(tkz);

    if (parse_struct.num_replies_found > 0) {
        post->replies_to = parse_struct.replies_found;
        post->num_replies_to = parse_struct.num_replies_found;      
    }

    return parsed_text.buffer_start;

parse_fail:
    free_str_rep_buffer(parsed_text); 
init_fail:
    lxb_html_tokenizer_destroy(tkz);
    return NULL;
}

void 
parse_html_for_thread(Thread thread)
{
    int num_of_posts = thread.num_of_posts;
    Post *replies = thread.posts;
    for (int i = (num_of_posts - 1); i >= 0; i--) {
        Post *post = replies + i;
        post->parsed_com = parse_html(post);

        int *next_slot = NULL; 
        /* Run through all of the posts made after the current post */
        for (int j = i+1; j < num_of_posts; j++) {
            Post *later_post = replies + j;
            int *lp_replies_to = later_post->replies_to;
            /* Check if any replies made to the current post from the later post */
            for (int k = 0; k < later_post->num_replies_to; k++) {
                if (post->no == lp_replies_to[k]) {
                    /* We only allocate memory when is at least one reply to the current post */
                    if (!next_slot) {
                        /* Since OP can't reply to himself, this is sufficiently large for all replies */
                        post->replies_from = malloc(num_of_posts * sizeof(int));
                        if (!(post->replies_from)) {
                            fprintf(stderr, "Error: Cannot allocate memory for replies from array for post\n");
                            goto allo_fail;
                        }
                        next_slot = post->replies_from;
                    }  

                    *next_slot = later_post->no;
                    next_slot++;
                    post->num_replies_from++;
                }
            }
        }
allo_fail:
    }
}


/* This is bad since it generates the post string contents twice at most. */
StrRepBuffer 
generate_thread_str_rep(Thread thread) 
{
    Post *replies = thread.posts;
    StrRepBuffer thread_str_buffer = generate_post_str_rep(replies); //Begin with converting the OP post.

    if (thread_str_buffer.buffer_size == 0) {
        fprintf(stderr, "Error: Could not start thread string representation buffer.\n");
        return thread_str_buffer;
    } 

    flush_divider_to_str_rep_buffer(&thread_str_buffer);

    for (int i = 1; i < thread.num_of_posts; i++) {
        StrRepBuffer reply_str_buffer = generate_post_str_rep(replies + i);
        concat_str_rep_buffers(&thread_str_buffer, reply_str_buffer);
        free_str_rep_buffer(reply_str_buffer);
        flush_divider_to_str_rep_buffer(&thread_str_buffer);
    }

    return thread_str_buffer;
}


StrRepBuffer 
new_error_buffer(char *board) 
{
    StrRepBuffer err_str_buffer = new_str_rep_buffer(NULL, 0);
    append_to_buffer_formatted(&err_str_buffer, 
                               "Error: Board %s has failed to load. Perhaps it doesn't exist?", 
                               board);
    
    return err_str_buffer;
}

void 
free_str_rep_buffer(StrRepBuffer str_buffer)
{
    free(str_buffer.buffer_start);
}

/* Generate the string representation of a chan post. */
StrRepBuffer 
generate_post_str_rep(Post *post) 
{
    char time_buffer[MAX_TIME_STR_LEN];
    char post_no_buffer[MAX_POST_NO_DIGITS + 1];
    char filename_buffer[MAX_FILENAME_LEN + 1];

    StrRepBuffer buffer = new_str_rep_buffer(NULL, 0);
    if (buffer.buffer_size == 0) {
        fprintf(stderr, "Error; Could not allocate memory to start a new StrRepBuffer.\n");
        return buffer;
    }

    append_to_buffer_formatted(&buffer, "Board: /%s/\n", post->board);

    int intstr_res = post_int_to_str(post->no, post_no_buffer); 
    if (intstr_res >= 0) {
        append_to_buffer_formatted(&buffer, "No: %s\n", post_no_buffer);
    }

    append_to_buffer_formatted(&buffer, "Name: %s\n", post->name);
    append_to_buffer_formatted(&buffer, "Trip: %s\n", post->trip);
    append_to_buffer_formatted(&buffer, "Email: %s\n", post->email);
    
    size_t time_res = generate_time_string(post->timestamp, time_buffer);
    if (time_res) {
        append_to_buffer_formatted(&buffer, "Time: %s\n", time_buffer);
    }

    int trun_res = concat_ori_truncated_filename_ext(post, filename_buffer);
    if (trun_res) {
        append_to_buffer_formatted(&buffer, "Attached File: %s\n", filename_buffer);
    }

    append_to_buffer_formatted(&buffer, "Subject: %s\n", post->sub);

    if (post->num_replies_from > 0) {
        char *reply_str_buf = malloc(post->num_replies_from * (MAX_POST_NO_DIGITS + 1) + 1);
        if (!reply_str_buf) {
            fprintf(stderr, "Error: Cannot allocate memory for reply string buffer\n");
            goto allo_fail;
        }

        size_t tail = 0;
        for (int i = 0; i < post->num_replies_from; i++) {
            int res = sprintf(reply_str_buf + tail, " %d", post->replies_from[i]);
            if (res < 0) {
                fprintf(stderr, "Error: sprintf failed while writing reply numbers.\n");
                goto print_fail;
            }
            tail += res;
        }

        append_to_buffer_formatted(&buffer, "Replies to this Post:%s\n", reply_str_buf);
print_fail:
        free(reply_str_buf);
    }
allo_fail:
    append_to_buffer_formatted(&buffer, "\n\n%s\n", post->parsed_com);
    return buffer;
}

/* 
 * Creates a new string representation buffer. If it is supplied a pre-existing buffer, then it simply
 * initializes the string rep buffer using that supplied buffer. Otherwise, it allocates a new empty buffer with
 * an empty string. 
 */
StrRepBuffer 
new_str_rep_buffer(char *input_str_buffer, int buffer_size) 
{
    if (input_str_buffer != NULL) {
        int str_len_in_buf = strlen(input_str_buffer);
        return (StrRepBuffer) {buffer_size, 
                               str_len_in_buf,
                               0, 
                               input_str_buffer, 
                               input_str_buffer + str_len_in_buf
                              };
    }

    buffer_size = 100; // Prob should turn this into a constant.
    char *buffer_start = malloc(buffer_size);

    if (!buffer_start) {
        buffer_start = "";
        buffer_size = 0;
    }

    return (StrRepBuffer) {buffer_size, 
                           0, 
                           0, 
                           buffer_start, 
                           buffer_start};
}

/* Concatenates the name of an attached file associated with a post and its extention together. */
char *
concat_filename_ext(Post *post, FilenameType type)
{
    char *filename = (type == ORIGINAL) ? post->filename : post->tim;
  
    if (!filename || !(post->ext)) {
        return NULL;
    }

    char *concat_file_str = malloc(strlen(filename) + strlen(post->ext) + 1);
    if (!concat_file_str) {
        fprintf(stderr, "Error: Could not concatenate tim and ext strings as memory could not be allocated.\n");
        return NULL;
    }

    strcpy(concat_file_str, filename);
    strcat(concat_file_str, post->ext);

    return concat_file_str;
}

static int 
concat_ori_truncated_filename_ext(Post *post, char buffer[])
{ 
    if (!(post->filename) || !(post->ext)) {
        return 0;
    }

    strncpy(buffer, post->filename, MAX_FILENAME_LEN - 6);
    strcat(buffer, post->ext);

    return 1;
}


/* Generates a string representing a point in time from a time_t data type. */
static size_t 
generate_time_string(const time_t t, char buffer[]) 
{
    struct tm *tm_st = localtime(&t);
    return strftime(buffer, MAX_TIME_STR_LEN, "%x - %I:%M%p", tm_st);
}

/* Concatenates two string rep buffers together. */
static void 
concat_str_rep_buffers(StrRepBuffer *s1, StrRepBuffer s2)
{
    check_buffer_dims(s1, s2.curr_str_size);
    memcpy(s1->str_end, s2.buffer_start, s2.curr_str_size);
    s1->str_end += s2.curr_str_size;
    s1->curr_str_size += s2.curr_str_size;   
}

/*
 * This routine is simply append a string to buffer with no regard for a column limit.
 */
void 
append_to_buffer(StrRepBuffer *str_buffer, char *str, int str_len)
{
    check_buffer_dims(str_buffer, str_len);
    memcpy(str_buffer->str_end, str, str_len);

    str_buffer->str_end += str_len;
    str_buffer->curr_str_size += str_len;

    *str_buffer->str_end = 0; // End nul terminator
}

/* 
 * Takes a string and appends to the supplied string representation buffer.
 * The number of columns is controlled by a compile-time constant for now.  
 * In its current form, this function does not strictly enforce the column limit
 * It appends a word at a time until the the column limit is reached. This is to
 * ensure that the output looks nicer.
 */
static void 
append_to_buffer_cols(StrRepBuffer *str_buffer, char *str, int str_len)
{
    check_buffer_dims(str_buffer, str_len + (str_len / MAX_NUM_COLS + 1));

    char *tail_ptr = str_buffer->str_end;
    int *used_cols = &str_buffer->used_cols;
    char *word_begin = str;
    char *p;
    int word_len;
    for (p = str; p < str + str_len; p++) {
        char c = *p;
        (*used_cols)++;

        /* 
         * If we find a word saturating the column limit or a newline character, 
         * copy it into the buffer 
         */
        if ((c == '\n') || ((c == ' ') && (*used_cols >= MAX_NUM_COLS))) {
            word_len = (p - word_begin) + 1;
            memcpy(tail_ptr, word_begin, word_len);
            tail_ptr += word_len;
            /* If we stopped at the end of a word, we start a new line. 
             * Note that this doesn't take into account a super long string with no spaces.
             * In that case, we just have to print the whole thing out on one line, which sucks.
             */
            if (c == ' ') {
                *(tail_ptr - 1) = '\n';              
            }
            word_begin += word_len;
            *used_cols = 0;
        }
    }

    /* Flush the remaining characters into the buffer */
    word_len = p - word_begin;
    memcpy(tail_ptr, word_begin, word_len);
    tail_ptr += word_len;

    if (*used_cols >= MAX_NUM_COLS) {
        *tail_ptr++ = '\n';
        *used_cols = 0;
    }

    str_buffer->curr_str_size += (tail_ptr - str_buffer->str_end);  
    str_buffer->str_end = tail_ptr;
   *str_buffer->str_end = 0; //Append nul character.      
}

/* Append a divider to the text file represented by the supplied string representaion buffer. */
static void 
flush_divider_to_str_rep_buffer(StrRepBuffer *str_buffer)
{
    check_buffer_dims(str_buffer, MAX_NUM_COLS);

    char *tail_ptr = str_buffer->str_end;
    *tail_ptr = *(tail_ptr + MAX_NUM_COLS - 1) = '\n'; //Insert the divider into the buffer.
    memset(tail_ptr + 1, '-', MAX_NUM_COLS - 2);

    str_buffer->str_end += MAX_NUM_COLS; //Update the end-of-string pointer and the size of the string.
    str_buffer->curr_str_size += MAX_NUM_COLS;
}

static void 
check_buffer_dims(StrRepBuffer *str_buffer, int str_len) 
{
    if (str_buffer->curr_str_size + (str_len + 1) > str_buffer->buffer_size) {
        int new_buffer_size = str_buffer->buffer_size + (str_len + 1) + 100;

        char *new_ptr = realloc(str_buffer->buffer_start, new_buffer_size);
        if (!new_ptr) {
            fprintf(stderr, "Error: Cannot reallocate new buffer or StrRepBuffer");
            return;
        }

        str_buffer->buffer_start = new_ptr;
        str_buffer->str_end = str_buffer->buffer_start + str_buffer->curr_str_size;
        str_buffer->buffer_size = new_buffer_size;
    } 
}

/* An auxiliary function used to generate post and thread text files. 
 *  This is used to append a formatted string to a string representation buffer.
 */
static void 
append_to_buffer_formatted(StrRepBuffer *str_buffer, char *str_formatter, char *str) 
{
    if(!str) return;
    size_t size_of_str = snprintf(NULL, 0, str_formatter, str);

    char *formatted_str = malloc(size_of_str + 1);
    if (!formatted_str) {
        fprintf(stderr, "Error: Could not allocate memory for formatted output string");
        return;
    }

    sprintf(formatted_str, str_formatter, str);
    append_to_buffer_cols(str_buffer, formatted_str, size_of_str);
    str_buffer->used_cols = 0; //Reset the column counter.
    free(formatted_str);
}



