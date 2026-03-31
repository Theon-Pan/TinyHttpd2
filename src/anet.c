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
#include <netinet/tcp.h>


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

static int anetSetTcpDelay(char *err, int fd, int val) {
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) == -1) {
        anetSetError(err, "setsockopt TCP_NODELAY: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anetEnableTcpDelay(char *err, int fd) {
    return anetSetTcpDelay(err, fd, 1);
}

int anetDisableTcpDelay(char *err, int fd) {
    return anetSetTcpDelay(err, fd, 0);
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

/* Enable TCP keep-alive mechanism to detect dead peers,
 * TCP_KEEPIDLE, TCP_KEEPINTVL and tCP_KEEPCNT will be set accordingly. */
int anetKeepAlive(char *err, int fd, int interval) {
    int enabled = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled)) == -1) {
        anetSetError(err, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
        return ANET_ERR;
    }

    int idle;
    int intvl;
    int cnt;

    /* There are platforms that are expected to support the full mechanism of TCP keep-alive, 
     * we want the compiler to emit warnings of unused variables if the preprocessor derectives
     * somehow fail, and other than those platforms, just omit these warnings if the happen.
     */
#if !(defined(_AIX) || defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__illumos__) || \
        defined(__linux__) || defined(__NetBSD__) || defined(__sun))
    UNUSED(interval);
    UNUSED(idle);
    UNUSED(intvl);
    UNUSED(cnt);
#endif

#ifdef __sun
    /* The implementation of TCP keep-alive on Solaris/SmartOS is a bit unusual
     * compared to other Unix-like systems.
     * Thus, we need to specialize it on Solaris.
     * 
     * There are two keep-alive mechanisms on Solaris:
     * - By default, the first keep-alive probe is sent out after a TCP connection is idle for twon hours.
     * If the peer does not respond to the probe within eight minutes, the TCP connection is aborted.
     * You can alter the interval for sending out the first probe using the socket option TCP_KEEPALIVE_THRESHOLD
     * in milliseconds or TCP_KEEPIDLE in seconds.
     * The system default is controller by the TCP ndd parameter tcp_keepalive_interval. The minimum value is ten 
     * seconds. The maximum is ten days, while the default is two hours. If you receive no reponse to the probe, you
     * can use the TCP_KEEPALIVE_ABORT_THRESHOLD socket option to change the time threshold for aborting a TCP
     * connection. The option value is an unsigned integer in milliseconds. The value zero indicates that TCP should
     * never time tou and abort the connection when probing. The system default is controlled by the TCP ndd parameter
     * tcp_keepalive_abort_interval. The default is eight minutes.
     * 
     * - The second implemetation is activated if socket option TCP_KEEPINTVL and/or TCP_KEEPCNT are set.
     * The time between each consequent probes is set by TCP_KEEPINTVL in seconds.
     * THe minimum value is ten seconds. The maximum is ten days, while the default is two hours.
     * The TCP connection will be aborted after certain amount of probes, which is set by TCP_KEEPCNT, without receiving
     * response.
     * */

    idle = interval;
    if (idle < 10) idle = 10;                               // Kernel expects at least 10 seconds
    if (idle > 10 * 24 * 60 * 60) idle = 10 * 24 * 60 * 60  // Kernel expects at most 10 days

    /* `TCP_KEEPIDLE`, `TCP_KEEPINTVL`, and `TCP_KEEPCNT` were not available on Solaris
     * until version 11.4, but let's take a chance here. */
#if defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL) && define(TCP_KEEPCNT)
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));
        return ANET_ERR;
    }

    intvl = idle / 3;
    if (intvl < 10) intvl = 10; /* Kernel expects at least 10 seconds */
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPINTVL: %s\n", strerror(errno));
        return ANET_ERR;
    }

    cnt = 3;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPCNT: %s\n", strerror(errno));
        return ANET_ERR;
    }
#else
    /* Fall back to the first implementation of tcp-alive mechanism for older Solaris,
     * simulate the tcp-alive mechanism on other platforms via `TCP_KEEPALIVE_THRESHOLD` +
     * `TCP_KEEPALIVE_ABORT_THRESHOLD`.
     * */
    idle *= 1000;   // Kernal expects milliseconds
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE_THRESHOLD, &idle, sizeof(idle)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPALIVE_THESHOLD: %s\n", strerror(errno));
        return ANET_ERR;
    }

    /* Note that the consequent probes will not be sent at equal intervals on Solaris,
     * but will be sent using the exponential backoff algorithm. */
    int time_to_abort = idle;
    id (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE_ABORT_THRESHOLD, &time_to_abort, sizeof(time_to_abort)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPALIVE_ABORT_THRESHOLD: %s\n", strerror(errno));
        return ANET_ERR;
    }
#endif
    return ANET_OK;
#endif

#ifdef TCP_KEEPIDLE
    /* Default settings are more or less garbage, with the keepalive time
     * set to 7200 by default on Linux and other Unix-like systems.
     * Modify settings to make the feature actually useful. */

     /* Send first probe after interval. */
     idle = interval;
     if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));
        return ANET_ERR;
     }
#elif defined(TCP_KEEPALIVE)
     /* Darwin/macOS uses TCP_KEEPALIVE in place of TCP_KEEPIDLE. */
     idle = interval;
     if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPALIVE: %s\n", strerror(errno));
        return ANET_ERR;
     }
#endif

#ifdef TCP_KEEPINTVL
     /* Send next probes after the specified interval. Note that we set the 
      * delay as interval / 3, as we send three probes before detecting
      * an error(see the next setsockopt call). */
     intvl = interval / 3;
     if (intvl == 0) intvl = 1;
     if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPINTVL: %s\n", strerror(errno));
        return ANET_ERR;
     }
#endif

#ifdef TCP_KEEPCNT
     /* Consider the socket in error state after three we send three ACK
      * probes without gettting a reply. */
     cnt = 3;
     if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)) == -1) {
        anetSetError(err, "setsockopt TCP_KEEPCNT: %s\n", strerror(errno));
        return ANET_ERR;
     }
#endif

     return ANET_OK;
}

int anetTcpServer(char *err, int port, char *bindaddr, int backlog, int mptcp) {
    return _anetTcpServer(err, port, bindaddr, AF_INET, backlog, mptcp);
}

int anetTcp6Server(char *err, int port, char *bindaddr, int backlog, int mptcp) {
    return _anetTcpServer(err, port, bindaddr, AF_INET6, backlog, mptcp);
}

/* For some error cases indicates transient errors and accept can be retried 
 * in order to serve other pending connections. This function shoudl be called with the last errno,
 * right after anetTcpAccept or anetUnixAccept returned an error in order to retry them. */
int anetRetryAcceptOnError(int err) {
    /* This is a trasient error which can happen, for example, when 
     * a client initiates a TCP handshake (SYN),
     * the server receives and queues it in the pending connections queue (the SYN queue),
     * but before accept() is called, the connection is aborded.
     * In such cases we can continue accepting other connections. */
    if (err == ECONNABORTED) {
        return 1;
    }

#if defined(__linux__)
    /* https://www.man7.org/linux/man-pages/man2/accept4.2 suggests that:
     * Linux accept() (and accept4()) passes already-pending network
       errors on the new socket as an error code from accept(), This 
       behavior differs from other BSD socket implementations. For 
       reliable operation, the application should detect the network
       errors defined for the protocol after accept() and treat them like
       EAGAIN by retrying. In the case of TCP/IP, these are ENETDOWN, 
       EPROTO, ENOPROTOOPT, EHOSTDOWN, ENONET, EHOSTUNREACH, EOPNOTSUPP,
       and ENETUNREACH. */
       if (err == ENETDOWN || err == EPROTO || err == ENOPROTOOPT ||
            err == EHOSTDOWN || err == ENONET || err == EHOSTUNREACH ||
            err == EOPNOTSUPP || err == ENETUNREACH) {
                return 1;
        }
#endif
    
    return 0;
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
        if (ip) inet_ntop(sa.ss_family, (void *)&(s->sin_addr), ip, ip_len);
        if (port) *port = ntohs(s->sin_port);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(sa.ss_family, (void *)&(s->sin6_addr), ip, ip_len);
        if (port) *port = ntohs(s->sin6_port);
    }

    /* Enable TCP_NODELAY by default */
    anetEnableTcpDelay(err, fd);

    return fd;
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