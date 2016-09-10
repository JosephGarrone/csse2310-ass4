/* 
 * Joseph Garrone
 * 43541984
 *
 * Assignment 4 Utilities implementation
 *
 * */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

/**
 * Consumes the rest of the stream until EOF and checks to see if it is empty
 *
 * Returns a bool indicating whether it was empty or not
 * */
bool is_empty_to_eof(FILE *stream) {
    char chr = fgetc(stream);
    int read = 0;

    while (chr != EOF) {
        chr = fgetc(stream);
        read++;
    }

    return (read > 0) ? false : true;
}

/**
 * Reads a stream until a newline or eof is encountered,
 * at which point it ceases reading.
 * 
 * Returns a line from a file, and sets the err value to EOF
 * if it is encountered.
 * */
char *read_to_eoln(FILE *stream, int *err) {
    int size = BUFFER_SIZE;
    int read = 0;
    char chr;
    char *buffer = (char*) malloc(sizeof(char) * size);

    chr = fgetc(stream);
    while (chr != EOF && chr != '\n') {
        /* The -2 allows for \0 at end of buffer */  
        if (read == size - 2) {
            size *= 2;
            buffer = (char*) realloc(buffer, size);
        }
        buffer[read++] = chr;
        chr = fgetc(stream);
    }

    *err = 0;
    if (chr == EOF) {
        *err = EOF;
    }

    buffer[read] = '\0';
    
    return buffer;
}

/** 
 * Matches the start of two strings
 *
 * Returns bool indicating whether the starts are matching
 * */
bool str_start_match(char *base, char *cmp) {
    if (strlen(base) > strlen(cmp)) {
        return false;
    }

    for (int i = 0; i < strlen(base); i++) {
        if (base[i] != cmp[i]) {
            return false;
        }
    }

    return true;
}

/**
 * Matches completely a string and check for len match as well
 *
 * Returns bool indicating whether they are identical matches
 * */
bool str_match(char *base, char *cmp) {
    if (strlen(base) == strlen(cmp) && str_start_match(base, cmp)) {
        return true;
    }

    return false;
}

/**
 * Converts an int to a nul terminated string
 *
 * Returns int as a nul terminated string
 * */
char *int_to_str(int num) {
    int length = 1;
    int copy = num;

    while (copy /= 10) {
        length++;
    }

    char *result = (char*) malloc(sizeof(char) * length);

    sprintf(result, "%d", num);

    return result;
}

/**
 * Checks whether a char is in the ascii num range
 *
 * Returns bool indicating whether the char is a number
 * */
bool is_num(char test) {
    return (test >= '0' && test <= '9');
}


/**
 * Writes a string to a file and then flushes
 *
 * Returns nothing
 * */
void write_flush(FILE *dest, const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfprintf(dest, format, args);
    fflush(dest);

    va_end(args);
}









