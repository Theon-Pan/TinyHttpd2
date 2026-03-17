#ifndef __AE_H
#define __AE_H
#include <pthread.h>


#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0               /* No events registered. */
#define AE_READABLE 1           /* Fire when descriptor is readable. */
#define AE_WRITABLE 2           /* Fire when descriptor is writable. */
#define AE_BARRIER 4            /* With WRITABLE, never fire the event if the       \
                                   READABLE event already fired in the same event   \
                                   loop iteration. Useful when you want to persist  \
                                   things to disk before sending replies, and want  \
                                   to do that in a group fashion. */

#define AE_FILE_EVENTS (1 << 0)
#define AE_TIME_EVENTS (1 << 1)
#define AE_ALL_EVENTS (AE_FILE_EVENTS | AE_TIME_EVENTS)
#define AE_DONT_WAIT (1 << 2)
#define AE_CALL_BEFORE_SLEEP (1 << 3)
#define AE_CALL_AFTER_SLEEP (1 << 4)
#define AE_PROTECT_POLL (1 << 5)

struct aeEventLoop;

/* Callbacks */
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
typedef long long aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);
typedef void aeAfterSleepProc(struct aeEventLoop *eventLoop, int numevents);
typedef int aeCustomPollProc(struct aeEventLoop *eventLoop);

/* File event structure */
typedef struct aeFileEvent {
    int mask;                       /* One of AE_(READABLE|WRITABLE|BARRIER) */
    aeFileProc *rfileProc;
    aeFileProc *wfileProc;
    void *clientData;
} aeFileEvent;

/* Time event structure */
typedef struct aeTimeEvent {
    long long id;                   /* Time event identifier */
    aeTimeProc *timeProc;
    aeEventFinalizerProc *finalizerProc;
    void *clientData;
    struct aeTimeEvent *prev;
    struct aeTimeEvent *next;
    int refcount;                   /* Refcount to prevent timer events from being
    * freed in recursive time event calls. */
} aeTimeEvent;

/* A fired event */
typedef struct aeFiredEvent {
    int fd;
    int mask;
} aeFiredEvent;

/* State of an event based program */
typedef struct aeEventLoop {
    int maxfd;                      /* Highest file descriptor currently registered */
    int setsize;                    /* Max number of file descriptor tracked */
    long long timeEventNextId;
    aeFileEvent *events;            /* Registered events */
    aeFiredEvent *fired;            /* Fired events */
    aeTimeEvent *timeEventHead;
    int stop;
    void *apidata;                  /* This is used for polling API specific data */
    aeBeforeSleepProc *beforesleep;
    aeAfterSleepProc *aftersleep;
    aeCustomPollProc *custompoll;
    pthread_mutex_t poll_mutex;
    int flags;
} aeEventLoop;

/* Prototypes */
aeEventLoop *aeCreateEventLoop(int setsize);
void aeDeleteEventLoop(aeEventLoop *eventLoop);

#endif