/**
 *
 * Joseph Garrone
 * 43541984
 *
 * Assignment 4 Connection Handler Header
 *
 * */

#ifndef CONN_HANDLER_H
#define CONN_HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <netdb.h>
#include <stdarg.h>

#include "structs.h"

/* Prototypes */

/* Base functions */
void *handle_connection(void *newConnection);
void handle_input(Connection *connection, char *input);
void authenticate(Connection *connection, char *secret);
void process_info(Station *station, ParseInfo *info);
void forward_train(Station *station, ParseInfo *info);
ParseInfo *parse_input(char *input, Connection *conn);

/* Parser Helpers */
char get_char(ParseInfo *info);
char peek(ParseInfo *info);
char last(ParseInfo *info);

/* Parser sections */
void parse_station(Station *station, ParseInfo *info);
void parse_type(Station *station, ParseInfo *info);
void parse_doomtrain(Station *station, ParseInfo *info);
void parse_stoptrain(Station *station, ParseInfo *info);
void parse_resource(Station *station, ParseInfo *info);
void parse_add(Station *station, ParseInfo *info);
void parse_forward(Station *station, ParseInfo *info);

/* Validation */
bool valid_name_char(char test);
bool valid_host_char(char test);
bool valid_op(char test);

/* Handlers */
void handle_doom_train(Station *station, ParseInfo *info);
void handle_stop_train(Station *station, ParseInfo *info);
void handle_resource_train(Station *station, ParseInfo *info);
void handle_add_train(Station *station, ParseInfo *info);

/* Constructors */
Train *new_train();
Resource *new_resource(char *name, int quant, Resource *prev);
ParseInfo *new_parse_info(char *input, Train *train, Connection *conn);
ConnInfo *new_conn_info(char *port, char *host, ConnInfo *prev);

/* Deconstructors */
void free_parse_info(ParseInfo *info);
void free_train(Train *train);
#endif
