/**
 * 
 */
#include "globalsettings.h"
#include "ae.h"
#include "connection.h"
#include "anet.h"

/* Error codes */
#define C_OK 0
#define C_ERR -1
#define C_RETRY -2



#define serverLog(level, ...)                           \
    do {                                                \
        if (((level) & 0xff) < LOG_VERBOSITY) break;    \
        _serverlog(level, __VA_ARGS__);                 \
    } while(0)

struct tinyHttpServer {
    char neterr[ANET_ERR_LEN];                          /* Error buffer for anet.c */
    char *bindaddr[CONFIG_BINDADDR_MAX];                /* Addresses we should bind to */
    int bindaddr_count;                                 /* The number of bind addressed. */
    int port;                                           /* TCP listening port */
    int tcp_backlog;                                    /* TCP listen() backlog */
    int mptcp;                                          /* Use Multiple path TCP */
    int socket_mark_id;                                 /* ID for listen socket marking */
    connListener listeners[CONN_TYPE_MAX];              /* TCP/Unix/TLS even more types */
    aeEventLoop *el;
};

void _serverlog(int level, const char *fmt, ...);


int listenToPort(connListener *listener);

/*------------------------------------------------------------------------------------------------
 * Extern declarations
 *-----------------------------------------------------------------------------------------------*/
extern struct tinyHttpServer server;



