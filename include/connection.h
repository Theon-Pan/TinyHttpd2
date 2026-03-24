#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "ae.h"

typedef struct connection connection;

typedef enum {
    CONN_TYPE_INVALID = -1,
    CONN_TYPE_SOCKET,
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

static inline const char *getConnectionTypeName(int type)
{
    switch (type) {
    case CONN_TYPE_SOCKET:
        return "tcp";
    case CONN_TYPE_TLS:
        return "tls";
    default:
        return "invalid type";
    }
}

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

int TinyHttpdRegisterConnectionTypeSocket(void);
int TinyHttpdRegisterConnectionTypeTLS(void);

#endif