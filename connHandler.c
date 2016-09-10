/**
 *
 * Joseph Garrone
 * 43541984
 *
 * Assignment 4 Connection Handler Implementation
 *
 * */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <netdb.h>
#include <stdarg.h>

#include "structs.h"
#include "connHandler.h"
#include "utils.h"
#include "station.h"

/**
 * Handles incoming messages from a connection
 *
 * Returns a pointer to NULL
 * */
void *handle_connection(void *newConnection) {
    Connection *connection = (Connection*) newConnection;
   
    int err = 0;
    char *line;
    while (err != EOF) {
        line = read_to_eoln(connection->read, &err);

        if (connection->station->isDoomed) {
            continue;
        }

        if (strlen(line) == 0 && err == EOF) {
            break;
        }

        if (connection->authed) {
            handle_input(connection, line);
        } else {
            authenticate(connection, line);
        }

        if (!connection->authed) {
            break;
        }
    }

    fclose(connection->read);
    fclose(connection->write);

    connection->authed = false;
    connection->ready = false;

    return NULL;
}

/**
 * Validates and performs necessary actions on the input
 *
 * Returns nothing
 * */
void handle_input(Connection *conn, char *input) {
    if (!conn->ready) {
        if (!conn->wasAdded) {
            write_flush(conn->write, "%s\n", conn->station->name);
        }
        if (!conn_set_name(conn, input)) {
            station_set_exit(conn->station, CONFLICTING_STATION);
            graceful_exit();
        }
    } else {
        process_info(conn->station, parse_input(input, conn));
    }
}

/**
 * Parses the input for a train and checks validity
 *
 * Returns a train, CHOOO CHOOO
 * */
ParseInfo *parse_input(char *input, Connection *conn) {
    Train *train = new_train();
    Station *station = conn->station;

    ParseInfo *info = new_parse_info(input, train, conn);

    while (!info->err && info->section != END) {
        switch (info->section) {
            case STATION:
                parse_station(station, info);
                break;
            case DETERMINE_TYPE:
                parse_type(station, info);
                break;
            case DOOM:
                parse_doomtrain(station, info);
                break;
            case STOP:
                parse_stoptrain(station, info);
                break;
            case RESOURCE:
                parse_resource(station, info);
                break;
            case ADD:
                parse_add(station, info);
                break;
            case FORWARD:
                parse_forward(station, info);
                break;
            default:
                info->err = true;
                break;
        }
    }

    return info;
}

/**
 * Processes a ParseInfo object and performs all neccessary actions
 *
 * Returns nothing
 * */
void process_info(Station *station, ParseInfo *info) {
    if (!info->err) {
        increase_processed(station);
        switch (info->train->type) {
            case DOOM_TRAIN:
                handle_doom_train(station, info);
                break;
            case STOP_TRAIN:
                handle_stop_train(station, info);
                break; 
            case RESOURCE_TRAIN:
                handle_resource_train(station, info);
                break;
            case ADD_TRAIN:
                handle_add_train(station, info);
                break;
            default:
                break;
        }

        if (info->train->hasForward && info->train->type != STOP_TRAIN) {
            forward_train(station, info);
        }
    }

    /*LogInfo *log = get_log_info(station);
    printf("NotMine: %d, FormatErr: %d, NoFwd: %d, Processed: %d, Fwd: %s\n", 
            log->notMine, log->formatErr, log->noFwd, log->processed, 
            info->train->forward);
    free(log);*/

    free_parse_info(info);
}

/**
 * Handles the sending of doomtrain when it is received
 *
 * Returns nothing
 * */
void handle_doom_train(Station *station, ParseInfo *info) {
    lock_edit_conn();
    
    station->isDoomed = true;

    Connection *conn = station->lastConn;
    for (; conn != NULL; conn = conn->prev) {
        if (conn->authed && conn->ready) {
            write_flush(conn->write, "%s:doomtrain\n", conn->name);
        }
    }

    unlock_edit_conn(); 

    station_set_exit(station, NORMAL_EXIT);
    graceful_exit();
}

/**
 * Handles the shutdown of the station when a stop train is received
 *
 * Returns nothing
 * */
void handle_stop_train(Station *station, ParseInfo *info) {
    forward_train(station, info);

    station->mustStop = true;

    station_set_exit(station, NORMAL_EXIT);
    graceful_exit();
}

/**
 * Handles the adding and removing of resources when a resource train arrives
 *
 * Returns nothing
 * */
void handle_resource_train(Station *station, ParseInfo *info) {
    add_resources(station, info->train->lastRes);
}

/**
 * Handles the adding of new station when an add train arrives
 *
 * Returns nothing
 * */
void handle_add_train(Station *station, ParseInfo *info) {
    ConnInfo *conn = info->train->lastInfo;
    for (; conn != NULL; conn = conn->prev) {
        attempt_connect(station, conn->port, conn->host);
    }
}

/**
 * Attempts to read a char from an info object
 *
 * Returns the char that was read
 * */
char get_char(ParseInfo *info) {
    if (info->pos >= info->len) {
        info->ranOutOfChars = true;
        return EOF;
    }

    return info->input[info->pos++];
}

/**
 * Gets the next char in the input string
 *
 * Returns the next char
 * */
char peek(ParseInfo *info) {
    if (info->pos >= info->len) {
        return EOF;
    }

    return info->input[info->pos];
}

/**
 * Gets the previous char in the input string
 *
 * Returns the previous char
 * */
char last(ParseInfo *info) {
    if (info->pos <= 0) {
        return EOF;
    }

    return info->input[info->pos - 1];
}

/**
 * Attempts to parse a station name
 *
 * Returns nothing
 * */
void parse_station(Station *station, ParseInfo *info) {
    char chr = get_char(info);

    if (!str_start_match(station->name, info->input)) {
        info->err = true;
        increase_not_mine(station);
        return;
    }

    while (chr != ':') {
        if (info->ranOutOfChars || !valid_name_char(chr)) {
            info->err = true;
            increase_not_mine(station);
            return;
        }

        chr = get_char(info);
    }

    if (strlen(station->name) != info->pos - 1) {
        info->err = true;
        increase_not_mine(station);
        return;
    }

    info->section = DETERMINE_TYPE;
}


/**
 * Attempts to determine the type of the message
 *
 * Returns nothing
 * */
void parse_type(Station *station, ParseInfo *info) {
    char chr = get_char(info);
    int pos = 0;
    char *msg = (char*) malloc(sizeof(char) * info->len);

    while (valid_name_char(chr) && chr != EOF) {
        msg[pos++] = chr;
        chr = get_char(info);
    }
    msg[pos] = '\0';
    
    if (str_match("doomtrain", msg) && (chr == EOF || chr == ':')) {
        info->train->type = DOOM_TRAIN;
        info->section = DOOM;
        return;
    }
  
    if (str_start_match("stopstation", msg) && (chr == EOF || chr == ':')) {
        info->train->type = STOP_TRAIN;
        info->section = STOP;
        return;
    }

    if (str_match("add", msg) && (chr == '(')) {
        info->train->type = ADD_TRAIN;
        info->section = ADD;
        return;
    }

    // If this isnt a resource at this stage, then that train is no good
    if (!valid_op(chr)) {
        info->err = true;
        increase_format_err(station);
        return;
    }

    // Reset the string pos to back after the first colon
    info->pos = strlen(station->name) + 1;
    info->train->type = RESOURCE_TRAIN;
    info->section = RESOURCE;
}

/**
 * Attempts to parse a doomtrain
 *
 * Returns nothing
 * */
void parse_doomtrain(Station *station, ParseInfo *info) {
    info->section = END;
}

/**
 * Attempts to parse a stoptrain
 *
 * Returns nothing
 * */
void parse_stoptrain(Station *station, ParseInfo *info) {
    if (last(info) == ':') {
        info->section = FORWARD;
    } else {
        info->section = END;
    }
}

/**
 * Attempts to parse a resource
 *
 * Returns nothing
 * */
void parse_resource(Station *station, ParseInfo *info) {
    char chr = 0, changed[info->len];
    int pos = 0, amount = 0;
    char *err, *resName = (char*) malloc(sizeof(char) * info->len);
    
    info->err = true;

    while (chr == 0 || valid_name_char(chr)) {
        if (info->ranOutOfChars) {
            increase_format_err(station);
            return;
        }
    
        if (chr != 0) {
            resName[pos++] = chr;
        }
        chr = get_char(info);
    }
    resName[pos] = '\0';

    pos = 0;
    if (valid_op(chr) && is_num(peek(info)) && strlen(resName) > 0) {
        changed[pos++] = chr;
        while (is_num(chr = get_char(info))) {
            changed[pos++] = chr;
        }
        changed[pos] = '\0';

        if (chr == ':') {
            info->section = FORWARD;
        } else if (chr == EOF) {
            info->section = END;
        } else if (chr != ',') {
            increase_format_err(station);
            return;
        }

        amount = strtol(changed, &err, BASE_TEN);
    } else {
        increase_format_err(station);
        return;
    }
    
    Resource *res = new_resource(resName, amount, info->train->lastRes);
    info->train->lastRes = res;
    info->err = false;
}

/**
 * Attempts to parse the message for stations to add
 *
 * Returns nothing
 * */
void parse_add(Station *station, ParseInfo *info) {
    char chr = get_char(info);
    int pos = 0;
    char *host = (char*) malloc(sizeof(char) * info->len);
    char *port = (char*) malloc(sizeof(char) * info->len);
    
    info->err = true;
    while (chr != '@') {
        if (info->ranOutOfChars || !is_num(chr)) {
            increase_format_err(station);
            return;
        }

        port[pos++] = chr;
        chr = get_char(info);
    }
    port[pos] = '\0';
    
    pos = 0;
    if (valid_name_char(peek(info)) && strlen(port) > 0) {
        while (valid_host_char((chr = get_char(info)))) {
            host[pos++] = chr;
        }
    } else {
        increase_format_err(station);
        return;
    }
    host[pos] = '\0';

    if (chr == ')' && strlen(host) > 0) {
        if (peek(info) == ':') {
            get_char(info); // Consume the : character
            info->section = FORWARD;
        } else if (peek(info) == EOF) {
            info->section = END;
        } else {
            increase_format_err(station);
            return;
        }
    } else if (chr == ',' && strlen(host) > 0) {
        info->section = ADD;
    } else {
        increase_format_err(station);
        return;
    }

    ConnInfo *connInfo = new_conn_info(port, host, info->train->lastInfo);
    info->train->lastInfo = connInfo;
    info->err = false;
}

/**
 * Attempts to parse the info for forwarding the train on
 *
 * Returns nothing
 * */
void parse_forward(Station *station, ParseInfo *info) {
    char chr = get_char(info);
    char *forward = (char*) malloc(sizeof(char) * info->len);
    int pos = 0;
    info->train->forward = &(info->input[info->pos - 1]);

    while (chr != ':' && !info->ranOutOfChars) {
        if (!valid_name_char(chr)) {
            info->badForward = true;
            increase_no_forward(station);
            break;
        }

        forward[pos++] = chr;

        chr = get_char(info);
    }
    forward[pos] = '\0';

    if (strlen(forward) == 0) {
        info->badForward = true;
        increase_no_forward(station);
        return;
    }

    info->train->forwardTo = forward;
    info->train->hasForward = true;
    info->section = END;
}

/**
 * Attempts to forward the message on to the next train specified
 *
 * Returns nothing
 * */
void forward_train(Station *station, ParseInfo *info) {
    if (!info->badForward && info->train->hasForward) {
        lock_edit_conn();

        bool sent = false;
        Train *train = info->train;
        Connection *conn = station->lastConn;
        for (; conn != NULL; conn = conn->prev) {
            if (conn->authed && conn->ready) {
                if (str_match(conn->name, train->forwardTo)) {
                    write_flush(conn->write, "%s\n", train->forward);
                    sent = true;
                    break;
                }
            }
        }

        if (!sent) {
            increase_no_forward(station);
        }

        unlock_edit_conn(); 
    }
}

/**
 * Checks whether the specified char is a valid name char
 *
 * Returns bool indicating whether the char is valid
 * */
bool valid_name_char(char test) {
    char invalid[7] = {':', '+', '-', '\n', '\0', '(', ')'};
    int len = (int) (sizeof(invalid) / sizeof(invalid[0]));
    
    for (int i = 0; i < len; i++) {
        if (invalid[i] == test) {
            return false;
        }
    }

    return true;
}

/**
 * Checks whether the specified char is a valid host char
 *
 * Returns bool indicating whether the char is valid
 * */
bool valid_host_char(char test) {
    char invalid[9] = {':', '+', '-', '\n', '\0', '(', ')', ',', '@'};
    int len = (int) (sizeof(invalid) / sizeof(invalid[0]));
    
    for (int i = 0; i < len; i++) {
        if (invalid[i] == test) {
            return false;
        }
    }

    return true;
}

/**
 * Checks whether the specified char is a valid resource operation
 *
 * Returns bool indicating whether the char is valid
 * */
bool valid_op(char test) {
    if (test == '+' || test == '-') {
        return true;
    }

    return false;
}

/**
 * Attempts to authenticate the connection with the supplied secret
 *
 * Returns nothing
 * */
void authenticate(Connection *connection, char *secret) {
    if (str_match(connection->station->secret, secret)) {
        connection->authed = true;
    } else {
        fclose(connection->write);
        fclose(connection->read);
        connection->authed = false;
        connection->ready = false;
    }
}

/**
 * Creates a new train
 *
 * Returns the new train
 * */
Train *new_train() {
    Train *train = (Train*) malloc(sizeof(Train));

    
    train->valid = false;
    train->forward = "No Train";
    train->lastRes = NULL;
    train->lastInfo = NULL;
    train->hasForward = false;

    return train;
}

/**
 * Creates a new resource
 *
 * Returns the new resource
 * */
Resource *new_resource(char *name, int quant, Resource *prev) {
    Resource *resource = (Resource*) malloc(sizeof(Resource));

    resource->quantity = quant;
    resource->name = name;
    resource->prev = prev;
    resource->next = NULL;

    return resource;
}

/**
 * Creates a new conn info
 *
 * Returns the new conn info
 * */
ConnInfo *new_conn_info(char *port, char *host, ConnInfo *prev) {
    ConnInfo *info = (ConnInfo*) malloc(sizeof(ConnInfo));

    info->port = port;
    info->host = host;
    info->prev = prev;
    info->next = NULL;

    return info;
}

/**
 * Creates a new parse info
 *
 * Returns the new parse info
 * */
ParseInfo *new_parse_info(char *input, Train *train, Connection *conn) {
    ParseInfo *info = (ParseInfo*) malloc(sizeof(ParseInfo));
    
    info->input = input;
    info->pos = 0;
    info->len = strlen(input);
    info->err = false;
    info->ranOutOfChars = false;
    info->section = STATION;
    info->train = train;
    info->conn = conn; 
    info->badForward = false;

    return info;
}

/**
 * Frees all memory in use by a parseinfo object
 *
 * Returns nothing
 * */
void free_parse_info(ParseInfo *info) {
    free_train(info->train);
    free(info);
}

/**
 * Frees all memory in use by a train object
 *
 * Returns nothing
 * */
void free_train(Train *train) {
    Resource *curRes = train->lastRes;
    while (curRes != NULL) {
        Resource *temp = curRes;
        curRes = curRes->prev;
        free(temp);
    }

    ConnInfo *curInfo = train->lastInfo;
    while (curInfo != NULL) {
        ConnInfo *temp = curInfo;
        curInfo = curInfo->prev;
        free(temp);
    }

    free(train);
}
