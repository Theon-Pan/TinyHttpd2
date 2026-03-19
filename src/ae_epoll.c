#include <sys/epoll.h>
#include "anet.h"

typedef struct aeApiState
{
    int epfd;
    struct epoll_event *events;
} aeApiState;

static int aeApiCreate(aeEventLoop *eventLoop)
{
    aeApiState *state = malloc(sizeof(aeApiState));
    if (!state) return -1;
    state->events = malloc(sizeof(struct epoll_event) * eventLoop->setsize);
    if (!state->events)
    {
        free(state);
        return -1;
    }
    state->epfd = epoll_create(1024); /* 1024 is just a hint for the kernel */
    if (state->epfd == -1)
    {
        free(state->events);
        free(state);
        return -1;
    }
    anetCloexec(state->epfd);
    eventLoop->apidata = state;
    return 0;
}

static int aeApiFree(aeEventLoop *eventLoop)
{
    aeApiState *state = eventLoop->apidata;

    if (state)
    {
        if (state->epfd)
        {
            close(state->epfd);
        }
        if (state->events)
        {
            free(state->events);
        }

        free(state);
    }
}

static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask)
{
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0};

    /* If the fd was already monitored for some event, we need a MOD
     * operation. Otherwise we need an ADD operation. */
    int op = eventLoop->events[fd].mask = AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    ee.events = 0;
    mask |= eventLoop->events[fd].mask; /* Merge old events. */
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;  /* We use fd num as events' index, so we save it in struct epoll_event. */
    if (epoll_ctl(state->epfd, op, fd, &ee) == -1) return -1;
    
    return 0;
}

static int aeApiDelEvent(aeEventLoop *eventLoop, int fd, int mask)
{
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0};

    /* We rely on the fact taht our caller has already updated the mask in the eventLoop */
    mask = eventLoop->events[fd].mask;

    ee.events = 0;
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if (mask != AE_NONE)
    {
        epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &ee);
    }
    else
    {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL */
        epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}

