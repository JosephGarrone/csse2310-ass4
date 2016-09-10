/*
 *
 * Joseph Garrone
 * 43541984
 *
 * Assignment 4 Structs
 *
 * */

#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>

/* Structs */
typedef struct Station Station;
typedef struct Connection Connection;
typedef struct Train Train;
typedef struct Resource Resource;
typedef struct ParseInfo ParseInfo;
typedef struct LogInfo LogInfo;
typedef struct ConnInfo ConnInfo;

typedef enum {
    DOOM_TRAIN,
    STOP_TRAIN,
    ADD_TRAIN,
    RESOURCE_TRAIN
} TrainType;

typedef enum {
    STATION,
    DETERMINE_TYPE,
    DOOM,
    STOP,
    RESOURCE,
    ADD,
    FORWARD,
    END
} Section;

struct Station {
    char *name;

    pthread_t sigHupHandler;

    bool allGood;
    bool exiting;

    bool mustStop;
    bool isDoomed;

    FILE *auth;
    char *secret;

    FILE *log;
    char *logFile;

    char *host;
    char *port;
    int actualPort;
    int socket;

    int processed;
    int notMine;
    int formatErr;
    int noFwd;

    Resource *lastRes;
    Connection *lastConn;

    bool error;
    int exitCode;
};

struct Connection {
    int socket;
    char *hostname;
    int port;

    char *name;

    pthread_t thread;

    Connection *prev;
    Connection *next;

    Station *station;

    bool authed;
    bool ready;
    bool wasAdded;

    FILE *read;
    FILE *write;
};

struct Train {
    Resource *lastRes;
    ConnInfo *lastInfo;

    char *host;
    char *port;
    char *forward;
    char *forwardTo;

    bool hasForward;
    bool valid;

    TrainType type;
};

struct Resource {
    Resource *prev;
    Resource *next;

    char *name;

    int quantity;
};

struct ParseInfo {
    char *input;

    int pos;
    int len;

    bool err;
    bool ranOutOfChars;
    bool badForward;

    Section section;
    Train *train;
    Connection *conn;
};

struct LogInfo {
    int notMine;
    int formatErr;
    int noFwd;
    int processed;

    Station *station;
};

struct ConnInfo {
    ConnInfo *prev;
    ConnInfo *next;

    char *host;
    char *port;
};

#endif
