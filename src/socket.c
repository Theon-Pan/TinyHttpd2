#include "server.h"
#include "connhelpers.h"



static ConnectionType CT_Socket;

static int connSocketGetType(void) {
    return CONN_TYPE_SOCKET;
}

static int connSocketListen(connListener *listener) {
    return listenToPort(listener);
}

/* When a connection is created we must know its type already, but the 
 * underlying socket may or may not exist:
 * */
static connection *connCreateSocket(void) {
    connection *conn = malloc(sizeof(connection));
    if (conn == NULL) return NULL;
    memset(conn, 0, sizeof(connection));
    conn->type = &CT_Socket;
    conn->fd = -1;
    conn->iovcnt = IOV_MAX;

    return conn;
}

/* Create a new socket-type connection that is already associated with
 * an accepted connection. 
 * 
 * The socket is not ready for I/O until connAccept() was called and
 * invoked the connection-level accpet handler.
 * 
 * Callers should use connGetState() and verify the created connection
 * is not in an error state (which is not possible for a socket connection,
 * but could but possible with other protocols).
 * */
static connection *connCreateAcceptedSocket(int fd, void *priv) {
    UNUSED(priv);
    connection *conn = connCreateSocket();
    if (conn == NULL) return NULL;
    conn->fd = fd;
    conn->state = CONN_STATE_ACCEPTING;
    return conn;
}

static void connSocketAcceptHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd;
    int max = server.max_new_conns_per_cycle;
    char cip[NET_IP_STR_LEN];
    UNUSED(el);
    UNUSED(mask);
    UNUSED(privdata);

    while(max--) {
        cfd = anetTcpAccept(server.neterr, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (anetRetryAcceptOnError(errno)) continue;
            if (errno != EWOULDBLOCK) serverLog(LL_WARNING|LL_RAW, "Accepting client connection: %s", server.neterr);
            return;
        }
        serverLog(LL_VERBOSE|LL_RAW, "Accepted %s:%d", cip, cport);
    }
    if (server.tcpkeepalive) anetKeepAlive(NULL, cfd, server.tcpkeepalive);
    acceptCommonHandler(connCreateAcceptedSocket(cfd, NULL), cip);
}

static int connSocketWrite(connection *conn, const void *data, size_t data_len) {
    int ret = write(conn->fd, data, data_len);
    if (ret < 0 && errno != EAGAIN) {
        conn->last_errno = errno;

        /* Don't overwrite the state of a connection that is already
         * connected, not to mess with handler callbacks. */
        if (errno != EINTR && conn->state == CONN_STATE_CONNECTED) conn->state = CONN_STATE_ERROR;
    }

    return ret;
}

static int connSocketAddr(connection *conn, char *ip, size_t ip_len, int *port, int remote) {
    if (anetFdToString(conn->fd, ip, ip_len, port, remote) == 0) return 0;

    conn->last_errno = errno;
    return -1;
}

static const char *connSocketGetLastError(connection *conn) {
    return strerror(conn->last_errno);
}


static void connSocketCloseListener(connListener *listener) {
    int j;

    for (j = 0; j < listener->count; j++) {
        if (listener->fd[j] == -1) continue;

        aeDeleteFileEvent(server.el, listener->fd[j], AE_READABLE);
        close(listener->fd[j]);
    }
    listener->count = 0;
}


/* Close the connection and free resources. */
static void connSocketClose(connection *conn) {
    if (conn->fd != -1) {
        aeDeleteFileEvent(server.el, conn->fd, AE_READABLE | AE_WRITABLE);
        close(conn->fd);
        conn->fd = -1;
    }

    /* If called from within a handler, schedule the close but
     * keep the connection until the handler returns. 
     */
    if (connHasRefs(conn)) {
        conn->flags |= CONN_FLAG_CLOSE_SCHEDULED;
        return;
    }

    free(conn);
}


static ConnectionType CT_Socket = {
    /* Connection Type */
    .get_type = connSocketGetType,

    /* Connection Type initialize & finalize & configure */
    .init = NULL,
    .cleanup = NULL,
    .configure = NULL,

    /* ae & accept & listen & error &address handler */
    .accept_handler = connSocketAcceptHandler,
    .listen = connSocketListen,
    .closeListener = connSocketCloseListener, 
    .addr = connSocketAddr,

    /* Create/Shutdown/Close connection */
    .close = connSocketClose,

    /* IO */
    .get_last_error = connSocketGetLastError,
    .write = connSocketWrite,

};

int TinyHttpdRegisterConnectionTypeSocket(void) {
    return connTypeRegister(&CT_Socket);
}