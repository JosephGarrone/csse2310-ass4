/*
 *
 * Joseph Garrone
 * 43541984
 *
 * Assignment 4 Station Header File
 *
 * */

#ifndef STATION_H
#define STATION_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "structs.h"

/* Defines */

/* Misc defines */
#define BASE_TEN 10

/* Network defines */
#define LOCALHOST "127.0.0.1"
#define DEFAULT_PROTOCOL 0
#define MAX_HOSTNAME_LEN 128
#define DEFAULT_PORT "0"

/* Program arg checking */
#define MIN_ARGS 4
#define MAX_ARGS 6
#define ARG_NAME 1
#define ARG_AUTH 2
#define ARG_LOG 3
#define ARG_PORT 4
#define ARG_HOST 5
#define MIN_PORT 1
#define MAX_PORT 65534

/* Error messages */
#define NORMAL_EXIT_MSG ""
#define INVALID_NUMBER_ARGS_MSG "Usage: station name authfile logfile [port" \
        " [host]]\n"
#define INVALID_LOG_FILE_MSG "Unable to open log\n"
#define INVALID_NAME_OR_AUTH_MSG "Invalid name/auth\n"
#define INVALID_PORT_MSG "Invalid port\n"
#define LISTEN_ERROR_MSG "Listen error\n"
#define INVALID_STATION_MSG "Unable to connect to station\n"
#define CONFLICTING_STATION_MSG "Duplicate station names\n"
#define SYSTEM_CALL_FAILURE_MSG "Unspecified system call failure\n"

/* Enums */
typedef enum {
    NORMAL_EXIT = 0,
    INVALID_NUMBER_ARGS = 1,
    INVALID_NAME_OR_AUTH = 2,
    INVALID_LOG_FILE = 3,
    INVALID_PORT = 4,
    LISTEN_ERROR = 5,
    INVALID_STATION = 6,
    CONFLICTING_STATION = 7,
    SYSTEM_CALL_FAILURE = 99
} ExitCode;

/* Function prototypes */
Station *new_station();
Connection *new_connection(Station *station);
void setup_add_connection(Connection *conn, int port, int socket);
ExitCode trigger_error(ExitCode exitCode);
bool valid_name_and_auth(Station *station, char *name, char *auth);
bool valid_port(Station *station, char *port);
bool valid_log_file(Station *station, char *log);
void start_server(Station *station);
bool handle_connections(Station *station);
bool start_listening(Station *station);
void attempt_connect(Station *station, char *port, char *host);
bool finalise_station_start(Station *station);
bool station_exists(Station *station, char *name);
void graceful_exit();
void init_semaphores();
void sig_hup_handler(int signal);
void *log_sig_hup(void *stationInfo);
void start_sig_handler(Station *station);
void add_resources(Station *station, Resource *last);
Resource *modify_resource(Station *station, Resource *search);

/* Log generation functions */
void update_log(Station *station);
void print_stations(Station *station);
void print_resources(Station *station);
int compare_string(const void *a, const void *b);
int compare_resource(const void *a, const void *b);

/* Semaphore based functions */
void lock_edit_conn();
void unlock_edit_conn();
bool conn_set_name(Connection *connection, char *name);
void lock_edit_exit_code();
void unlock_edit_exit_code();
void station_set_exit(Station *station, int code);
void lock_edit_station_log();
void unlock_edit_station_log();
void lock_edit_resources();
void unlock_edit_resources();
void increase_not_mine(Station *station);
void increase_format_err(Station *station);
void increase_no_forward(Station *station);
void increase_processed(Station *station);
LogInfo *get_log_info(Station *station);

#endif




