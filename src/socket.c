#include "server.h"

static int connSocketGetType(void) {
    return CONN_TYPE_SOCKET;
}

static ConnectionType CT_Socket = {
    /* Connection Type */
    .get_type = connSocketGetType,

    /* Connection Type initialize & finalize & configure */
    .init = NULL,
    .cleanup = NULL,
    .configure = NULL,

    /* ae & accept & listen & error &address handler */

};