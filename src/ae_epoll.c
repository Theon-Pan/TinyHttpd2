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