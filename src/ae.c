#include "ae.h"
#include "serverassert.h"
#include "globalsettings.h"

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef HAVE_EPOLL
#include "ae_epoll.c"
#endif

#define AE_LOCK(eventLoop)                                              \
    if ((eventLoop)->flags & AE_PROTECT_POLL) {                         \
        assert(pthread_mutex_lock(&(eventLoop)->poll_mutex) == 0);      \
    }

#define AE_UNLOCK(eventLoop)                                            \
    if ((eventLoop)->flags & AE_PROTECT_POLL) {                         \
        assert(pthread_mutex_unlock(&(eventLoop)->poll_mutex) == 0);    \
    }

aeEventLoop *aeCreateEventLoop(int setsize)
{
    aeEventLoop *eventLoop;
    int i;

    if ((eventLoop = malloc(sizeof(aeEventLoop))) == NULL) goto err;
    eventLoop->events = calloc(setsize, sizeof(aeFileEvent));
    eventLoop->fired = calloc(setsize, sizeof(aeFiredEvent));
    if(eventLoop->events == NULL || eventLoop->fired == NULL) goto err;
    eventLoop->setsize = setsize;
    eventLoop->timeEventHead = NULL;
    eventLoop->timeEventNextId = 1;
    eventLoop->stop = 0;
    eventLoop->maxfd = -1;
    eventLoop->beforesleep = NULL;
    eventLoop->aftersleep = NULL;
    eventLoop->custompoll = NULL;
    eventLoop->flags = 0;

    /* Initalize the eventloop mutex with PTHREAD_MUTEX_ERRORCHECK type */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (pthread_mutex_init(&eventLoop->poll_mutex, &attr) != 0) goto err;

    if (aeApiCreate(eventLoop) == -1) goto err;
    /* Events with mask == AE_NONE are not set. So let's initalize the
     * vector with it. */
    for (i = 0; i < setsize; i++) eventLoop->events[i].mask = AE_NONE;
    return eventLoop;


err:
    if (eventLoop)
    {
        if (eventLoop->events)
        {
            free(eventLoop->events);
        }
        if (eventLoop->fired)
        {
            free(eventLoop->fired);
        }
        free(eventLoop);
    }
    return NULL;
}

int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask, aeFileProc *proc, void *clientData)
{
    AE_LOCK(eventLoop);
    int ret = AE_ERR;
    if (fd >= eventLoop->setsize)
    {
        errno = ERANGE;
        goto done;
    }
    
    aeFileEvent *fe = &(eventLoop->events[fd]);
    
    if (aeApiAddEvent(eventLoop, fd, mask) == -1) goto done;
    fe->mask |= mask;
    if (mask & AE_READABLE) fe->rfileProc = proc;
    if (mask & AE_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData;
    if (fd > eventLoop->maxfd) eventLoop->maxfd = fd;

    ret = AE_OK;

done:
    AE_UNLOCK(eventLoop);
    return ret;
}

/**
 * Remove fd with the specified event from multiplex, if the event is mismatched,
 * then nothing happened.
 */
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask)
{
    AE_LOCK(eventLoop);
    if (fd > eventLoop->setsize) goto done;

    aeFileEvent *fe = &eventLoop->events[fd];
    if (fe->mask == AE_NONE) goto done;

    /* We want to always remove AE_BARRIER if set when AE_WRITABLE
     * is removed. 
     * @todo: So for I don't understand the purpose of this piece of code, so comment it*/
    // if (mask & AE_WRITABLE) mask |= AE_BARRIER;

    /* Only remove attached events. */
    mask = mask & fe->mask;
    fe->mask = fe->mask & (~mask);
    if (fd == eventLoop->maxfd && fe->mask == AE_NONE)
    {
        /* Update the max fd. */
        int j;
        for (j = eventLoop->maxfd - 1; j >= 0; j--)
            if (eventLoop->events[j].mask != AE_NONE) break;
        eventLoop->maxfd = j;
    }

    /* Check whether there are events to be removed.
     * Note: user may remove the AE_BARRIER without
     * touching the actual events. */
    if (mask & (AE_READABLE | AE_WRITABLE))
    {
        /* Must be invoked after the eventLoop maskis modified,
         * which is required by evport and epoll */
        aeApiDelEvent(eventLoop, fd, mask);
    }

done:
    AE_UNLOCK(eventLoop);
}

int aeProcessEvents(aeEventLoop *eventLoop, int flags)
{
    int processed = 0, numevents;

    /* NULL means infinite wait. */
    struct timeval tv, *tvp = NULL; 

    if (eventLoop->maxfd != -1)
    {

        /* @todo: set 100 milli-second by default, 
                  will change to set the value which is from configure.*/
        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000;
        tvp = &tv;
        /* Call the multiplexing API, will return only on timeout or when
         * some event fires. */
        numevents = aeApiPoll(eventLoop, tvp);

        for (int j = 0; j < numevents; j++)
        {
            int fd = eventLoop->fired[j].fd;
            aeFileEvent *fe = &eventLoop->events[fd];
            int mask = eventLoop->fired[j].mask;
            int fired = 0; /* Number of events fired for current fd. */

            if (fe && fe->rfileProc && (fe->mask & mask & AE_READABLE))
            {
                fe->rfileProc(eventLoop, fd, fe->clientData, mask);
                fired++;
            }
            if (fe && fe->wfileProc && (fe->mask & mask & AE_WRITABLE))
            {
                fe->wfileProc(eventLoop, fd, fe->clientData, mask);
                fired++;
            }
            processed++;
        }
    }

    return processed;
}

void aeMain(aeEventLoop *eventLoop)
{
    eventLoop->stop = 0;
    while (!eventLoop->stop)
    {
        aeProcessEvents(eventLoop, 0);
    }
}