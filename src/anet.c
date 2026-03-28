#include "anet.h"
#include "serverassert.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <unistd.h>
#include <grp.h>


/* XXX: Until glibc 2.41, getaddrinfo with hints.ai_protocol of IPPROTO_MPTCP leads error.
 * Use hints.ai_protocol IPPROTO_IP (0) or IPPROTO_TCP (6) to resolve address and overwrite
 * it when MPTCP is enabled.
 * Ref: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/tools/testing/selftests/net/mptcp/mptcp_connect.c
 *      https://sourceware.org/git/?p=glibc.git;a=commit;h=a8e9022e0f829d44a818c642fc85b3bfbd26a514
 */
static int anetTcpGetProtocol(int is_mptcp_enabled) {
#ifdef IPPROTO_MPTCP
    return is_mptcp_enabled ? IPPROTO_MPTCP : IPPROTO_TCP;
#else
    assert(!is_mptcp_enabled);
    return IPPROTO_TCP;
#endif
}

static void anetSetError(char *err, const char *fmt, ...) {
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, ANET_ERR_LEN, fmt, ap);
    va_end(ap);
}


static int anetV6Only(char *err, int s) {
    int yes = 1;
    if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

static int anetSetReuseAddr(char *err, int fd) {
    int yes = 1;
    /* Make sure connection-intensive things like the benchmark tool
     * will be able to close/open sockets a zillion of times. */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

static int anetListen(char *err, int s, struct sockaddr *sa, socklen_t len, int backlog, mode_t perm, char *group) {
    if (bind(s, sa, len) == -1) {
        anetSetError(err, "bind: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }

    if (sa->sa_family == AF_LOCAL && perm) chmod(((struct sockaddr_un *)sa)->sun_path, perm);
    if (sa->sa_family == AF_LOCAL && group != NULL) {
        struct group *grp;
        if ((grp = getgrnam(group)) == NULL) {
            anetSetError(err, "getgrnam error for group '%s':%s", group, strerror(errno));
            close(s);
            return ANET_ERR;
        }

        /* Owner of the socket remains same. */
        if (chown(((struct sockaddr_un *)sa)->sun_path, -1, grp->gr_gid) == -1) {
            anetSetError(err, "chown error for group '%s':%s", group, strerror(errno));
            close(s);
            return ANET_ERR;
        }
    }

    if (listen(s, backlog) == -1) {
        anetSetError(err, "listen: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    return ANET_OK;
}

static int _anetTcpServer(char *err, int port, char *bindaddr, int af, int backlog, int mptcp) {
    int s = -1, rv;
    char _port[6]; /* strlen("65535") */
    struct addrinfo hints, *servinfo, *p;

    snprintf(_port, sizeof(_port), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; /* No effect if bindaddr != NULL */
    if (bindaddr && !strcmp("*", bindaddr)) bindaddr = NULL;
    if (af == AF_INET6 && bindaddr && !strcmp("::*", bindaddr)) bindaddr = NULL;

    if ((rv = getaddrinfo(bindaddr, _port, &hints, &servinfo)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype, anetTcpGetProtocol(mptcp))) == -1) continue;

        if (af == AF_INET6 && anetV6Only(err, s) == ANET_ERR) goto error;
        if (anetSetReuseAddr(err, s) == ANET_ERR) goto error;
        if (anetListen(err, s, p->ai_addr, p->ai_addrlen, backlog, 0, NULL) == ANET_ERR) s = ANET_ERR;
        goto end;
    }
    if (p == NULL) {
        anetSetError(err, "unable to bind socket, errno: %d", errno);
        goto error;
    }

error:
    if (s != -1) close(s);
    s = ANET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}
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

int anetTcpServer(char *err, int port, char *bindaddr, int backlog, int mptcp) {
    return _anetTcpServer(err, port, bindaddr, AF_INET, backlog, mptcp);
}

int anetTcp6Server(char *err, int port, char *bindaddr, int backlog, int mptcp) {
    return _anetTcpServer(err, port, bindaddr, AF_INET6, backlog, mptcp);
}

/* Accpet a connection and also make sure the socket is non-blocking, and CLOEXEC.
 * returns the new socket FD, or -1 on error. */
static int anetGenericAccept(char *err, int s, struct sockaddr *sa, socklen_t *len) {
    int fd;
    do {
        /* Use the accept4() call on linux to simultaneously accept and
         * set a socket as non-blocking. */
#ifdef HAVE_ACCEPT4
        fd = accept4(s, sa, len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
        fd = accept(s, sa, len);
#endif
    } while (fd == -1 && errno == EINTR);

    if (fd == -1) {
        anetSetError(err, "accept: %s", strerror(errno));
        return ANET_ERR;
    }
#ifndef HAVE_ACCEPT4
    if (anetCloexec(fd) == -1) {
        anetSetError(err, "anetCloexec: %s", strerror(errno));
        close(fd);
        return ANET_ERR;
    }
    if (anetNonBlock(err, fd) == -1) {
        close(fd);
        return ANET_ERR;
    }
#endif
    return fd;
}

/* Accept a connection and also make sure the socket is non-blocking, and CLOEXEC.
 * returns the new socket FD, or -1 on error. */
int anetTcpAccept(char *err, int serversock, char *ip, size_t ip_len, int *port) {
    int fd;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    if ((fd = anetGenericAccept(err, serversock, (struct sockaddr *)&sa, &salen)) == ANET_ERR) return ANET_ERR;

    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
    }
}



int anetSetSockMarkId(char *err, int fd, uint32_t id) {
#ifdef HAVE_SOCKOPTMARKID
    if (setsockopt(fd, SOL_SOCKET, SOCKOPTMARKID, (void *)&id, sizeof(id)) == -1) {
        anetSetError(err, "setsockopt: %s", strerror(errno));
        return ANENT_ERR;
    }
    return ANET_OK;
#else
    UNUSED(fd);
    UNUSED(id);
    anetSetError(err, "anetSetSockMarkId unsupported on this platform");
    return ANET_ERR;
#endif
}

int anetGetSocketFlags(char *err, int fd) {
    int flags;

    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        anetSetError(err, "fcntl(F_GETFL): %s", strerror(errno));
        return ANET_ERR;
    }
    return flags;
}

int anetSetBlock(char *err, int fd, int non_block) {
    int flags = anetGetSocketFlags(err, fd);
    if (flags == ANET_ERR) {
        return ANET_ERR;
    }

    /* Check if this flag has been set or unset, if so,
     * then there is no need to call fcntl to set/unset it again. */
    if (!!(flags & O_NONBLOCK) == !!non_block) return ANET_OK;

    if (non_block)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    
    if (fcntl(fd, F_SETFL, flags) == -1) {
        anetSetError(err, "fcntl(F_SETFL, O_NONBLOCK): %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anetNonBlock(char *err, int fd) {
    return anetSetBlock(err, fd, 1);
}

int anetBlock(char *err, int fd) {
    return anetSetBlock(err, fd, 0); 
}

int anetIsBlock(char *err, int fd) {
    int flags = anetGetSocketFlags(err, fd);
    if (flags == ANET_ERR) {
        return ANET_ERR;
    }

    /* Check if the O_NONBLOCK flag is set */
    if (flags & O_NONBLOCK) {
        return 0;   /* Socket is non-blocking */
    } else {
        return 1;   /* Socket is blocking */
    }
}