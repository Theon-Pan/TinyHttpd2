#ifndef __GLOBAL_SETTINGS_H
#define __GLOBAL_SETTINGS_H

#define _POSIX_C_SOURCE 200809L

#ifdef __linux__
#define HAVE_EPOLL 1
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

#define CONFIG_BINDADDR_MAX 16                  /* The max chars of the bind address */
#define CONFIG_DEFAULT_PORT 8086                /* The default TCP port */

#endif