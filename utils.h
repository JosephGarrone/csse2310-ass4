/*
 * Joseph Garrone
 * 43541984
 *
 * Assignment 4 Utilities header
 *
 * */

#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

/* Defines */
#define BUFFER_SIZE 80

/* Function prototypes */
bool is_empty_to_eof(FILE *stream);
char *read_to_eoln(FILE *stream, int *err);
bool str_start_match(char *base, char *cmp);
bool str_match(char *base, char *cmp);
char *int_to_str(int num);
bool is_num(char test);
void write_flush(FILE *dest, const char *format, ...);

#endif
