#include "ae.h"
#include "serverassert.h"

#define AE_LOCK(eventLoop)                                              \
    if ((eventLoop)->flags & AE_PROTECT_POLL) {                         \
        assert(pthread_mutex_lock(&(eventLoop)->poll_mutex) == 0);      \
    }

#define AE_UNLOCK(eventLoop)                                            \
    if ((eventLoop)->flags & AE_PROTECT_POLL) {                         \
        assert(pthread_mutex_unlock(&(eventLoop)->poll_mutex) == 0)    \
    }

