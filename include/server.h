/**
 * 
 */
#include "globalsettings.h"



#define serverLog(level, ...)                           \
    do {                                                \
        if (((level) & 0xff) < LOG_VERBOSITY) break;    \
        _serverlog(level, __VA_ARGS__);                 \
    }while(0)

struct tinyHttpServer {
    char bindaddr[CONFIG_BINDADDR_MAX + 1];                /* Address we should bind to */
    int port;                                           /* TCP listening port */
};

void _serverlog(int level, const char *fmt, ...);




