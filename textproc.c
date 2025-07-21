#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

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

    for (int i = 1; i < thread.num_of_replies; i++) {
        StrRepBuffer reply_str_buffer = generate_post_str_rep(replies + i);
        concat_str_rep_buffers(&thread_str_buffer, reply_str_buffer);
        free_str_rep_buffer(reply_str_buffer);
        flush_divider_to_str_rep_buffer(&thread_str_buffer);
    }

    return thread_str_buffer;
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
    append_to_buffer_formatted(&buffer, "\n\n%s\n", post->com);
 
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

    return (StrRepBuffer) {buffer_size, 0, buffer_start, buffer_start};
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

/* Takes a string and appends to the supplied string representation buffer.
 * The number of columns is controlled by a compile-time constant for now. 
 */
static void 
append_to_buffer_cols(StrRepBuffer *str_buffer, char *str, int str_len)
{
    check_buffer_dims(str_buffer, str_len + (str_len / MAX_NUM_COLS + 1));

    char *tail_ptr = str_buffer->str_end;

     /* We do not consider the nul character at the end of str. The nul character will be appended at the end. */
    char *start_mark = str;
    int char_remaining = str_len;
    while (char_remaining >= MAX_NUM_COLS) {
	    memcpy(tail_ptr, start_mark, MAX_NUM_COLS);

	    tail_ptr += MAX_NUM_COLS;
	    start_mark += MAX_NUM_COLS;
       *tail_ptr++ = '\n';

	    char_remaining -= MAX_NUM_COLS;
    }  
    
    /* Copy the rest of the string into buffer */
    memcpy(tail_ptr, start_mark, char_remaining);
    tail_ptr += char_remaining;

    str_buffer->curr_str_size += (tail_ptr - str_buffer->str_end);  
    str_buffer->str_end = tail_ptr;
   *str_buffer->str_end = '\0'; //Append nul character.      
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
    if (str_buffer->curr_str_size + str_len + 1 > str_buffer->buffer_size) {
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
    }

    sprintf(formatted_str, str_formatter, str);
    append_to_buffer_cols(str_buffer, formatted_str, size_of_str);
    free(formatted_str);
}



