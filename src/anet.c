#include "anet.h"

#include <fcntl.h>
#include <errno.h>
/**
 * Enable the FD_CLOEXEC on the given fd to avoid fd leaks.
 * This function should be invoked for fd's on specific palces
 * where fork + execve system calls are called.
 */
int anetCloexec(int fd)
{
    int r;
    int flags;

    do {
        r = fcntl(fd, F_GETFD);
    } while(r == -1 && errno == EINTR);

    if (r == -1 || (r & FD_CLOEXEC)) return r;

    flags = r | FD_CLOEXEC;

    do {
        r = fcntl(fd, F_SETFD, flags);
    } while (r == -1 && errno == EINTR);

    return r;
}