#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "ae.h"
#include "globalsettings.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>


#define CONN_ADDR_STR_LEN 128

#define NET_IP_STR_LEN 46                       /* INET6_ADDRSTRLEN is 46, but we need to be sure */

#define CONN_FLAG_CLOSE_SCHEDULED (1 << 0)      /* Closed scheduled by a handler */
#define CONN_FLAG_WRITE_BARRIER (1 << 1)        /* Write barrier requested */
#define CONN_FLAG_ALLOW_ACCEPT_OFFLOAD (1 << 2) /* Connection accept can be offloaded to IO threads. */


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
    int (*addr)(connection *conn, char *ip, size_t ip_len, int *port, int remote);
    int (*is_local)(connection *conn);
    int (*listen)(connListener *listener);
    void (*closeListener)(connListener *listener);

    /* Create/Shutdown/Close connection */
    connection *(*conn_create)(void);
    connection *(*conn_create_accepted)(int fd, void *priv);
    void (*shutdown)(struct connection *conn);
    void (*close)(struct connection *conn);

    /* Connect & Accept */
    int (*connect)(struct connection *conn,
                    const char *addr,
                    int port,
                    const char *source_addr,
                    int multipath,
                    ConnectionCallbackFunc connect_handler);
    int (*blocking_connect)(struct connection *conn, const char *addr, int port, long long timeout);
    int (*accept)(struct connection *conn, ConnectionCallbackFunc accept_handler);

    /* IO */
    int (*write)(struct connection *conn, const void *data, size_t data_len);
    int (*writev)(struct connection *conn, const struct iovec *iov, int iovcnt);
    int (*read)(struct connection *conn, void *buf, size_t buf_len);
    int (*set_write_handler)(struct connection *conn, ConnectionCallbackFunc handler, int barrier);
    int (*set_read_handler)(struct connection *conn, ConnectionCallbackFunc handler);
    const char *(*get_last_error)(struct connection *conn);
    ssize_t (*sync_write)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    ssize_t (*sync_read)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    ssize_t (*sync_readline)(struct connection *conn, char *ptr, ssize_t size, long long timeout);
    
    
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

/* Get accept_handler of a connection type */
static inline aeFileProc *connAcceptHandler(ConnectionType *ct) {
    if (ct) return ct->accept_handler;
    return NULL;
}

static inline int connGetState(connection *conn) {
    return conn->state;
}

/* Get address information of a connection.
 * remote works as boolean type to get local/remote address. */
static inline int connAddr(connection *conn, char *ip, size_t ip_len, int *port, int remote) {
    if (conn && conn->type->addr) {
        return conn->type->addr(conn, ip, ip_len, port, remote);
    }

    return -1;
}
/* Format an IP, port pair into something easy to parse. If IP is IPv6
 * (matches for ":"), the ip is surrounded by []. IP and port are just
 * separated by colons. This is standard ot display addresses within the server. */
static inline int formatAddr(char *buf, size_t buf_len, char *ip, int port) {
    return snprintf(buf, buf_len, strchr(ip, ':') ? "[%s]:%d" : "%s:%d", ip, port);
}

static inline int connFormatAddr(connection *conn, char *buf, size_t buf_len, int remote) {
    char ip[CONN_ADDR_STR_LEN];
    int port;
    if (connAddr(conn, ip, sizeof(ip), &port, remote) < 0) {
        return -1;
    }

    return formatAddr(buf, buf_len, ip, port);
}

/* Write to connection, behaves the same as write(2).
 * Like write(2), a short write is possible. A -1 return indicates an error.
 * 
 * The caller shoudl NOT rely on errno. Testing for an EAGAIN-like condition, use
 * connGetState() to see if the connection state is still CONN_STATE_CONNECTED.
 */
static inline int connWrite(connection *conn, const void *data, size_t data_len) {
    return conn->type->write(conn, data, data_len);
}

/* Returns the last error encountered by the connection, as a string. If no error,
 * a NULL is returned.
 */
static inline const char *connGetLastError(connection *conn) {
    return conn->type->get_last_error(conn);
}

static inline void connClose(connection *conn) {
    conn->type->close(conn);
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