#include "ae.h"
#include "serverassert.h"
#include "globalsettings.h"

#include <pthread.h>
#include <stdlib.h>

#ifdef HAVE_EPOLL
#include "ae_epoll.c"
#endif

#define AE_LOCK(eventLoop)                                              \
    if ((eventLoop)->flags & AE_PROTECT_POLL) {                         \
        assert(pthread_mutex_lock(&(eventLoop)->poll_mutex) == 0);      \
    }

#define AE_UNLOCK(eventLoop)                                            \
    if ((eventLoop)->flags & AE_PROTECT_POLL) {                         \
        assert(pthread_mutex_unlock(&(eventLoop)->poll_mutex) == 0)    \
    }

aeEventLoop *aeCreateEventLoop(int setsize)
{
    aeEventLoop *eventLoop;
    int i;

    if ((eventLoop = malloc(sizeof(aeEventLoop))) == NULL) goto err;
    eventLoop->events = malloc(sizeof(aeFileEvent) * setsize);
    eventLoop->fired = malloc(sizeof(aeFiredEvent) * setsize);
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