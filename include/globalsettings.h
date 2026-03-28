#ifndef __GLOBAL_SETTINGS_H
#define __GLOBAL_SETTINGS_H

#define _GNU_SOURCE
#define UNUSED(x) ((void)x)
#define _POSIX_C_SOURCE 200809L

#ifdef __linux__
#define HAVE_EPOLL 1
#endif

/* Test for accept4 */
#if defined(__linux__) || defined(__FreeBSD__) || defined(OpenBSD5_7) || \
    (defined(_DragonFly__) && __DragonFly_version >= 400305) ||          \
    (defined(__NetBSD__) && (defined(NetBSD8_0) || __NetBSD_Version__ >= 800000000))
#define HAVE_ACCEPT4
#endif

/* MSG_NOSIGNAL */
#if defined(__linux__)
#define HAVE_MSG_NOSIGNAL 1

/* SOCKOPTMARKID*/
#if defined(SO_MARK)
#define HAVE_SOCKOPTMARKID 1
#define SOCKOPTMARKID SO_MARK
#endif
#endif

#if defined(__FreeBSD__)
#if defined(SO_USER_COOKIE)
#define HAVE_SOCKOPTMARKID 1
#define SOCKOPTMARKID SO_USER_COOKIE
#endif
#endif

#if defined(__OpenBSD)
#if defined(SO_RTABLE)
#define HAVE_SOCKOPTMARKID 1
#define SOCKOPTMARKID SO_RTABLE
#endif
#endif

/* Log levels */
#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3
#define LL_NOTHING 4
#define LL_RAW (1 << 10)                        /* Modifier to log without timestamp */
#define LOG_VERBOSITY LL_DEBUG
#define LOG_MAX_LEN 1024

#define CONFIG_BINDADDR_MAX 16                  /* The max number of the bind addresses */
#define CONFIG_DEFAULT_PORT 8086                /* The default TCP port */
#define CONFIG_DEFAULT_BINDADDR_COUNT 2
#define CONFIG_DEFAULT_BINDADDR {"*", "-::*"}

#endif