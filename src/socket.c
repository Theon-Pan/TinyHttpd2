#include "server.h"


static ConnectionType CT_Socket;

static int connSocketGetType(void) {
    return CONN_TYPE_SOCKET;
}

static int connSocketListen(connListener *listener) {
    return listenToPort(listener);
}

static void connSocketAcceptHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd;
    int max = server.max_new_conns_per_cycle;
    char cip[NET_IP_STR_LEN];
    UNUSED(el);
    UNUSED(mask);
    UNUSED(privdata);
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

static ConnectionType CT_Socket = {
    /* Connection Type */
    .get_type = connSocketGetType,

    /* Connection Type initialize & finalize & configure */
    .init = NULL,
    .cleanup = NULL,
    .configure = NULL,

    /* ae & accept & listen & error &address handler */
    .listen = connSocketListen,
    .closeListener = connSocketCloseListener, 

};

int TinyHttpdRegisterConnectionTypeSocket(void) {
    return connTypeRegister(&CT_Socket);
}