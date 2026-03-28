#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "ae.h"
#include "globalsettings.h"

#include <unistd.h>

#define NET_IP_STR_LEN 46                       /* INET6_ADDRSTRLEN is 46, but we need to be sure */


typedef struct connection connection;
typedef struct connListener connListener;

typedef enum {
    CONN_TYPE_INVALID = -1,
    CONN_TYPE_SOCKET,
    CONN_TYPE_UNIX,
    CONN_TYPE_TLS,
    CONN_TYPE_MAX,
} ConnectTypeId;

typedef enum {
    CONN_STATE_NONE = 0,
    CONN_STATE_CONNECTING,
    CONN_STATE_ACCEPTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_CLOSED,
    CONN_STATE_ERROR
} ConnectionState;


typedef void ConnectionCallbackFunc(struct connection *conn);

typedef struct ConnectionType {
    /* Connection type */
    int (*get_type)(void);
    
    /* Connection type initialize & finalize & configure */
    void (*init)(void); /* auto-call during register */
    void (*cleanup)(void);
    int (*configure)(void *priv, int reconfigure);
    
    /* ae & accept & listen & error & address handler */
    void (*ae_handler)(struct aeEventLoop *el, int fd, void *clientData, int mask);
    aeFileProc *accept_handler;
    
    int (*listen)(connListener *listener);
    void (*closeListener)(connListener *listener);
    
    
} ConnectionType;

struct connection {
    ConnectionType *type;
    ConnectionState state;
    
    int last_errno;
    int fd;
    short int flags;
    short int refs;
    unsigned short int iovcnt;
    void *private_data;
    
    ConnectionCallbackFunc *conn_handler;
    ConnectionCallbackFunc *write_handler;
    ConnectionCallbackFunc *read_handler;
};

/* Setup a listener by a connection type. */
struct connListener {
    int fd[CONFIG_BINDADDR_MAX];
    int count;
    char **bindaddr;
    int bindaddr_count;
    int port;
    ConnectionType *ct;
    void *priv;             /* Used by connetion type specified data */
};

static inline const char *getConnectionTypeName(int type)
{
    switch (type) {
    case CONN_TYPE_SOCKET:
        return "tcp";
    case CONN_TYPE_UNIX:
        return "unix";
    case CONN_TYPE_TLS:
        return "tls";
    default:
        return "invalid type";
    }
}

/* Listen on an initialized listener */
static inline int connListen(connListener *listener) {
    return listener->ct->listen(listener);
}

/* Close on an initialized listener */
static inline void connCloseListener(connListener *listener) {
    if (listener->count) {
        listener->ct->closeListener(listener);
    }
}

/* Initialize the connection framework */
int connTypeInitialize(void);

/* Register a connection type into the connection framework */
int connTypeRegister(ConnectionType *ct);

/* Lookup a connection type by type name */
ConnectionType *connectionByType(int type);

/* Fast path to get TCP connection type */
ConnectionType *connectionTypeTcp(void);

/* Fast path to get TLS connection type */
ConnectionType *connectionTypeTls(void);

/* Fast path to get Unix connection type */
ConnectionType *connectionTypeUnix(void);


int TinyHttpdRegisterConnectionTypeSocket(void);
int TinyHttpdRegisterConnectionTypeTLS(void);

#endif