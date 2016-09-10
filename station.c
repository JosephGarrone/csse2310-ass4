/*
 *
 * Joseph Garrone
 * 43541984
 *
 * Assignment 4 Station Implementation
 *
 * */

/**
 * READ ME:
 * Just some general information regarding how this program works
 * etc. Might help with speeding up comprehension of how stuff is done
 * 
 * ERRORS:
 * For errors outside main, the station->exitCode is set, and the execution
 * chain bubbles back to the main function.
 *
 * WHATS IN WHAT:
 * utils.c/h - Custom utils that I wrote such as read_to_eoln
 *
 *
 * */

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
#include <semaphore.h>
#include <signal.h>

#include "station.h"
#include "utils.h"
#include "structs.h"
#include "connHandler.h"

/* Global station variable for use in signal handlers */
Station *sigStation;

/* Semaphores */
/* For editing connections */
sem_t editConn;
/* For setting the exit code of the server */
sem_t editExitCode;
/* For editing the log variables */
sem_t editStationLog;
/* For editing the resource list */
sem_t editResources;
/* For logging */
sem_t editLog;

int main(int argc, char **argv) {
    Station *station = new_station();
    sigStation = station;

    init_semaphores();

    if (argc < MIN_ARGS || argc > MAX_ARGS) {
        return trigger_error(INVALID_NUMBER_ARGS); 
    }

    if (!valid_name_and_auth(station, argv[ARG_NAME], argv[ARG_AUTH])) {
        return trigger_error(INVALID_NAME_OR_AUTH); 
    }
  
    if (!valid_log_file(station, argv[ARG_LOG])) {
        return trigger_error(INVALID_LOG_FILE);
    } 

    if (argc > ARG_PORT && !valid_port(station, argv[ARG_PORT])) {
        return trigger_error(INVALID_PORT);
    }

    if (argc > ARG_HOST) {
        station->host = argv[ARG_HOST];
    }

    start_sig_handler(station); 
    start_server(station);

    graceful_exit();

    // Should not be reached as graceful_exit calls exit();
    return station->exitCode;
}

/**
 * Sets up the signal handler
 *
 * Returns nothing
 * */
void start_sig_handler(Station *station) {
    if (pthread_create(&station->sigHupHandler, NULL, log_sig_hup, 
            (void*) station)) {
        station_set_exit(station, SYSTEM_CALL_FAILURE);
        graceful_exit();
    }

    signal(SIGHUP, sig_hup_handler);
}

/**
 * Adds each SIGHUP signal to a queue to be processed
 *
 * Returns nothing
 * */
void sig_hup_handler(int signal) {
    sem_post(&editLog);
}

/**
 * Waits for SIGHUP to be called and logs each time it is
 *
 * Returns a void pointer to NULL
 * */
void *log_sig_hup(void *stationInfo) {
    Station *station = (Station*) stationInfo;
    
    while (1) {
        sem_wait(&editLog);
        update_log(station);
    }
}

/**
 * Updates the log files
 *
 * Returns nothing
 * */
void update_log(Station *station) {
    lock_edit_station_log();
    lock_edit_resources();
    lock_edit_conn();

    fprintf(station->log, "=======\n");
    fprintf(station->log, "%s\n", station->name);
    fprintf(station->log, "Processed: %d\n", station->processed);
    fprintf(station->log, "Not mine: %d\n", station->notMine);
    fprintf(station->log, "Format err: %d\n", station->formatErr);
    fprintf(station->log, "No fwd: %d\n", station->noFwd);

    print_stations(station);
    print_resources(station);

    if (station->mustStop) {
        fprintf(station->log, "stopstation\n");
    } else if (station->isDoomed) {
        fprintf(station->log, "doomtrain\n");
    }

    fflush(station->log);

    unlock_edit_conn();
    unlock_edit_resources();
    unlock_edit_station_log();
}

/**
 * Compares 2 strings using strcmp
 *
 * Returns int indicating relative lexicographic ordering
 * */
int compare_string(const void *a, const void *b) { 
    return strcmp(*((char**) a), *((char**) b));
}

/**
 * Compares 2 resources
 *
 * Returns int indicating relative lexicographic ordering
 * */
int compare_resource(const void *a, const void *b) {
    Resource *resA = *((Resource**) a);
    Resource *resB = *((Resource**) b);
    return strcmp(resA->name, resB->name);
}

/**
 * Prints the connected stations names in lexicographic order
 *
 * Returns nothing
 * */
void print_stations(Station *station) {
    int stations = 0;
    Connection *conn = station->lastConn;
    for (; conn != NULL; conn = conn->prev) {
        if (conn->authed && conn->ready) {
            stations++;
        }
    }

    int pos = 0;
    char *names[stations];
    conn = station->lastConn;
    for (; conn != NULL; conn = conn->prev) {
        if (conn->authed && conn->ready) {
            names[pos++] = conn->name;
        }
    }

    size_t size = sizeof(names) / sizeof(char*);
    qsort(names, size, sizeof(char*), compare_string);

    if (stations == 0) {
        fprintf(station->log, "NONE\n");
        return;
    }

    for (int i = 0; i < stations; i++) {
        if (i) {
            fprintf(station->log, ",");
        }
        fprintf(station->log, "%s", names[i]);
    }
    fprintf(station->log, "\n");
}

/**
 * Prints the resources in lexicographic order
 *
 * Returns nothing
 * */
void print_resources(Station *station) {
    int resources = 0;
    Resource *res = station->lastRes;
    for (; res != NULL; res = res->prev) {
        resources++;
    }

    int pos = 0;
    Resource *allRes[resources];
    res = station->lastRes;
    for (; res != NULL; res = res->prev) {
        allRes[pos++] = res;
    }

    qsort(allRes, (size_t) resources, sizeof(Resource*), compare_resource);

    for (int i = 0; i < resources; i++) {
        res = allRes[i];
        fprintf(station->log, "%s %d\n", res->name, res->quantity);
    }
}

/**
 * Adds resources from one linked list to those stored in the main data
 * 
 * Returns nothing
 * */
void add_resources(Station *station, Resource *last) {
    lock_edit_resources();
    
    for (; last != NULL; last = last->prev) {
        modify_resource(station, last);
    }

    unlock_edit_resources();
}

/**
 * Gets the existing resource pointer or makes a new one if the specified
 * name doesn't exist for a resource (From the specified list)
 *
 * Returns a pointer to a resource
 * */
Resource *modify_resource(Station *station, Resource *search) {
    Resource *temp = station->lastRes;
    for (; temp != NULL; temp = temp->prev) {
        if (str_match(temp->name, search->name)) {
            temp->quantity += search->quantity;
            return temp;
        }
    }

    Resource *resource = new_resource(search->name, search->quantity, 
            station->lastRes);
    station->lastRes = resource;

    return resource;
}

/**
 * Initialise all the semaphores to their default values
 *
 * Returns nothing
 * */
void init_semaphores() {
    sem_init(&editConn, 0, 1);
    sem_init(&editExitCode, 0, 1);
    sem_init(&editStationLog, 0, 1);
    sem_init(&editResources, 0, 1);
    sem_init(&editLog, 0, 0);
}

/**
 * Cleanly exits the program and writes data to logfile
 *
 * NOTE: May be called from other threads
 *
 * Returns nothing
 * */
void graceful_exit() {
    if (sigStation->isDoomed || sigStation->mustStop) {
        update_log(sigStation);
    }
    exit(trigger_error(sigStation->exitCode));
}

/**
 * Creates a new station
 *
 * Returns a pointer to the new station
 * */
Station *new_station() {
    Station *station = (Station*) malloc(sizeof(Station));

    /* Initialise variables to defaults */
    station->exitCode = NORMAL_EXIT;
    station->host = LOCALHOST;
    station->port = DEFAULT_PORT;
    station->allGood = true;
    station->exiting = false;
    station->mustStop = false;
    station->isDoomed = false;
    station->lastConn = NULL;
    station->lastRes = NULL;

    return station;
}

/**
 * Creates a new connection
 *
 * Returns a pointer to the new connection
 * */
Connection *new_connection(Station *station) {
    Connection *connection = (Connection*) malloc(sizeof(Connection));
    
    connection->prev = station->lastConn;
    connection->next = NULL;
    connection->authed = false;
    connection->ready = false;
    connection->station = station;
    connection->wasAdded = false;

    if (station->lastConn != NULL) {
        station->lastConn->next = connection;
    }

    return connection;
}

/**
 * Prints the error message that matches the exitCode to stderr
 *
 * Returns the exit code that was passed in
 * */
ExitCode trigger_error(ExitCode exitCode) {
    char *msg;

    switch (exitCode) {
        case NORMAL_EXIT:
            msg = NORMAL_EXIT_MSG;
            break;
        case INVALID_NUMBER_ARGS:
            msg = INVALID_NUMBER_ARGS_MSG;
            break;
        case INVALID_NAME_OR_AUTH:
            msg = INVALID_NAME_OR_AUTH_MSG;
            break;
        case INVALID_LOG_FILE:
            msg = INVALID_LOG_FILE_MSG;
            break;
        case INVALID_PORT:
            msg = INVALID_PORT_MSG;
            break;
        case LISTEN_ERROR:
            msg = LISTEN_ERROR_MSG;
            break;
        case INVALID_STATION:
            msg = INVALID_STATION_MSG;
            break;
        case CONFLICTING_STATION:
            msg = CONFLICTING_STATION_MSG;
            break;
        case SYSTEM_CALL_FAILURE:
            msg = SYSTEM_CALL_FAILURE_MSG;
            break;
        default:
            msg = SYSTEM_CALL_FAILURE_MSG;
            break;
    }
    
    fprintf(stderr, msg);

    return exitCode;
}

/**
 * Validates the station name and auth file that are supplied from the command
 * line.
 *
 * Returns bool indicating the validity of them
 * */
bool valid_name_and_auth(Station *station, char *name, char *auth) {
    if (str_match(name, "")) {
        return false;
    }
    station->name = name;

    station->auth = fopen(auth, "r");
    if (station->auth == NULL) {
        return false;
    }

    int err = 0;
    char *secret = read_to_eoln(station->auth, &err);
    
    station->secret = (char*) malloc(sizeof(char) * strlen(secret));
    station->secret = secret;
    if (str_match(station->secret, "")) {
        return false;
    }

    return true;
}

/**
 * Validates the port that the station is meant to run on
 *
 * Returns bool indicating the validity of it
 * */
bool valid_port(Station *station, char *requestedPort) {
    char *err;
    int port = strtol(requestedPort, &err, BASE_TEN);

    station->port = requestedPort;

    if (port < MIN_PORT || port > MAX_PORT) {
        return false;
    }

    if (*err != '\0') {
        return false;
    }

    return true;
}

/**
 * Checks that the log file exists
 *
 * Returns bool indicating the validity of the log file
 * */
bool valid_log_file(Station *station, char *log) {
    station->log = fopen(log, "a");

    if (station->log == NULL) {
        return false;
    }

    station->logFile = log;

    return true;
}

/**
 * Start the server and perform all necessary functions
 *
 * Returns nothing
 * */
void start_server(Station *station) {
    if (!start_listening(station)) {
        return;
    }

    if (!finalise_station_start(station)) {
        return;
    }

    while (station->allGood) {
        if (!handle_connections(station)) {
            station->allGood = false;
        }
    }
}

/**
 * Handle all incoming connections and spawn threads to process
 * them individually
 *
 * Returns bool indicating if no errors were encountered
 * */
bool handle_connections(Station *station) { 
    /* Default to error, makes code shorter */
    station->exitCode = SYSTEM_CALL_FAILURE;
    
    char *hostname = (char*) malloc(sizeof(char) * MAX_HOSTNAME_LEN);
    struct sockaddr_in from;
    socklen_t fromSize = sizeof(from);

    int socket = accept(station->socket, (struct sockaddr*)&from, &fromSize);
    if (socket == -1) {
        return false;
    }

    int error = getnameinfo((struct sockaddr*)&from, fromSize, 
            hostname, MAX_HOSTNAME_LEN, NULL, 0, 0);
    if (error) {
        return false;
    }

    lock_edit_conn();

    Connection *connection = new_connection(station);
    connection->hostname = hostname;
    connection->socket = socket;
    connection->port = ntohs(from.sin_port);
    connection->write = fdopen(connection->socket, "w");
    connection->read = fdopen(connection->socket, "r");

    if (pthread_create(&connection->thread, NULL, handle_connection,
            (void *) connection)) {
        return false;
    }

    station->lastConn = connection;
    station->exitCode = NORMAL_EXIT;

    unlock_edit_conn();
    return true;
}

/**
 * Attempts to connect to an add train's specified connection
 *
 * Returns nothing
 * */
void attempt_connect(Station *station, char *port, char *host) {
    struct addrinfo hints, *result, *attempt;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = DEFAULT_PROTOCOL;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    int success = getaddrinfo(host, port, &hints, &result);
    if (success != 0) {
        station_set_exit(station, INVALID_STATION);
        graceful_exit();
    }

    int socketTry;
    /* Iterate through a list of possible conns and find one that works */
    for (attempt = result; attempt != NULL; attempt = attempt->ai_next) {
        socketTry = socket(attempt->ai_family, attempt->ai_socktype, 
                attempt->ai_protocol);

        if (socketTry == -1) {
            continue;
        }

        if (connect(socketTry, attempt->ai_addr, attempt->ai_addrlen) != -1) {
            break;
        }

        close(socketTry);
    }

    if (attempt == NULL) {
        return;
    }

    lock_edit_conn();
    Connection *connection = new_connection(station);
    setup_add_connection(connection, atoi(port), socketTry);
    unlock_edit_conn();

    write_flush(connection->write, "%s\n", station->secret);
    write_flush(connection->write, "%s\n", station->name);

    if (pthread_create(&connection->thread, NULL, handle_connection,
            (void *) connection)) {
        station_set_exit(station, SYSTEM_CALL_FAILURE);
        graceful_exit();
    }
}

/**
 * Function to setup a connection that was added via a train
 *
 * Returns nothing
 * */
void setup_add_connection(Connection *conn, int port, int socket) {
    conn->socket = socket;
    conn->port = port;
    conn->write = fdopen(conn->socket, "w");
    conn->read = fdopen(conn->socket, "r");
    conn->authed = true;
    conn->wasAdded = true;
    conn->station->lastConn = conn;
}

/**
 * Finds the port that the station bound to and prints it to the terminal
 *
 * Returns bool indicating if errors were encountered
 * */
bool finalise_station_start(Station *station) {
    station->exitCode = LISTEN_ERROR;

    struct sockaddr_in socket;
    socklen_t len = sizeof(socket);

    if (getsockname(station->socket, (struct sockaddr *)&socket, &len) == -1) {
        return false;
    }

    station->actualPort = ntohs(socket.sin_port);
    station->port = int_to_str(station->actualPort);

    printf("%s\n", station->port);
    fflush(stdout);

    return true;
}

/**
 * Start listening for incoming connections on the port/address given at
 * program start
 *
 * Returns bool indicating success
 * */
bool start_listening(Station *station) {
    /* Assume failure - makes code shorter */
    station->exitCode = LISTEN_ERROR;

    /* Setup hints for looking for interfaces to bind to */
    struct addrinfo hints, *result, *attempt;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = DEFAULT_PROTOCOL;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    /* Fetch a list of interfaces to bind to*/
    if (getaddrinfo(station->host, station->port, &hints, &result) != 0) {
        return false;
    }

    /* Loop through all available interfaces until we can bind to one */
    for (attempt = result; attempt != NULL; attempt = attempt->ai_next) {
        station->socket = socket(attempt->ai_family, attempt->ai_socktype, 
                attempt->ai_protocol);
        
        if (station->socket == -1) {
            continue; /* Failed to connect on this interface */
        }

        if (bind(station->socket, attempt->ai_addr, 
                attempt->ai_addrlen) == 0) {
            break; /* Successfully bound */
        }

        close(station->socket);
    }

    if (attempt == NULL) {
        return false;
    }

    listen(station->socket, SOMAXCONN);

    station->exitCode = NORMAL_EXIT;
    return true;
}

/**
 * See if the specified station has already been added
 *
 * Returns bool indicating that
 * */
bool station_exists(Station *station, char *name) {
    if (str_match(station->name, name)) {
        return true;
    }

    Connection *conn = station->lastConn;
    for (; conn != NULL; conn = conn->prev) {
        if (conn->ready && str_match(conn->name, name)) {
            return true;
        }
    }

    return false;
}

/**
 * Blocks until a lock can be made on the editConn semaphore
 *
 * Returns nothing
 * */
void lock_edit_conn() {
    sem_wait(&editConn);
}

/**
 * Unlocks the editConn semaphore
 *
 * Returns nothing
 * */
void unlock_edit_conn() {
    sem_post(&editConn);
}

/**
 * Sets the name of a station in a thread safe manner
 *
 * Returns bool indicating whether the name exists already
 * */
bool conn_set_name(Connection *connection, char *name) {
    lock_edit_conn();

    bool result = true;

    if (station_exists(connection->station, name)) {
        result = false;
    }

    if (result) {
        connection->name = name;
        connection->ready = true;
    }

    unlock_edit_conn();

    return result;
}

/**
 * Blocks until a lock can be made on the editExitCode semaphore
 *
 * Returns nothing
 * */
void lock_edit_exit_code() {
    sem_wait(&editExitCode);
}

/**
 * Unlocks the editExitCode semaphore
 *
 * Returns nothing
 * */
void unlock_edit_exit_code() {
    sem_post(&editExitCode);
}


/**
 * Locks the editResources semaphore
 *
 * Returns nothing
 * */
void lock_edit_resources() {
    sem_wait(&editResources);
}

/**
 * Unlocks the editResources semaphore
 *
 * Returns nothing
 * */
void unlock_edit_resources() {
    sem_post(&editResources);
}

/**
 * Sets the exit parameters for the station
 *
 * NOTE: Should only be called if an exit is impending
 *
 * Returns nothing
 * */
void station_set_exit(Station *station, int code) {
    lock_edit_exit_code();

    if (!station->exiting) {
        station->exitCode = code;
        station->exiting = true;
        station->allGood = false;
    }

    unlock_edit_exit_code();
}

/**
 * Blocks until a lock can be made on the edit_station_log semaphore
 *
 * Returns nothing
 * */
void lock_edit_station_log() {
    sem_wait(&editStationLog);
}

/**
 * Unlocks the edit_station_log semaphore
 *
 * Returns nothing
 * */
void unlock_edit_station_log() {
    sem_post(&editStationLog);
}

/**
 * Increases the number of trains who arrived at the wrong station
 *
 * Returns nothing
 * */
void increase_not_mine(Station *station) {
    lock_edit_station_log();

    station->notMine++;

    unlock_edit_station_log();
}

/**
 * Increases the number of trains discarded due to format err
 *
 * Returns nothing
 * */
void increase_format_err(Station *station) {
    lock_edit_station_log();

    station->formatErr++;

    unlock_edit_station_log();
}

/**
 * Increases the number of trains not forwarded
 *
 * Returns nothing
 * */
void increase_no_forward(Station *station) {
    lock_edit_station_log();

    station->noFwd++;

    unlock_edit_station_log();
}

/**
 * Increases the number of trains processed
 *
 * Returns nothing
 * */
void increase_processed(Station *station) {
    lock_edit_station_log();
    
    station->processed++;

    unlock_edit_station_log();
}

/**
 * Fetches all the info required to record an entry in the log
 *
 * Returns a LogInfo containing all the data
 * */
LogInfo *get_log_info(Station *station) {
    lock_edit_station_log();

    LogInfo *info = (LogInfo*) malloc(sizeof(LogInfo));
    info->notMine = station->notMine;
    info->formatErr = station->formatErr;
    info->noFwd = station->noFwd;
    info->processed = station->processed;
    info->station = station;
    
    unlock_edit_station_log();

    return info;
}





